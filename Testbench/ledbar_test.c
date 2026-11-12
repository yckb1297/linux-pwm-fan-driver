/*
 * ledbar_test.c - LED 바 드라이버 테스트/시연 앱
 *
 * 빌드: gcc -O2 -Wall -o ledbar_test ledbar_test.c -lm   (또는 make test)
 *
 * 사용법:
 *   ./ledbar_test level 5          아래부터 5칸
 *   ./ledbar_test pattern 0xA5     비트마스크로 직접
 *   ./ledbar_test blink [ms]       깜빡임
 *   ./ledbar_test wave [ms]        1→8→0 웨이브 (전원 ON 효과)
 *   ./ledbar_test progress [ms]    3초 길게 누르기 진행 표시
 *   ./ledbar_test stop             애니메이션 정지
 *   ./ledbar_test state            현재 상태 출력
 *   ./ledbar_test temp 26.4        온도 → 칸 수 변환 결과 표시
 *   ./ledbar_test tempsim          온도 22→31→22 시뮬레이션 (히스테리시스 확인)
 *   ./ledbar_test demo             전체 기능 시연
 *   ./ledbar_test off              전부 끄기
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>

#include "ledbar_ioctl.h"

static int fd = -1;

static const char *anim_name(int a)
{
	static const char *names[] = { "none", "blink", "wave", "progress" };

	return (a >= 0 && a < 4) ? names[a] : "?";
}

static void xioctl(unsigned long req, void *arg, const char *what)
{
	if (ioctl(fd, req, arg) < 0) {
		perror(what);
		exit(1);
	}
}

static void set_level(int level)
{
	__s32 v = level;

	xioctl(LEDBAR_SET_LEVEL, &v, "LEDBAR_SET_LEVEL");
}

static void set_pattern(unsigned int pat)
{
	__u8 v = pat;

	xioctl(LEDBAR_SET_PATTERN, &v, "LEDBAR_SET_PATTERN");
}

static void set_anim(int anim, unsigned int period_ms)
{
	struct ledbar_anim a = { .anim = anim, .period_ms = period_ms };

	xioctl(LEDBAR_SET_ANIM, &a, "LEDBAR_SET_ANIM");
}

static void print_state(void)
{
	struct ledbar_state st;
	int i;

	xioctl(LEDBAR_GET_STATE, &st, "LEDBAR_GET_STATE");
	printf("level=%d pattern=0x%02x shown=0x%02x anim=%s period=%ums nleds=%u\n",
	       st.level, st.pattern, st.shown, anim_name(st.anim),
	       st.period_ms, st.nleds);

	/* 위(LED8)부터 그림으로 */
	for (i = st.nleds - 1; i >= 0; i--)
		printf("  LED%d %s\n", i + 1, (st.shown >> i) & 1 ? "■" : "□");
}

static void msleep(int ms)
{
	usleep(ms * 1000);
}

/*
 * 오토모드 온도 → 칸 수 (메인 앱 담당자가 그대로 가져다 쓰면 됨)
 *   ~23.9°C: 1칸, 24: 2칸, 25: 3칸 ... 29: 7칸, 30°C~: 8칸
 *   경계에서 깜빡이지 않게 ±HYST 히스테리시스
 *   prev: 직전 칸 수 (처음엔 0)
 */
#define TEMP_BASE	22.0
#define HYST		0.3

static int temp_to_level(double t, int prev)
{
	int lv = (int)floor(t - TEMP_BASE);

	if (lv < 1)
		lv = 1;
	if (lv > LEDBAR_MAX_LEDS)
		lv = LEDBAR_MAX_LEDS;

	if (prev >= 1 && prev <= LEDBAR_MAX_LEDS && lv != prev) {
		double lower = TEMP_BASE + prev;	/* prev칸 시작 온도 */
		double upper = TEMP_BASE + prev + 1;	/* prev+1칸 시작 온도 */

		if (t >= lower - HYST && t < upper + HYST)
			return prev;			/* 경계 근처면 유지 */
	}
	return lv;
}

static void tempsim(void)
{
	int level = 0, i, n = 0;
	double temps[200];
	double t;

	/* 22 → 31 → 22 로 올렸다 내리고, 센서 노이즈 흉내 */
	for (t = 22.0; t <= 31.0; t += 0.1)
		temps[n++] = t;
	for (t = 31.0; t >= 22.0 && n < 200; t -= 0.1)
		temps[n++] = t;

	for (i = 0; i < n; i++) {
		double noisy = temps[i] + 0.25 * sin(i * 1.7);
		int lv = temp_to_level(noisy, level);

		if (lv != level) {
			printf("T=%5.2f°C → %d칸\n", noisy, lv);
			set_level(lv);
			level = lv;
		}
		msleep(80);
	}
}

static void demo(void)
{
	int i;

	puts("[1] 전원 ON 웨이브");
	set_anim(LEDBAR_ANIM_WAVE, 0);
	msleep(1300);

	puts("[2] 매뉴얼 풍량 1→8단");
	for (i = 1; i <= 8; i++) {
		set_level(i);
		msleep(300);
	}
	for (i = 7; i >= 1; i--) {
		set_level(i);
		msleep(150);
	}

	puts("[3] 패턴 0xA5 / 0x5A");
	for (i = 0; i < 4; i++) {
		set_pattern(i & 1 ? 0x5A : 0xA5);
		msleep(300);
	}

	puts("[4] 레벨 4 깜빡임 (센서 오류 표시 예)");
	set_level(4);
	set_anim(LEDBAR_ANIM_BLINK, 250);
	msleep(2000);
	set_anim(LEDBAR_ANIM_NONE, 0);

	puts("[5] 3초 길게 누르기 진행 표시");
	set_anim(LEDBAR_ANIM_PROGRESS, 0);
	msleep(3500);

	puts("[6] 오토모드 온도 시뮬레이션");
	tempsim();

	puts("[7] 전원 OFF");
	set_level(0);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"사용법: %s <명령> [값]\n"
		"  level N | pattern 0xHH | blink [ms] | wave [ms] | progress [ms]\n"
		"  stop | state | temp T | tempsim | demo | off\n", prog);
	exit(2);
}

int main(int argc, char **argv)
{
	const char *cmd;
	unsigned int ms;

	if (argc < 2)
		usage(argv[0]);
	cmd = argv[1];
	ms = argc > 2 ? strtoul(argv[2], NULL, 0) : 0;

	if (!strcmp(cmd, "temp")) {	/* 장치 없이도 변환만 확인 가능 */
		if (argc < 3)
			usage(argv[0]);
		printf("%.1f°C → %d칸\n", atof(argv[2]),
		       temp_to_level(atof(argv[2]), 0));
		return 0;
	}

	fd = open(LEDBAR_DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror(LEDBAR_DEV_PATH);
		return 1;
	}

	if (!strcmp(cmd, "level") && argc > 2)
		set_level(atoi(argv[2]));
	else if (!strcmp(cmd, "pattern") && argc > 2)
		set_pattern(strtoul(argv[2], NULL, 0));
	else if (!strcmp(cmd, "blink"))
		set_anim(LEDBAR_ANIM_BLINK, ms);
	else if (!strcmp(cmd, "wave"))
		set_anim(LEDBAR_ANIM_WAVE, ms);
	else if (!strcmp(cmd, "progress"))
		set_anim(LEDBAR_ANIM_PROGRESS, ms);
	else if (!strcmp(cmd, "stop"))
		set_anim(LEDBAR_ANIM_NONE, 0);
	else if (!strcmp(cmd, "off")) {
		set_anim(LEDBAR_ANIM_NONE, 0);
		set_level(0);
	} else if (!strcmp(cmd, "tempsim"))
		tempsim();
	else if (!strcmp(cmd, "demo"))
		demo();
	else if (strcmp(cmd, "state"))
		usage(argv[0]);

	print_state();
	close(fd);
	return 0;
}

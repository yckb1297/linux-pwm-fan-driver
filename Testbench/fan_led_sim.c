/*
 * fan_led_sim.c - 팀원 코드 없이 ledbar_ctrl 동작을 확인하는 시뮬레이터
 *
 * 전역변수를 여기서 정의하고, 다른 팀원 코드가 하는 것처럼 값을 바꿔가며
 * ledbar_ctrl_update() 를 호출한다.
 *
 * 빌드: make sim
 * 실행: ./fan_led_sim
 */
#include <stdio.h>
#include <unistd.h>

#include "fan_globals.h"
#include "ledbar_ctrl.h"

/* 실제 프로젝트에서는 메인 앱이 정의하는 변수들 */
float		TEMP = 20.0f;
int		CW_CNT;
int		CCW_CNT;
int		PWM_DUTY;
int		FAN_LEVEL = 1;
enum fan_mode	fan_mode = WAIT;

/* 엔코더 담당 코드 흉내: 다이얼 한 칸에 단수 ±1, 1~8 제한 */
static void dial(int dir)
{
	if (dir > 0) {
		CW_CNT++;
		if (FAN_LEVEL < 8)
			FAN_LEVEL++;
	} else {
		CCW_CNT++;
		if (FAN_LEVEL > 1)
			FAN_LEVEL--;
	}
}

/* ms 동안 20ms 주기로 update 호출 (메인 루프 흉내) */
static void run(int ms)
{
	for (; ms > 0; ms -= 20) {
		ledbar_ctrl_update();
		usleep(20 * 1000);
	}
}

static void report(const char *what)
{
	static int last = -2;

	if (ledbar_ctrl_level() != last) {
		printf("%-24s → %d칸\n", what, ledbar_ctrl_level());
		last = ledbar_ctrl_level();
	}
}

int main(void)
{
	char buf[64];
	float t;
	int i;

	if (ledbar_ctrl_init() < 0)
		return 1;

	puts("== WAIT (전부 꺼짐) ==");
	fan_mode = WAIT;
	run(1000);
	report("WAIT");

	puts("== AUTO: 온도 19.5 → 29.0 → 19.5 ==");
	fan_mode = AUTO;
	run(1300);		/* 켜질 때 웨이브 */
	for (t = 19.5f; t <= 29.0f; t += 0.1f) {
		TEMP = t;
		run(60);
		snprintf(buf, sizeof(buf), "AUTO TEMP=%.1f", t);
		report(buf);
	}
	for (t = 29.0f; t >= 19.5f; t -= 0.1f) {
		TEMP = t;
		run(60);
		snprintf(buf, sizeof(buf), "AUTO TEMP=%.1f", t);
		report(buf);
	}

	puts("== MANUAL: 시계방향 10번 → 반시계방향 10번 ==");
	fan_mode = MANUAL;
	for (i = 0; i < 10; i++) {
		dial(+1);
		run(300);
		snprintf(buf, sizeof(buf), "MANUAL FAN_LEVEL=%d", FAN_LEVEL);
		report(buf);
	}
	for (i = 0; i < 10; i++) {
		dial(-1);
		run(300);
		snprintf(buf, sizeof(buf), "MANUAL FAN_LEVEL=%d", FAN_LEVEL);
		report(buf);
	}

	puts("== 다시 WAIT ==");
	fan_mode = WAIT;
	run(500);
	report("WAIT");

	ledbar_ctrl_close();
	return 0;
}

/*
 * fan_globals.h - 선풍기 팀 공용 전역변수 선언
 *
 * 실제 변수 정의(메모리 할당)는 메인 앱 .c 파일 "한 곳"에서만 한다. 예)
 *     float TEMP;
 *     int   CW_CNT, CCW_CNT, PWM_DUTY;
 *     int   FAN_LEVEL = 1;
 *     enum fan_mode fan_mode = WAIT;
 *
 * 자료형은 팀에서 확정한 것과 반드시 같게 맞출 것 (다르면 컴파일 에러/오동작).
 */
#ifndef FAN_GLOBALS_H
#define FAN_GLOBALS_H

enum fan_mode {
	WAIT = 0,	/* 전원 OFF(대기) */
	AUTO,		/* 온도에 따라 자동 */
	MANUAL		/* 다이얼로 수동 */
};

extern float		TEMP;		/* 현재 온도 (°C, BMP180) */
extern int		CW_CNT;		/* 다이얼 시계방향 누적 회전수 */
extern int		CCW_CNT;	/* 다이얼 반시계방향 누적 회전수 */
extern int		PWM_DUTY;	/* 모터 PWM duty (%) 0~100 */
extern int		FAN_LEVEL;	/* MANUAL 풍량 단수 1~8 (엔코더/모터 담당이 계산해서 저장) */
extern enum fan_mode	fan_mode;	/* 현재 모드 */

#endif /* FAN_GLOBALS_H */

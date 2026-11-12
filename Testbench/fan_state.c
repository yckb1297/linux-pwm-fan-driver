#include "fan_state.h"

float TEMP;
int DIAL_CNT;
int PWM_DUTY;
enum fan_mode fan_mode = WAIT;

static const struct fan_ops *ops;
static int duty_step = 5;
static int dial_max = DUTY_MAX / 5;
static enum fan_mode saved_mode = MANUAL;	/* mode restored by a click in WAIT */

/*
 * TODO: placeholder, the AUTO temperature steps are not decided yet.
 * The first entry whose temp is <= TEMP is used, below the last one the fan is off.
 */
static const struct {
	int temp;
	int duty;
} auto_table[] = {
	{ 30, 100 },
	{ 28,  70 },
	{ 26,  40 },
	{ 24,  20 },
};

static int auto_duty(float temp)
{
	unsigned int i;

	for (i = 0; i < sizeof(auto_table) / sizeof(auto_table[0]); i++) {
		if (temp >= auto_table[i].temp)
			return auto_table[i].duty;
	}

	return 0;
}

static int clamp_int(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/* output side, call after the mode or an input changed */
static void apply(void)
{
	int duty;

	switch (fan_mode) {
	case MANUAL:
		duty = DIAL_CNT * duty_step;
		break;
	case AUTO:
		duty = auto_duty(TEMP);
		break;
	default:
		duty = 0;
		break;
	}
	duty = clamp_int(duty, 0, DUTY_MAX);

	if (ops && ops->set_led)
		ops->set_led(fan_mode == AUTO);

	if (duty != PWM_DUTY) {
		PWM_DUTY = duty;
		if (ops && ops->set_pwm)
			ops->set_pwm(PWM_DUTY);
		if (ops && ops->on_duty)
			ops->on_duty(PWM_DUTY);
	}
}

static void change_mode(enum fan_mode to)
{
	enum fan_mode from = fan_mode;

	if (to == from)
		return;

	if (to == WAIT)
		saved_mode = from;		/* AUTO or MANUAL */

	/* take over the current output so the duty does not jump */
	if (to == MANUAL && from == AUTO)
		DIAL_CNT = clamp_int((PWM_DUTY + duty_step / 2) / duty_step, 0, dial_max);

	fan_mode = to;
	if (ops && ops->on_mode)
		ops->on_mode(from, to);
	apply();
}

void fan_init(const struct fan_ops *o, int step)
{
	ops = o;
	duty_step = (step >= 1 && step <= DUTY_MAX) ? step : 5;
	dial_max = DUTY_MAX / duty_step;
	DIAL_CNT = 0;
	PWM_DUTY = 0;
	fan_mode = WAIT;
	saved_mode = MANUAL;

	if (ops && ops->set_led)
		ops->set_led(0);
	if (ops && ops->set_pwm)
		ops->set_pwm(0);
}

void fan_on_rotate(int delta)
{
	if (fan_mode != MANUAL)
		return;

	DIAL_CNT = clamp_int(DIAL_CNT + delta, 0, dial_max);
	apply();
}

void fan_on_click(void)
{
	switch (fan_mode) {
	case WAIT:
		change_mode(saved_mode);
		break;
	case MANUAL:
		change_mode(AUTO);
		break;
	case AUTO:
		change_mode(MANUAL);
		break;
	}
}

void fan_on_long_press(void)
{
	if (fan_mode != WAIT)
		change_mode(WAIT);
}

void fan_set_temp(float temp)
{
	TEMP = temp;
	if (fan_mode == AUTO)
		apply();
}

const char *fan_mode_name(enum fan_mode m)
{
	switch (m) {
	case WAIT:	return "WAIT";
	case AUTO:	return "AUTO";
	case MANUAL:	return "MANUAL";
	}
	return "?";
}

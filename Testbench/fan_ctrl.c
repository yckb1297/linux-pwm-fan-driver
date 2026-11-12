#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

#include "ec11_ioctl.h"
#include "fan_state.h"
#include "ledbar_ioctl.h"
#include "ledbar_ctrl.h"

#define DEV_EC11	"/dev/ec11"
#define GPIOCHIP	"/dev/gpiochip0"
#define LED_LINE	112		/* header pin 11, PR.04 */
#define DUTY_STEP	5		/* [%] per knob step, 20 steps for 0..100 */
#define ROTATE_REVERSED	1		/* 1: swap the knob direction reported by the driver (S1/S2 are wired crossed) */
#define EV_MAX		16

/* team globals that fan_state.c does not define */
int CW_CNT;
int CCW_CNT;
int FAN_LEVEL = 1;

static volatile sig_atomic_t running = 1;
static int led_fd = -1;
static int led_active_low;

static void on_signal(int sig)
{
	(void)sig;
	running = 0;
}

/* --- mode LED (GPIO character device, no extra library) --- */
static int led_open(int line)
{
	struct gpiohandle_request req;
	int chip, ret;

	chip = open(GPIOCHIP, O_RDONLY);
	if (chip == -1)
		return -1;

	memset(&req, 0, sizeof(req));
	req.lineoffsets[0] = line;
	req.lines = 1;
	req.flags = GPIOHANDLE_REQUEST_OUTPUT;
	req.default_values[0] = led_active_low ? 1 : 0;
	strncpy(req.consumer_label, "fan_mode_led", sizeof(req.consumer_label) - 1);

	ret = ioctl(chip, GPIO_GET_LINEHANDLE_IOCTL, &req);
	close(chip);
	if (ret == -1)
		return -1;

	return req.fd;
}

static void led_set(int on)
{
	struct gpiohandle_data data;

	if (led_fd == -1)
		return;

	memset(&data, 0, sizeof(data));
	data.values[0] = (on ? 1 : 0) ^ (led_active_low ? 1 : 0);
	if (ioctl(led_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == -1)
		printf("fan_ctrl: LED write failed: %s\n", strerror(errno));
}

/* --- LED bar --- */
static int bar_enabled;

/*
 * MANUAL shows the duty on the 8 bars, rounded up: 0 % = 0 bars, any duty above 0 lights
 * at least one bar. With the 5 % knob step this gives 2 and 3 steps per bar alternately
 * (25 % = 2 bars, 50 % = 4, 75 % = 6, 100 % = 8).
 */
static int duty_to_level(int duty)
{
	return (duty * LEDBAR_MAX_LEDS + DUTY_MAX - 1) / DUTY_MAX;
}

static void bar_update(void)
{
	if (!bar_enabled)
		return;

	FAN_LEVEL = duty_to_level(PWM_DUTY);
	ledbar_ctrl_update();
}

/* --- fan_ops --- */
static void ops_set_pwm(int duty)
{
	/* TODO: motor integration, apply the duty to the PWM device here */
	(void)duty;
}

static void ops_on_mode(enum fan_mode from, enum fan_mode to)
{
	printf("[STATE] %s -> %s\n", fan_mode_name(from), fan_mode_name(to));
	fflush(stdout);
}

static void ops_on_duty(int duty)
{
	printf("[DUTY ] %3d %%   (dial %d, mode %s)\n", duty, DIAL_CNT, fan_mode_name(fan_mode));
	fflush(stdout);
}

static const struct fan_ops ops = {
	.set_pwm = ops_set_pwm,
	.set_led = led_set,
	.on_mode = ops_on_mode,
	.on_duty = ops_on_duty,
};

static void usage(const char *prog)
{
	printf("usage: %s [-s duty_step] [-l led_line] [-L] [-n] [-B] [-r]\n", prog);
	printf("  -s  duty step per knob step in %% (default %d)\n", DUTY_STEP);
	printf("  -l  mode LED gpiochip0 line (default %d = header pin 11)\n", LED_LINE);
	printf("  -L  LED is active low\n");
	printf("  -n  do not use the LED\n");
	printf("  -B  do not use the LED bar (/dev/ledbar)\n");
	printf("  -r  swap the knob direction (default is %s)\n", ROTATE_REVERSED ? "swapped" : "as reported by the driver");
}

int main(int argc, char *argv[])
{
	struct ec11_event evs[EV_MAX];
	struct sigaction sa;
	int step = DUTY_STEP, line = LED_LINE, use_led = 1, use_bar = 1;
	int reversed = ROTATE_REVERSED;
	int fd, opt, rlen, i, n, delta;

	while ((opt = getopt(argc, argv, "s:l:LnBrh")) != -1) {
		switch (opt) {
		case 's': step = atoi(optarg); break;
		case 'l': line = atoi(optarg); break;
		case 'L': led_active_low = 1; break;
		case 'n': use_led = 0; break;
		case 'B': use_bar = 0; break;
		case 'r': reversed = !reversed; break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}
	if (step < 1 || step > DUTY_MAX) {
		printf("fan_ctrl: duty step must be 1..%d\n", DUTY_MAX);
		return EXIT_FAILURE;
	}

	fd = open(DEV_EC11, O_RDONLY);
	if (fd == -1) {
		printf("fan_ctrl: %s: %s (is ec11.ko loaded?)\n", DEV_EC11, strerror(errno));
		return EXIT_FAILURE;
	}

	if (use_led) {
		led_fd = led_open(line);
		if (led_fd == -1)
			printf("fan_ctrl: LED line %d not available (%s), continuing without LED\n",
			       line, strerror(errno));
	}

	if (use_bar) {
		if (ledbar_ctrl_init() == 0)
			bar_enabled = 1;
		else
			printf("fan_ctrl: LED bar not available, continuing without it\n");
	}

	/* no SA_RESTART: read() returns EINTR so Ctrl+C is seen right away */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	fan_init(&ops, step);
	printf("fan_ctrl: state = %s, duty = %d %%, step = %d %% (Ctrl+C to quit)\n",
	       fan_mode_name(fan_mode), PWM_DUTY, step);
	fflush(stdout);

	while (running) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int ready = poll(&pfd, 1, 100);

		if (ready == -1) {
			if (errno == EINTR)
				continue;
			printf("fan_ctrl: poll: %s\n", strerror(errno));
			break;
		}
		if (ready == 0) {	/* no knob event, still refresh the bar (TEMP may change in AUTO) */
			bar_update();
			continue;
		}

		rlen = read(fd, evs, sizeof(evs));
		if (rlen == -1) {
			if (errno == EINTR)
				continue;
			printf("fan_ctrl: read: %s\n", strerror(errno));
			break;
		}

		n = rlen / sizeof(struct ec11_event);
		for (i = 0; i < n; i++) {
			switch (evs[i].type) {
			case EC11_EV_ROTATE:
				delta = reversed ? -evs[i].delta : evs[i].delta;
				if (delta > 0)
					CW_CNT++;
				else
					CCW_CNT++;
				fan_on_rotate(delta);
				break;
			case EC11_EV_CLICK:
				fan_on_click();
				break;
			case EC11_EV_LONG_PRESS:
				fan_on_long_press();
				break;
			default:	/* EC11_EV_STATE is only for the raw level test */
				break;
			}
		}
		bar_update();
	}

	if (bar_enabled)
		ledbar_ctrl_close();
	led_set(0);
	if (led_fd != -1)
		close(led_fd);
	close(fd);
	printf("fan_ctrl: bye\n");

	return EXIT_SUCCESS;
}

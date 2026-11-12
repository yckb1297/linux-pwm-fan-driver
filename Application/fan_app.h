#ifndef FAN_APP_H
#define FAN_APP_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <motor_ioctl.h>
#include <ec11_ioctl.h>
#include <fan_globals.h>
#include <ledbar_ctrl.h>


/* Device paths */
#define MOTOR_DEV_PATH "/dev/mydev"
#define EC11_DEV_PATH  "/dev/ec11"

#define TEMP_PATH \
    "/sys/bus/iio/devices/iio:device0/in_temp_input"

/* Time */
#define SAMPLE_INTERVAL_MS 1000
#define POLL_INTERVAL_MS   100

/* EC11 */
#define MANUAL_DUTY_STEP  5
#define ROTATE_REVERSED   1
#define EC11_EVENT_MAX    16

/* AUTO temperature settings: mC */
#define FAN_ON_TEMP_MC    22000
#define FAN_OFF_TEMP_MC   20000
#define AUTO_FULL_TEMP_MC 32000

/* Duty */
#define MIN_DUTY_PERCENT  40
#define MAX_DUTY_PERCENT  100

typedef struct {
    enum fan_mode saved_mode;

    int current_temp_mc;
    bool temp_valid;
    bool auto_on;

    int manual_duty;
    int applied_duty;
} fan_state_t;

#if 0
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <motor_ioctl.h>

#define MOTOR_DEV_PATH "/dev/mydev"

/* Example path: replace with the actual BMP180 IIO device */
#define TEMP_PATH \
    "/sys/bus/iio/devices/iio:device0/in_temp_input"

#define SAMPLE_INTERVAL_SEC 1

#define FAN_ON_TEMP_MC   22000
#define FAN_OFF_TEMP_MC  20000
#define MIN_DUTY_PERCENT 40
#define MAX_DUTY_PERCENT 100

typedef struct {
    int current_temp_mc;
    unsigned int duty_percent;
    int fan_on;
    int temp_valid;
} fan_state_t;
#endif 
#endif /* FAN_APP_H */
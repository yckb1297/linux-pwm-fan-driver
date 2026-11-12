
#include "fan_app.h"

/* Team shared globals */
float TEMP = 0.0f;
int CW_CNT = 0;
int CCW_CNT = 0;
int PWM_DUTY = 0;
int FAN_LEVEL = 0;
enum fan_mode fan_mode = WAIT;
static int ledbar_active = 0;

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static const char *mode_name(enum fan_mode mode)
{
    switch (mode)
    {
    case WAIT:
        return "WAIT";
    case AUTO:
        return "AUTO";
    case MANUAL:
        return "MANUAL";
    default:
        return "UNKNOWN";
    }
}

static int64_t get_time_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return -1;

    return (int64_t)ts.tv_sec * 1000 +
           ts.tv_nsec / 1000000;
}

static int clamp_duty(int duty)
{
    if (duty < 0)
        return 0;
    if (duty > 100)
        return 100;
    return duty;
}

/* BMP180 IIO temperature read */
static int fan_read_temperature(int *temp_mc)
{
    FILE *fp;
    long value;

    if (!temp_mc)
        return -EINVAL;

    fp = fopen(TEMP_PATH, "r");
    if (!fp)
        return -errno;

    if (fscanf(fp, "%ld", &value) != 1)
    {
        int err = ferror(fp) ? errno : EIO;
        fclose(fp);
        return -(err ? err : EIO);
    }

    if (fclose(fp) != 0)
        return -errno;

    if (value < -40000 || value > 85000)
        return -ERANGE;

    *temp_mc = (int)value;
    return 0;
}

/* Motor character device ioctl */
static int motor_set_duty(int fd, int percent)
{
    __u32 duty;

    if (percent < 0 || percent > 100)
        return -EINVAL;

    duty = (__u32)percent;

    if (ioctl(fd, MY_IOCTL_CMD_SET_PWM, &duty) < 0)
        return -errno;

    return 0;
}

/* Existing AUTO hysteresis and linear duty */
static int calculate_auto_duty(fan_state_t *fan)
{
    int temp = fan->current_temp_mc;

    if (!fan->auto_on && temp >= FAN_ON_TEMP_MC)
        fan->auto_on = true;

    else if (fan->auto_on && temp < FAN_OFF_TEMP_MC)
        fan->auto_on = false;

    if (!fan->auto_on)
        return 0;

    if (temp <= FAN_ON_TEMP_MC)
        return MIN_DUTY_PERCENT;

    if (temp >= AUTO_FULL_TEMP_MC)
        return MAX_DUTY_PERCENT;

    return MIN_DUTY_PERCENT +
           (temp - FAN_ON_TEMP_MC) *
               (MAX_DUTY_PERCENT - MIN_DUTY_PERCENT) /
               (AUTO_FULL_TEMP_MC - FAN_ON_TEMP_MC);
}

/* Apply the duty selected by the current mode */
static int fan_apply(int motor_fd, fan_state_t *fan)
{
    int duty = 0;
    int ret;

    if (fan_mode == MANUAL)
    {
        duty = fan->manual_duty;
    }
    else if (fan_mode == AUTO && fan->temp_valid)
    {
        duty = calculate_auto_duty(fan);
    }

    duty = clamp_duty(duty);

    if (duty != fan->applied_duty)
    {
        ret = motor_set_duty(motor_fd, duty);
        if (ret < 0)
            return ret;

        fan->applied_duty = duty;
    }

    PWM_DUTY = duty;
    FAN_LEVEL = (duty * 8 + 99) / 100;

    /* Update LED Bar according to mode */
    if (ledbar_active)
    {
        ledbar_ctrl_update();
    }

    return 0;
}

/* Measure and update AUTO mode */
static int fan_update_auto(int motor_fd, fan_state_t *fan)
{
    int ret;

    ret = fan_read_temperature(&fan->current_temp_mc);

    if (ret < 0)
    {
        fprintf(stderr,
                "BMP180 read failed: %s\n",
                strerror(-ret));

        fan->temp_valid = false;
        fan->auto_on = false;
    }
    else
    {
        fan->temp_valid = true;
        TEMP = fan->current_temp_mc / 1000.0f;
    }

    ret = fan_apply(motor_fd, fan);
    if (ret < 0)
        return ret;

    if (fan->temp_valid)
    {
        printf("[AUTO] Temp=%.1f C, Duty=%d%%\n",
               TEMP, PWM_DUTY);
    }

    return 0;
}

/* Change WAIT / AUTO / MANUAL mode */
static int fan_change_mode(
    int motor_fd,
    fan_state_t *fan,
    enum fan_mode next_mode,
    int64_t *next_sample)
{
    enum fan_mode old_mode = fan_mode;

    if (next_mode == old_mode)
        return 0;

    if (next_mode == WAIT)
        fan->saved_mode = old_mode;

    fan_mode = next_mode;

    printf("[MODE] %s -> %s\n",
           mode_name(old_mode),
           mode_name(fan_mode));

    if (fan_mode == AUTO)
    {
        /* Require a fresh measurement */
        fan->temp_valid = false;
        *next_sample = 0;
        return 0;
    }

    return fan_apply(motor_fd, fan);
}

/* Handle EC11 events */
static int handle_ec11_event(
    int motor_fd,
    fan_state_t *fan,
    const struct ec11_event *ev,
    int reversed,
    int step,
    int64_t *next_sample)
{
    int delta;

    switch (ev->type)
    {

    case EC11_EV_ROTATE:
        delta = reversed ? -ev->delta : ev->delta;

        if (delta > 0)
            CW_CNT++;
        else if (delta < 0)
            CCW_CNT++;

        if (fan_mode != MANUAL)
            return 0;

        fan->manual_duty = clamp_duty(
            fan->manual_duty + delta * step);

        printf("[MANUAL] Duty=%d%%\n",
               fan->manual_duty);

        return fan_apply(motor_fd, fan);

    case EC11_EV_CLICK:
        if (fan_mode == WAIT)
            return fan_change_mode(
                motor_fd, fan, fan->saved_mode,
                next_sample);

        if (fan_mode == MANUAL)
            return fan_change_mode(
                motor_fd, fan, AUTO,
                next_sample);

        return fan_change_mode(
            motor_fd, fan, MANUAL,
            next_sample);

    case EC11_EV_LONG_PRESS:
        if (fan_mode != WAIT)
            return fan_change_mode(
                motor_fd, fan, WAIT,
                next_sample);
        return 0;

    case EC11_EV_STATE:
    default:
        return 0;
    }
}

int main(int argc, char **argv)
{
    fan_state_t fan = {
        .saved_mode = MANUAL};

    struct sigaction sa = {0};
    struct ec11_event events[EC11_EVENT_MAX];

    int motor_fd;
    int ec11_fd;
    int ret, opt;
    int reversed = ROTATE_REVERSED;
    int step = MANUAL_DUTY_STEP;
    int64_t next_sample = 0;

    while ((opt = getopt(argc, argv, "s:rh")) != -1)
    {
        switch (opt)
        {
        case 's':
            step = atoi(optarg);
            break;
        case 'r':
            reversed = !reversed;
            break;
        default:
            fprintf(stderr,
                    "Usage: %s [-s duty_step] [-r]\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (step < 1 || step > 100)
    {
        fprintf(stderr, "Invalid duty step\n");
        return EXIT_FAILURE;
    }

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    motor_fd = open(MOTOR_DEV_PATH, O_RDWR);
    if (motor_fd < 0)
    {
        perror("Motor open");
        return EXIT_FAILURE;
    }

    ret = motor_set_duty(motor_fd, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Motor init failed\n");
        close(motor_fd);
        return EXIT_FAILURE;
    }

    ec11_fd = open(EC11_DEV_PATH, O_RDONLY | O_NONBLOCK);
    if (ec11_fd < 0)
    {
        perror("EC11 open");
        close(motor_fd);
        return EXIT_FAILURE;
    }

    /* LED Bar initialization */
    if (ledbar_ctrl_init() == 0)
    {
        ledbar_active = 1;
        printf("[LED BAR] Initialized\n");
    }
    else
    {
        fprintf(stderr, "[LED BAR] Initialization failed\n");
    }

    printf("Fan Application Started\n");
    printf("[MODE] WAIT\n");
    printf("Duty step=%d%%, reversed=%d\n",
           step, reversed);

    while (running)
    {
        int64_t now;
        struct pollfd pfd = {
            .fd = ec11_fd,
            .events = POLLIN};

        /* AUTO periodic measurement */
        if (fan_mode == AUTO)
        {
            now = get_time_ms();
            if (now < 0)
                break;

            if (next_sample == 0 ||
                now >= next_sample)
            {

                ret = fan_update_auto(motor_fd, &fan);
                if (ret < 0)
                    break;

                next_sample = get_time_ms() +
                              SAMPLE_INTERVAL_MS;
            }
        }
        else
        {
            next_sample = 0;
        }

        ret = poll(&pfd, 1, POLL_INTERVAL_MS);

        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            perror("EC11 poll");
            break;
        }

        if (ret == 0)
            continue;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            fprintf(stderr, "EC11 device error\n");
            break;
        }

        if (pfd.revents & POLLIN)
        {
            ssize_t nread;
            size_t count, i;

            nread = read(ec11_fd, events, sizeof(events));

            if (nread < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                    continue;

                perror("EC11 read");
                break;
            }

            if (nread == 0 ||
                nread % sizeof(events[0]) != 0)
            {
                fprintf(stderr, "Invalid EC11 event data\n");
                break;
            }

            count = nread / sizeof(events[0]);

            for (i = 0; i < count; i++)
            {
                ret = handle_ec11_event(
                    motor_fd,
                    &fan,
                    &events[i],
                    reversed,
                    step,
                    &next_sample);

                if (ret < 0)
                {
                    fprintf(stderr,
                            "Fan control error: %s\n",
                            strerror(-ret));
                    running = 0;
                    break;
                }
            }
        }
    }

    if (motor_set_duty(motor_fd, 0) < 0)
        fprintf(stderr, "Motor stop failed\n");

    /* LED Bar cleanup */
    if (ledbar_active)
    {
        ledbar_ctrl_close();
        ledbar_active = 0;
    }

    close(ec11_fd);
    close(motor_fd);

    printf("Fan Application Stopped\n");

    return EXIT_SUCCESS;
}

#if 0
#include "fan_app.h"

static volatile sig_atomic_t running = 1;

/* Signal handler */
static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
};

/*
 * Read BMP180 temperature via the IIO sysfs interface.
 * Unit: milli-degree Celsius (mC).
 */
static int fan_read_temperature(int *temp_mc)
{
    FILE *fp;
    long value;

    if (temp_mc == NULL)
        return -EINVAL;

    fp = fopen(TEMP_PATH, "r");
    if (fp == NULL)
        return -errno;

    if (fscanf(fp, "%ld", &value) != 1) {
        int err = ferror(fp) ? errno : EIO;
        fclose(fp);
        return -(err ? err : EIO);
    }

    if (fclose(fp) != 0)
        return -errno;

    if (value < -40000 || value > 85000)
        return -ERANGE;

    *temp_mc = (int)value;

    return 0;
};

/* Hysteresis + proportional fan duty control */
static unsigned int fan_calculate_duty(fan_state_t *fan)
{
    int temp = fan->current_temp_mc;

    /* Step 1. Hysteresis ON/OFF */
    if (!fan->fan_on && temp >= FAN_ON_TEMP_MC)
        fan->fan_on = 1;

    else if (fan->fan_on && temp < FAN_OFF_TEMP_MC)
        fan->fan_on = 0;

    if (!fan->fan_on)
        return 0;

    if (temp <= 22000)
        return MIN_DUTY_PERCENT;

    if (temp >= 32000)
        return MAX_DUTY_PERCENT;

    /* 25C ~ 35C : 40% ~ 100% */
    return MIN_DUTY_PERCENT +
           (unsigned int)((temp - 22000) * 60 / 10000);
};

/* Send PWM Duty to the Motor Character Device Driver */
static int fan_motor_set_duty(int motor_fd, unsigned int duty_percent)
{
    __u32 duty = (__u32)duty_percent;

    if (duty_percent > 100)
        return -EINVAL;

    if (ioctl(motor_fd, MY_IOCTL_CMD_SET_PWM, &duty) < 0)
        return -errno;

    return 0;
};

/* Wait for the next sampling interval */
static void fan_wait_interval(void)
{
    struct timespec req = {
        .tv_sec = SAMPLE_INTERVAL_SEC,
        .tv_nsec = 0
    };

    struct timespec rem;

    while (running && nanosleep(&req, &rem) < 0) {
        if (errno != EINTR)
            break;
        req = rem;
    }
};

int main(void)
{
    int motor_fd;
    int ret;
    fan_state_t fan = {0};

    struct sigaction sa = {0};

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Open Motor Driver once */
    motor_fd = open(MOTOR_DEV_PATH, O_RDWR);

    if (motor_fd < 0) {
        perror("Motor device open");
        return EXIT_FAILURE;
    }

    /* Initial motor stop */
    ret = fan_motor_set_duty(motor_fd, 0);
    if (ret < 0) {
        fprintf(stderr, "Motor initialization failed: %d\n", ret);
        close(motor_fd);
        return EXIT_FAILURE;
    }

    printf("Automatic Fan Control Started\n");

    while (running) {

        /* 1. Read current temperature */
        ret = fan_read_temperature(&fan.current_temp_mc);

        if (ret < 0) {
            fprintf(stderr, "BMP180 temperature read failed: %d\n", ret);

            fan.temp_valid = 0;
            fan.fan_on = 0;
            fan.duty_percent = 0;

            /* Fail-safe policy: stop the motor */
            if (fan_motor_set_duty(motor_fd, 0) < 0) {
                fprintf(stderr, "Motor stop failed\n");
                break;
            }

            fan_wait_interval();
            continue;
        }

        fan.temp_valid = 1;

        /* 2. Calculate PWM Duty */
        fan.duty_percent = fan_calculate_duty(&fan);

        /* 3. Apply PWM through ioctl */
        ret = fan_motor_set_duty(motor_fd, fan.duty_percent);

        if (ret < 0) {
            fprintf(stderr, "Motor ioctl failed: %d\n", ret);
            break;
        }

        /* 4. Log current status */
        printf("Temp: %.1f C | Fan: %s | Duty: %u%%\n",
               fan.current_temp_mc / 1000.0,
               fan.fan_on ? "ON" : "OFF",
               fan.duty_percent);

        fflush(stdout);

        /* 5. Wait one second */
        fan_wait_interval();
    }

    /* Stop motor before closing */
    if (fan_motor_set_duty(motor_fd, 0) < 0)
        fprintf(stderr, "Final motor stop failed\n");

    close(motor_fd);

    printf("Automatic Fan Control Stopped\n");

    return EXIT_SUCCESS;
};
#endif
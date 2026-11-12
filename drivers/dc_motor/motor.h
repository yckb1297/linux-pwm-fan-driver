#ifndef MOTOR_H
#define MOTOR_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/platform_device.h>
#include <linux/of.h>

#include <linux/gpio/consumer.h>
#include <linux/pwm.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "motor_ioctl.h"

/* Character device */
#define DEVICE_NAME "mydev"

/* Device Tree compatible */
#define MOTOR_COMPATIBLE "smartfan,dc-motor"

/* PWM period: 1 ms = 1 kHz */
#define PWM_PERIOD_NS 1000000ULL

struct motor_dev {
    struct device *dev;

    /* GPIO resources from Device Tree */
    struct gpio_desc *in1;
    struct gpio_desc *in2;

    /* PWM resource from Device Tree */
    struct pwm_device *pwm;

    /* Character device */
    struct miscdevice miscdev;

    /* Synchronization */
    struct mutex lock;
};

#endif /* MOTOR_H */

#if 0
#ifndef MOTOR_H
#define MOTOR_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "motor_ioctl.h"

#define DEVICE_NAME "mydev"
#define DEVICE_MAJOR 120
#define DEVICE_MINOR 0

#define PWM_PERIOD_NS 1000000ULL  // 1kHz

#define GPIO_IN1 492
#define GPIO_IN2 433
#define PWM_ID 2

#endif /* MOTOR_H */
#endif
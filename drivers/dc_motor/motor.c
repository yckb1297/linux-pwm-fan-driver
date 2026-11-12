#include "motor.h"

/* 듀티비와 출력 활성화 여부를 PWM 장치에 적용한다. */
static int motor_pwm_apply(struct motor_dev *motor, bool enable, unsigned int duty_percent)
{
    struct pwm_state state;
    u64 duty_ns;

    if (!motor || !motor->pwm)
        return -EINVAL;

    if (duty_percent > 100)
        return -EINVAL;

    // 듀티비(%)를 PWM 주기 내 활성 시간(ns)으로 변환
    duty_ns = PWM_PERIOD_NS * (u64)duty_percent / 100;

    // 현재 상태를 가져온 뒤 필요한 설정 갱신
    pwm_get_state(motor->pwm, &state);

    // 출력을 비활성화할 때는 활성 시간도 0으로 설정
    state.period = PWM_PERIOD_NS;
    state.duty_cycle = enable ? duty_ns : 0;
    state.polarity = PWM_POLARITY_NORMAL;
    state.enabled = enable;

    return pwm_apply_state(motor->pwm, &state);
};

// PWM 출력을 끄고 방향 제어 신호를 0으로 설정
static int motor_stop(struct motor_dev *motor)
{
    int ret;

    if (!motor)
        return -EINVAL;

    ret = motor_pwm_apply(motor, false, 0);

    // PWM 비활성화가 실패하더라도 방향 제어 신호는 0으로 설정
    gpiod_set_value(motor->in1, 0);
    gpiod_set_value(motor->in2, 0);

    if (ret)
        dev_err(motor->dev,
                "Failed to disable PWM (%d)\n", ret);
    else
        dev_info(motor->dev, "Motor stopped\n");

    return ret;
};

// 0~100% 듀티비로 속도를 제어, 0% 요청은 정지 처리
static int motor_set_speed(struct motor_dev *motor,
                           unsigned int duty_percent)
{
    int ret;

    if (!motor)
        return -EINVAL;

    if (duty_percent > 100)
        return -EINVAL;

    if (duty_percent == 0)
        return motor_stop(motor);

    // IN1=1, IN2=0으로 회전 방향을 설정
    gpiod_set_value(motor->in1, 1);
    gpiod_set_value(motor->in2, 0);

    ret = motor_pwm_apply(motor, true, duty_percent);

    if (ret)
    {
        dev_err(motor->dev,
                "PWM apply failed (%d)\n", ret);

        // PWM 적용에 실패하면 정지를 시도하고 오류 반환
        motor_stop(motor);
        return ret;
    }

    return 0;
};

// 사용자 공간의 ioctl 요청을 받아 PWM 듀티비를 설정
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct motor_dev *motor = filp->private_data;
    __u32 duty_percent;
    int ret;

    if (!motor)
        return -ENODEV;

    switch (cmd)
    {

    case MY_IOCTL_CMD_SET_PWM:

        // arg가 가리키는 사용자 공간에서 32비트 듀티비 값을 복사
        if (copy_from_user(&duty_percent,
                           (void __user *)arg,
                           sizeof(duty_percent)))
            return -EFAULT;

        if (duty_percent > 100)
            return -EINVAL;

        // 다른 ioctl 또는 파일 닫기 처리와 모터 제어가 겹치지 않도록 잠금
        mutex_lock(&motor->lock);

        ret = motor_set_speed(motor, duty_percent);

        mutex_unlock(&motor->lock);

        if (ret)
        {
            dev_err(motor->dev,
                    "Failed to set PWM duty: %d\n",
                    ret);
            return ret;
        }

        dev_info(motor->dev,
                 "PWM Duty: %u%%\n",
                 duty_percent);

        return 0;

    default:
        return -ENOTTY;
    }
};

// 장치 파일을 열면 이후 파일 연산에서 사용할 모터 정보 연결
static int device_open(struct inode *inode, struct file *filp)
{
    struct miscdevice *misc = filp->private_data;

    // misc 장치 포인터로 이를 포함하는 모터 구조체의 주소 구하기
    struct motor_dev *motor =
        container_of(misc, struct motor_dev, miscdev);

    filp->private_data = motor;

    dev_info(motor->dev, "Motor device opened\n");

    return 0;
};

// 열린 파일의 마지막 참조가 해제될 때 모터를 정지
static int device_release(struct inode *inode, struct file *filp)
{
    struct motor_dev *motor = filp->private_data;
    int ret;

    if (!motor)
        return -ENODEV;

    mutex_lock(&motor->lock);

    ret = motor_stop(motor);

    mutex_unlock(&motor->lock);

    dev_info(motor->dev, "Motor device closed\n");

    return ret;
};

// 장치 파일 연산과 드라이버의 처리 함수를 연결
static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};

// 장치 트리와 일치하는 장치의 자원을 확보하고 장치 파일 등록
static int motor_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct motor_dev *motor;
    int ret;

    // 장치별 상태를 저장, 장치 해제 시 메모리 자동 반환
    motor = devm_kzalloc(dev, sizeof(*motor), GFP_KERNEL);
    if (!motor)
        return -ENOMEM;

    motor->dev = dev;
    mutex_init(&motor->lock);

    // 디바이스 트리의 in1-gpio 확보, 논리값 0으로 초기화
    motor->in1 = devm_gpiod_get(
        dev, "in1", GPIOD_OUT_LOW);

    if (IS_ERR(motor->in1))
        return dev_err_probe(
            dev, PTR_ERR(motor->in1),
            "Failed to get IN1 GPIO\n");

    // 디바이스 트리의 in2-gpio 확보, 논리값 0으로 초기화
    motor->in2 = devm_gpiod_get(
        dev, "in2", GPIOD_OUT_LOW);

    if (IS_ERR(motor->in2))
        return dev_err_probe(
            dev, PTR_ERR(motor->in2),
            "Failed to get IN2 GPIO\n");

    // 디바이스 트리에서 "motor" 이름에 연결된 PWM 자원 확보
    motor->pwm = devm_pwm_get(dev, "motor");

    if (IS_ERR(motor->pwm))
        return dev_err_probe(
            dev, PTR_ERR(motor->pwm),
            "Failed to get motor PWM\n");

    // 사용자 요청을 받기 전에 PWM 출력을 비활성 상태로 초기화
    ret = motor_pwm_apply(motor, false, 0);

    if (ret)
        return dev_err_probe(
            dev, ret,
            "Failed to initialize PWM\n");

    // 마이너 번호를 동적 할당, /dev/mydev 장치를 등록
    motor->miscdev.minor = MISC_DYNAMIC_MINOR;
    motor->miscdev.name = DEVICE_NAME;
    motor->miscdev.fops = &my_fops;
    motor->miscdev.parent = dev;

    ret = misc_register(&motor->miscdev);

    if (ret)
        return dev_err_probe(
            dev, ret,
            "Failed to register motor misc device\n");

    // 장치 제거 시 모터 정보를 다시 찾을 수 있도록 저장
    platform_set_drvdata(pdev, motor);

    dev_info(dev,
             "Motor driver probed successfully\n");

    return 0;
};

// 디바이스 파일 등록을 해제, 모터를 정지
static int motor_remove(struct platform_device *pdev)
{
    struct motor_dev *motor =
        platform_get_drvdata(pdev);

    misc_deregister(&motor->miscdev);

    mutex_lock(&motor->lock);
    motor_stop(motor);
    mutex_unlock(&motor->lock);

    dev_info(&pdev->dev, "Motor driver removed\n");

    return 0;
};

// 디바이스 트리의 compatible 값과 드라이버를 연결
static const struct of_device_id motor_of_match[] = {
    {
        .compatible = MOTOR_COMPATIBLE,
    },
    {

    }
};

MODULE_DEVICE_TABLE(of, motor_of_match);

static struct platform_driver motor_driver = {
    .probe = motor_probe,
    .remove = motor_remove,
    .driver = {
        .name = "smartfan-motor",
        .of_match_table = motor_of_match,
    },
};

// 모듈을 불러오거나 내릴 때 플랫폼 드라이버를 등록하거나 해제
module_platform_driver(motor_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Jetson Orin Nano GPIO PWM test driver");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "motor_ioctl.h"

#define DEV_NAME "/dev/mydev"

int main(int argc, char *argv[])
{
    int fd;
    char *end;
    unsigned long value;
    __u32 duty_percent;

    if (argc != 2) {
        printf("Usage: %s <duty 0-100>\n", argv[0]);
        return EXIT_FAILURE;
    }

    errno = 0;
    value = strtoul(argv[1], &end, 10);
    if (errno != 0 || *end != '\0' || value > 100) {
        printf("Invalid duty: %s (expected 0-100)\n", argv[1]);
        return EXIT_FAILURE;
    }

    duty_percent = (__u32)value;

    fd = open(DEV_NAME, O_RDWR);
    if (fd == -1) {
        perror("motor_app: open");
        return EXIT_FAILURE;
    }

    printf("motor_app: %s opened\n", DEV_NAME);

    if (ioctl(fd, MY_IOCTL_CMD_SET_PWM, &duty_percent) == -1) {
        perror("motor_app: ioctl");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("motor_app: duty set to %u%%\n", duty_percent);

    if (duty_percent > 0) {
        printf("Press Enter to stop the motor.\n");
        getchar();
    }

    close(fd);
    printf("motor_app: %s closed\n", DEV_NAME);

    return EXIT_SUCCESS;
}
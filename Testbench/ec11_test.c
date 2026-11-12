#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "ec11_ioctl.h"

#define DEV_NAME "/dev/ec11"
#define EV_MAX 16

static void usage(const char *prog)
{
	printf("usage: %s            print KEY/S2/S1 levels on every change (Ctrl+C to quit)\n", prog);
	printf("       %s E          print rotation/click events (Ctrl+C to quit)\n", prog);
	printf("       %s I GET      print current count\n", prog);
	printf("       %s I RESET    reset count to 0\n", prog);
}

static int monitor(int fd, int state_mode)
{
	struct ec11_event evs[EV_MAX];
	int rlen, i, n;

	printf("ec11_test: waiting for events (Ctrl+C to quit)\n");
	while(1) {
		rlen = read(fd, evs, sizeof(evs));
		if(rlen == -1) {
			if(errno == EINTR) continue;
			printf("ec11_test: %s (%d)\n", strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		n = rlen / sizeof(struct ec11_event);
		for(i = 0; i < n; i++) {
			if(state_mode) {
				if(evs[i].type == EC11_EV_STATE) {
					printf("KEY=%d S2=%d S1=%d\n",
					       !!(evs[i].state & EC11_STATE_KEY),
					       !!(evs[i].state & EC11_STATE_S2),
					       !!(evs[i].state & EC11_STATE_S1));
				}
				continue;
			}

			switch(evs[i].type) {
				case EC11_EV_ROTATE:
					printf("[ROTATE] %s  step = %+d  count = %d\n",
					       evs[i].delta > 0 ? "CW " : "CCW", evs[i].delta, evs[i].count);
					break;
				case EC11_EV_CLICK:
					printf("[CLICK ] short click     count = %d\n", evs[i].count);
					break;
				case EC11_EV_LONG_PRESS:
					printf("[LONG  ] long press      count = %d\n", evs[i].count);
					break;
				default:
					break;
			}
		}
		fflush(stdout);
	}

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	int fd, ret = EXIT_SUCCESS;

	fd = open(DEV_NAME, O_RDONLY);
	if(fd == -1) {
		printf("ec11_test: %s (%d)\n", strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	printf("ec11_test: %s opened\n", DEV_NAME);

	if(argc == 1) {
		ret = monitor(fd, 1);
	}
	else if(argc == 2 && strcmp(argv[1], "E") == 0) {
		ret = monitor(fd, 0);
	}
	else if(argc == 3 && strcmp(argv[1], "I") == 0) {
		int count;

		if(strcmp(argv[2], "GET") == 0) {
			if(ioctl(fd, EC11_IOCTL_GET_COUNT, &count) == -1) {
				printf("ec11_test: %s (%d)\n", strerror(errno), __LINE__);
				ret = EXIT_FAILURE;
			}
			else {
				printf("ec11_test: count = %d\n", count);
			}
		}
		else if(strcmp(argv[2], "RESET") == 0) {
			if(ioctl(fd, EC11_IOCTL_RESET_COUNT) == -1) {
				printf("ec11_test: %s (%d)\n", strerror(errno), __LINE__);
				ret = EXIT_FAILURE;
			}
			else {
				printf("ec11_test: count reset\n");
			}
		}
		else {
			usage(argv[0]);
		}
	}
	else {
		usage(argv[0]);
	}

	close(fd);
	printf("ec11_test: %s closed\n", DEV_NAME);

	return ret;
}

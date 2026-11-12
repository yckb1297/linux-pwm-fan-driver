CC ?= cc
CFLAGS += -Wall -Wextra -O2
EC11_DIR := ../drivers/ec11
LEDBAR_DIR := ../drivers/ledbar
APP_DIR := ../Application
CPPFLAGS += -I$(EC11_DIR) -I$(LEDBAR_DIR) -I$(APP_DIR)

all: ec11_test ledbar_test fan_ctrl

ec11_test: ec11_test.c $(EC11_DIR)/ec11_ioctl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

ledbar_test: ledbar_test.c $(LEDBAR_DIR)/ledbar_ioctl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -lm

fan_ctrl: fan_ctrl.c fan_state.c fan_state.h $(APP_DIR)/fan_globals.h $(EC11_DIR)/ec11_ioctl.h $(LEDBAR_DIR)/ledbar_ioctl.h $(LEDBAR_DIR)/ledbar_ctrl.c $(LEDBAR_DIR)/ledbar_ctrl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ fan_ctrl.c fan_state.c $(LEDBAR_DIR)/ledbar_ctrl.c -lm

fan_led_sim: fan_led_sim.c $(APP_DIR)/fan_globals.h $(LEDBAR_DIR)/ledbar_ctrl.c $(LEDBAR_DIR)/ledbar_ctrl.h $(LEDBAR_DIR)/ledbar_ioctl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ fan_led_sim.c $(LEDBAR_DIR)/ledbar_ctrl.c -lm

test: ec11_test ledbar_test
sim: fan_led_sim
clean:
	rm -f ec11_test ledbar_test fan_ctrl fan_led_sim
.PHONY: all test sim clean

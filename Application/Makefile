CC ?= cc
CFLAGS += -Wall -Wextra -O2 -std=gnu11
MOTOR_DIR := ../drivers/dc_motor
EC11_DIR := ../drivers/ec11
LEDBAR_DIR := ../drivers/ledbar
CPPFLAGS += -I. -I$(MOTOR_DIR) -I$(EC11_DIR) -I$(LEDBAR_DIR)

TARGET := fan_app
SRCS := fan_app.c $(LEDBAR_DIR)/ledbar_ctrl.c
HEADERS := fan_app.h fan_globals.h $(MOTOR_DIR)/motor_ioctl.h $(EC11_DIR)/ec11_ioctl.h $(LEDBAR_DIR)/ledbar_ctrl.h $(LEDBAR_DIR)/ledbar_ioctl.h

all: $(TARGET)
$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) -lm
clean:
	rm -f $(TARGET)
.PHONY: all clean

# SMS app native and ARM | SE203134
CC_ARM := arm-linux-gnueabihf-gcc
CFLAGS := -std=gnu11 -O0 -g -Wall -Wextra -pthread
LIBS := -pthread -lrt
SRC := sms_app.c circular_buffer.c
all: arm host
arm:
	$(CC_ARM) $(CFLAGS) -march=armv7-a -mfpu=neon-vfpv3 -mfloat-abi=hard -static -o sms_app $(SRC) $(LIBS)
host:
	gcc $(CFLAGS) -o sms_app_x86 $(SRC) $(LIBS)
clean:
	rm -f sms_app sms_app_x86

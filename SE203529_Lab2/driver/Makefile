KDIR := /home/vankhoa/embedded_lab1/kernel/linux-5.15
obj-m += lab2_driver.o
ccflags-y += -march=armv7-a
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean

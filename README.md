# LOS201 - IC2009 Embedded Linux Labs

Team repository for LOS201_IC2009

## Members

- Minh
- Khoa
- Tuan

## Repository Contents

### Lab01
Embedded Linux environment setup, kernel/U-Boot/BusyBox build,
initramfs creation, and QEMU ARM boot.

### Lab02
Linux character device driver including:

- `/dev/lab2`
- read/write operations
- procfs interface
- sysfs interface
- mutex protection
- JFFS2 / MTD practice
- BusyBox automatic module loading
- ARM kernel module `lab2_driver.ko`

### Lab02-Practice-20Q
Additional examination practice extending the LAB2 driver with
20 features including counters, sysfs attributes, append mode,
statistics, configurable limits, one-shot reads, and ioctl.

## Target Platform

- QEMU: `vexpress-a9`
- CPU: Cortex-A9 / ARMv7
- Linux Kernel: 5.15
- BusyBox: 1.35.0

## Note About Kernel Modules

The included `.ko` files were compiled for the LAB target kernel
and ARM architecture. If the target kernel or configuration changes,
the module should be rebuilt from source using the included Makefile.

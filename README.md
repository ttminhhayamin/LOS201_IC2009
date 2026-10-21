# LOS201 - IC2009 Embedded Linux Labs

Team repository for LOS201_IC2009

## Members

- Minh
- Khoa
- Tuan

## Repository Contents

### `SE203134_Lab01/`
Embedded Linux environment setup, kernel/U-Boot/BusyBox build,
initramfs creation, and QEMU ARM boot.

### `SE203134_Lab02/`
Linux character device driver including:

- `/dev/lab2`
- read/write operations
- procfs interface
- sysfs interface
- mutex protection
- JFFS2 / MTD practice
- BusyBox automatic module loading
- ARM kernel module `lab2_driver.ko`

### `SE203134_Lab02-Practice-20Q/source/`
SE203134's additional LAB2 examination practice (20 features), including
the combined `lab2_driver.c`, `lab2_driver.ko`, and `test_all20.sh`.

### `SE204186_Lab02-Practice-20Q/source/`
Files contributed in the 2026-10-21 GitHub upload by `maituan16206-arch`.
This set includes Assignment 01 ioctl driver code, associated build files,
and a test program. Kept together under its contributor prefix.

The student-ID prefixes identify the project folders. Original source code,
compiled modules, scripts, and evidence filenames are preserved within them.

## Target Platform

- QEMU: `vexpress-a9`
- CPU: Cortex-A9 / ARMv7
- Linux Kernel: 5.15
- BusyBox: 1.35.0

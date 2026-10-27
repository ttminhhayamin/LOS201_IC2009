#!/usr/bin/env bash
set -euo pipefail
W="$HOME/embedded_asgn2"
script -q -f "$W/logs/task4_qemu_session.txt" -c \
  "qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic -kernel $HOME/embedded_exam3_fresh/output/zImage_exam3 -dtb $HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb -initrd $W/output/initramfs_asgn2.cpio.gz -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'"

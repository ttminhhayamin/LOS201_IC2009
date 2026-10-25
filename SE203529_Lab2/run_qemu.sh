#!/bin/bash
# Chay QEMU va ghi toan bo phien vao logs/boot_log.txt. Thoat QEMU: Ctrl+A roi X
L1=$HOME/embedded_lab1; L2=$HOME/embedded_lab2
script -q -c "qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
 -kernel $L1/output/zImage -dtb $L1/output/vexpress-v2p-ca9.dtb \
 -initrd $L2/output/initramfs_lab2.cpio.gz \
 -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'" "$L2/logs/boot_log.txt"

#!/usr/bin/env bash
set -euo pipefail
W="$HOME/embedded_exam3_fresh"
exec qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
 -kernel "$W/output/zImage_exam3" \
 -dtb "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb" \
 -initrd "$W/output/initramfs_exam3.cpio.gz" \
 -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'

#!/bin/bash
set -euo pipefail
W="$HOME/embedded_asgn2"
mkdir -p "$W/logs"
echo 'INSIDE QEMU, after / #, run: sh /usr/bin/task2_run.sh'
echo 'AFTER COMPLETE, leave QEMU with Ctrl+A then X.'
script -q -f -c "qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic -kernel $HOME/embedded_exam3_fresh/output/zImage_exam3 -dtb $HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb -initrd $W/output/initramfs_asgn2.cpio.gz -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'" "$W/logs/task2_qemu_session.txt"

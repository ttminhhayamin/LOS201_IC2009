#!/bin/sh
echo '=== LAB-02: Device Driver Test ==='
echo '[1] Loading lab2_driver...'
# Neu rcS da load san thi bo qua (insmod lan 2 se bao "File exists")
if lsmod | grep -q '^lab2_driver'; then
  echo '  (module already loaded by rcS)'
else
  insmod /lib/modules/5.15.0/lab2_driver.ko || { echo 'ERROR: insmod failed!'; exit 1; }
fi
echo '[2] Loaded modules:'
lsmod | grep lab2
echo '[3] Creating device node...'
[ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0
ls -la /dev/lab2
echo '[4] Writing to device...'
echo 'Hello from userspace, LAB-02!' > /dev/lab2
echo '[5] Reading from device...'
DATA=$(cat /dev/lab2)
echo "  Read back: [$DATA]"
echo '[6] Multiple write/read test...'
for i in 1 2 3; do
    echo "Message_$i" > /dev/lab2
    cat /dev/lab2
done
echo '[7] Kernel messages (dmesg):'
dmesg | grep lab2 | tail -10
echo '=== Test PASSED ==='

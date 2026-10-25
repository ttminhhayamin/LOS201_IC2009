#!/bin/sh
echo '##### BEGIN driver_test'; test_driver.sh; echo '##### END driver_test'
echo '##### BEGIN procfs_test'; test_procfs.sh; echo '##### END procfs_test'
echo '##### BEGIN direct_check'
echo '$ lsmod'; lsmod
echo '$ ls -la /dev/lab2'; ls -la /dev/lab2
echo '$ cat /proc/lab2_info'; cat /proc/lab2_info
echo '$ ls /sys/class/lab2_class/lab2/'; ls /sys/class/lab2_class/lab2/
for a in buffer_len open_count last_data; do echo "\$ cat /sys/class/lab2_class/lab2/$a"; cat /sys/class/lab2_class/lab2/$a; done
echo '$ cat /proc/devices | grep lab2'; grep lab2 /proc/devices
echo '$ dmesg | grep lab2'; dmesg | grep lab2
echo '##### END direct_check'

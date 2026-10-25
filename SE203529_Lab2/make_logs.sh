#!/bin/bash
# Tach boot_log.txt thanh driver_test.txt / procfs_test.txt (chay SAU khi thoat QEMU)
cd $HOME/embedded_lab2/logs
tr -d '\r' < boot_log.txt > boot_clean.txt
sed -n '/##### BEGIN driver_test/,/##### END driver_test/p' boot_clean.txt > driver_test.txt
sed -n '/##### BEGIN procfs_test/,/##### END procfs_test/p' boot_clean.txt > procfs_test.txt
sed -n '/##### BEGIN direct_check/,/##### END direct_check/p' boot_clean.txt > direct_check.txt
wc -l driver_test.txt procfs_test.txt direct_check.txt

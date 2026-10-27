#!/bin/sh
# Assignment02 SE203134, QEMU ARM final reliability/rotation test.
# Output is recorded by 'script' on the Host.
set -u
APP=/usr/bin/sms_app
MOD=/lib/modules/5.15.0/sms_sensor.ko
if ! grep -q '^sms_sensor ' /proc/modules; then
    insmod "$MOD" || { echo 'FAIL: sensor module load'; exit 2; }
fi
if [ ! -c /dev/sms_sensor ]; then
    DEV=$(cat /sys/class/sms_class/sms_sensor/dev) || exit 2
    mknod /dev/sms_sensor c "${DEV%:*}" "${DEV#*:}" || exit 2
fi
rm -f /tmp/sms.log /tmp/sms.log.1

echo '==== NORMAL 60 SECOND REGRESSION (REAL ARM DRIVER) ===='
"$APP" -i 500 -d 60 -t 40 -h 80 > /tmp/ff_normal.log 2>&1
rc=$?
echo "NORMAL_EXIT=$rc"
grep '^\[SUMMARY\]' /tmp/ff_normal.log
cat /proc/sms_stats
[ "$rc" -eq 0 ] || echo '[FAIL] normal run nonzero exit'

echo '==== LOG ROTATION THRESHOLD TEST ===='
# Synthetic 1MB+ log: tests rotation mechanics without a long wait.
rm -f /tmp/sms.log /tmp/sms.log.1
dd if=/dev/zero of=/tmp/sms.log bs=1024 count=1025 2>/dev/null
"$APP" -i 500 -d 5 > /tmp/ff_rotation.log 2>&1
rot_rc=$?
echo "ROTATION_APP_EXIT=$rot_rc"
grep '^\[SUMMARY\]' /tmp/ff_rotation.log
if [ -f /tmp/sms.log.1 ] && [ -s /tmp/sms.log ]; then
  echo 'ROTATION_PASS (sms.log.1 exists; new sms.log nonempty)'
  ls -l /tmp/sms.log /tmp/sms.log.1
else
  echo 'ROTATION_FAIL'
fi

echo '==== ERROR PATH: LOGGER CANNOT OPEN LOG ===='
rm -f /tmp/sms.log /tmp/sms.log.1
mkdir /tmp/sms.log || exit 2
"$APP" -i 500 -d 5 >/tmp/ff_error.log 2>&1 &
epid=$!
sleep 9
if kill -0 "$epid" 2>/dev/null; then
  echo 'ERROR_PATH_HANG - killing for safety'
  kill -9 "$epid" 2>/dev/null || true
  wait "$epid" 2>/dev/null || true
else
  wait "$epid"
  erc=$?
  echo "ERROR_PATH_EXIT=$erc"
  if [ "$erc" -ne 0 ]; then echo 'ERROR_PATH_NONZERO_PASS'; else echo 'ERROR_PATH_FAIL unexpected 0'; fi
fi
rmdir /tmp/sms.log || true

echo '==== FINAL FIXES QEMU DONE ===='

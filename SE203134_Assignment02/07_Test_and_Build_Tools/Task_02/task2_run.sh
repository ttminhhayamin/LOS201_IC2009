#!/bin/sh
# Assignment 02 Task 2 complete QEMU suite; run inside ARM console (/ #).
set -u
LOGDIR=/tmp
MODULE=/lib/modules/5.15.0/sms_sensor.ko
APP=/usr/bin/sms_app

if ! grep -q '^sms_sensor ' /proc/modules; then
    insmod "$MODULE" || { echo '[ERROR] Cannot load sms_sensor'; exit 1; }
fi
if [ ! -e /dev/sms_sensor ]; then
    DEV=$(cat /sys/class/sms_class/sms_sensor/dev) || exit 1
    mknod /dev/sms_sensor c "${DEV%:*}" "${DEV#*:}" || exit 1
fi

echo '===== TASK2 DRIVER INTEGRATION ====='
/usr/bin/task2_ioctl_test

# ioctl test can change interval and reads once, so wait before testing 60s
sleep 1

echo '===== TASK2 PROC BEFORE ====='
cat /proc/sms_stats | tee "$LOGDIR/task2_proc_before.txt"
BEFORE=$(awk '/^read_count:/ {print $2}' /proc/sms_stats)

echo '===== TASK2 APP START ====='
"$APP" -i 500 -t 40 -h 80 -d 60 -v > "$LOGDIR/task2_app.txt" 2>&1 &
PID=$!
echo "APP_PID=$PID"

measure() {
    name="$1"
    echo "===== TASK2 MEMORY $name ====="
    if [ ! -r "/proc/$PID/status" ]; then
        echo '[ERROR] application exited before measurement'; return 1
    fi
    grep -E '^(Name|VmRSS|VmSize|VmData|VmStk|Threads):' "/proc/$PID/status" | tee "$LOGDIR/task2_mem_${name}.txt"
}

sleep 5; measure T05 || exit 2
sleep 25; measure T30 || exit 2
sleep 25; measure T55 || exit 2

echo '===== TASK2 MAPS ====='
if [ -r "/proc/$PID/maps" ]; then
    cat "/proc/$PID/maps" | tee "$LOGDIR/task2_maps.txt"
fi
echo '===== TASK2 THREAD IDS ====='
ls "/proc/$PID/task"

wait "$PID"
STATUS=$?
echo '===== TASK2 EXIT CODE ====='
echo "exit=$STATUS"
echo '===== TASK2 SUMMARY ====='
grep '^\[SUMMARY\]' "$LOGDIR/task2_app.txt" || true

echo '===== TASK2 PROC AFTER ====='
cat /proc/sms_stats | tee "$LOGDIR/task2_proc_after.txt"
AFTER=$(awk '/^read_count:/ {print $2}' /proc/sms_stats)
DELTA=$((AFTER-BEFORE))
echo "DRIVER_READ_DELTA=$DELTA"
echo '===== TASK2 LOGGER ====='
ls -lh /tmp/sms.log 2>&1 || true
tail -4 /tmp/sms.log 2>&1 || true
if [ "$STATUS" -eq 0 ] && [ "$DELTA" -ge 110 ] && [ "$DELTA" -le 130 ]; then
    echo 'TASK2_QEMU_FUNCTIONAL_PASS'
else
    echo 'TASK2_QEMU_CHECK_COUNTS_OR_EXIT'
fi

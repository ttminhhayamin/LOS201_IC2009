#!/bin/sh
# QEMU ARM (BusyBox ash) | 2x125 seconds (100ms), 1000+ samples per policy.
# Run as root on QEMU: sh /usr/bin/task4_run.sh
set -u
if ! grep -q '^sms_sensor ' /proc/modules; then
    insmod /lib/modules/5.15.0/sms_sensor.ko || { echo 'TASK4_FAIL: driver load'; exit 1; }
fi
if [ ! -c /dev/sms_sensor ]; then
    DEV=$(cat /sys/class/sms_class/sms_sensor/dev) || exit 1
    mknod /dev/sms_sensor c "${DEV%:*}" "${DEV#*:}" || exit 1
fi
echo '===== TASK 4: QEMU / HARDWARE / BASELINE ====='
uname -a
cat /proc/sms_stats
fail=0
for policy in other fifo; do
    echo "===== TASK4 POLICY $policy BEGIN ====="
    /usr/bin/sms_app -r "$policy" -i 100 -d 125 > "/tmp/task4_${policy}.txt" 2>&1
    rc=$?
    echo "===== TASK4 POLICY $policy exit=$rc ====="
    grep '^\[RT_REPORT\]' "/tmp/task4_${policy}.txt" || true
    grep '^\[JITTER_HIST\]' "/tmp/task4_${policy}.txt" || true
    grep '^\[SUMMARY\]' "/tmp/task4_${policy}.txt" || true
    grep -E '^\[WARN\]|^\[ERROR\]|^\[RT\]' "/tmp/task4_${policy}.txt" | tail -8 || true
    n=$(sed -n 's/.*\[RT_REPORT\].* n=\([0-9][0-9]*\).*/\1/p' "/tmp/task4_${policy}.txt" | tail -1)
    if [ "$rc" -ne 0 ] || [ -z "$n" ] || [ "$n" -lt 1000 ]; then
       echo "TASK4_FAIL: policy=$policy exit=$rc samples=${n:-0} (requires >=1000)"
       fail=1
    fi
    if [ "$policy" = fifo ] && ! grep -q '\[RT_REPORT\] requested=FIFO effective=FIFO priority=50' "/tmp/task4_${policy}.txt"; then
       echo 'TASK4_FAIL: SCHED_FIFO priority 50 was NOT verified'
       fail=1
    fi
    if [ "$policy" = other ] && ! grep -q '\[RT_REPORT\] requested=OTHER effective=OTHER priority=0' "/tmp/task4_${policy}.txt"; then
       echo 'TASK4_FAIL: SCHED_OTHER priority 0 was NOT verified'
       fail=1
    fi
done
echo '===== TASK4 END ====='
if [ "$fail" -eq 0 ]; then echo 'TASK4_QEMU_METRICS_PASS'; else echo 'TASK4_QEMU_NEEDS_FIX'; fi
exit "$fail"

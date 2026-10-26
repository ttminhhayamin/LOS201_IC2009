#!/bin/sh
set -u
DEV=/dev/sms_sensor
KO=/lib/modules/5.15.0/sms_sensor.ko
FAIL=0
reload() {
    rmmod sms_sensor 2>/dev/null || true
    insmod "$KO" || return 1
    if [ ! -e "$DEV" ]; then
        d=$(cat /sys/class/sms_class/sms_sensor/dev) || return 1
        mknod "$DEV" c "${d%:*}" "${d#*:}" || return 1
    fi
    [ -r /proc/sms_stats ] || return 1
}
rd() { dd if="$DEV" bs=64 count=1 status=none; }

echo '========== R1: total_bytes_read =========='
reload || exit 1
A=$(awk '$1=="total_bytes_read:" {print $2}' /proc/sms_stats)
n=$(for i in $(seq 10); do rd; sleep 0.2; done | wc -c)
B=$(awk '$1=="total_bytes_read:" {print $2}' /proc/sms_stats)
echo "A=$A bytes_read_by_wc=$n B=$B"
if [ "$B" -eq "$((A+n))" ]; then echo 'R1 PASS'; else echo 'R1 FAIL'; FAIL=1; fi

echo '========== R2: min/max temperature =========='
reload || exit 1
BEFORE=$(grep -E 'min_temperature|max_temperature' /proc/sms_stats)
printf '%s\n' "$BEFORE"
if [ "$(printf '%s\n' "$BEFORE" | grep -c 'N/A')" -ne 2 ]; then echo 'R2 FAIL: expected two N/A'; FAIL=1; fi
data=$(for i in $(seq 20); do rd; sleep 0.2; done)
printf '%s\n' "$data" > /tmp/sms_20_samples.txt
printf '%s\n' "$data" | awk -F, 'NF==3 {if(++n==1){lo=hi=$2+0} if($2+0<lo)lo=$2+0; if($2+0>hi)hi=$2+0} END {printf "AWK: n=%d min=%.2f max=%.2f\n",n,lo,hi}'
cat /proc/sms_stats | grep -E 'min_temperature|max_temperature'
if printf '%s\n' "$data" | awk -F, 'NF==3 {if(++n==1){lo=hi=$2+0} if($2+0<lo)lo=$2+0; if($2+0>hi)hi=$2+0} END{printf "%d %.2f %.2f",n,lo,hi}' | {
    read -r COUNT LO HI
    PMIN=$(awk '$1=="min_temperature:" {print $2}' /proc/sms_stats)
    PMAX=$(awk '$1=="max_temperature:" {print $2}' /proc/sms_stats)
    [ "$COUNT" -eq 20 ] && [ "$LO" = "$PMIN" ] && [ "$HI" = "$PMAX" ]
}; then echo 'R2 PASS'; else echo 'R2 FAIL'; FAIL=1; fi

echo '========== R3: summary on normal exit =========='
/usr/bin/sms_app -d 5 > /tmp/sms_5sec.log 2>&1
RC=$?
tail -8 /tmp/sms_5sec.log
echo "exit=$RC"
SUMMARY=$(grep '^\[SUMMARY\]' /tmp/sms_5sec.log | tail -1)
if [ "$RC" -eq 0 ] && printf '%s\n' "$SUMMARY" | awk '
    /^\[SUMMARY\] produced=[0-9]+ consumed=[0-9]+ size=[0-9]+ overflow=[0-9]+$/ {
       split($2,p,"="); split($3,c,"="); split($4,s,"="); split($5,o,"=");
       if (p[2]==c[2]+s[2] && o[2]==0) good=1;
    } END {exit !good}' ; then
    echo 'R3 NORMAL PASS'
else
    echo 'R3 NORMAL FAIL'; FAIL=1
fi

echo '========== R3: SIGINT MANUAL =========='
echo 'Run: /usr/bin/sms_app -d 0'
echo 'After ~4s press Ctrl+C, check [SUMMARY] and echo "exit=$?" => 0'
echo 'NOTE: Four-number summary must come from circular-buffer getters.'
exit "$FAIL"

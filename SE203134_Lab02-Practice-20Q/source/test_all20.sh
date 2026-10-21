#!/bin/sh

SYS=/sys/class/lab2_class/lab2
DEV=/dev/lab2
PROC=/proc/lab2_info

echo
echo "======================================"
echo " LAB2 - ALL 20 PRACTICE QUESTIONS TEST"
echo "======================================"
echo


echo "=== Q1: read_count ==="
printf clear > $DEV
echo reset > $SYS/reset_stats

printf 'HELLO' > $DEV

BEFORE=$(cat $SYS/read_count)
cat $DEV >/dev/null
AFTER=$(cat $SYS/read_count)

echo "read_count before = $BEFORE"
echo "read_count after  = $AFTER"

if [ "$AFTER" -gt "$BEFORE" ]; then
    echo "Q1 PASS"
else
    echo "Q1 FAIL"
fi

echo


echo "=== Q2: write_count ==="
printf clear > $DEV
echo reset > $SYS/reset_stats

printf 'ABC' > $DEV

grep "Write count" $PROC
echo "Q2: write_count should be > 0"
echo


echo "=== Q3: clear command ==="
printf 'DATA' > $DEV
printf clear > $DEV

echo "buffer_len = $(cat $SYS/buffer_len)"
echo "status     = $(cat $SYS/status)"
echo "Expected: 0 / EMPTY"
echo


echo "=== Q4: default max write = 128 ==="
echo 128 > $SYS/max_size
ioctl_test
echo "Expected write() return = 128"
echo "Expected GET_BUFFER_LEN = 128"
echo


echo "=== Q5: buffer_size ==="
echo "buffer_size = $(cat $SYS/buffer_size)"
echo "Expected: 1024"
echo


echo "=== Q6: status EMPTY / DATA ==="
printf clear > $DEV
echo "After clear: $(cat $SYS/status)"

printf 'DATA' > $DEV
echo "After write: $(cat $SYS/status)"

echo "Expected: EMPTY then DATA"
echo


echo "=== Q7: total_bytes_read ==="
printf clear > $DEV
echo reset > $SYS/reset_stats

printf 'ABCDE' > $DEV
cat $DEV >/dev/null

grep "Total bytes read" $PROC
echo "Expected: 5"
echo


echo "=== Q8: total_bytes_written ==="
printf clear > $DEV
echo reset > $SYS/reset_stats

printf '1234567' > $DEV

grep "Total bytes written" $PROC
echo "Expected: 7"
echo


echo "=== Q9: last write timestamp ==="
grep "Last write time" $PROC
echo "Expected: non-zero Unix timestamp"
echo


echo "=== Q10: version sysfs ==="
echo "version = $(cat $SYS/version)"
echo "Expected: 1.0"
echo


echo "=== Q11: reset statistics ==="
printf 'TEST' > $DEV
cat $DEV >/dev/null

echo "Before reset:"
grep -E "Open count|Read count|Write count|Total bytes" $PROC

echo reset > $SYS/reset_stats

echo "After reset:"
grep -E "Open count|Read count|Write count|Total bytes" $PROC

echo "Expected counters after reset: 0"
echo


echo "=== Q12: thread-safe open_count ==="
printf clear > $DEV
echo reset > $SYS/reset_stats

cat $DEV >/dev/null &
cat $DEV >/dev/null &
cat $DEV >/dev/null &
cat $DEV >/dev/null &
cat $DEV >/dev/null &

wait

echo "open_count = $(cat $SYS/open_count)"
echo "Expected: 5"
echo


echo "=== Q13: mutex read protection ==="
printf clear > $DEV
printf 'MUTEX_TEST' > $DEV

cat $DEV >/dev/null &
cat $DEV >/dev/null &
wait

echo "Concurrent reads completed."
echo "If no crash/corruption occurred, runtime smoke test PASS."
echo


echo "=== Q14: append mode ==="
printf clear > $DEV

printf 'AAA' > $DEV
printf 'BBB' > $DEV

echo "last_data = $(cat $SYS/last_data)"
echo "Expected: AAABBB"
echo


echo "=== Q15: clear with clear\\n ==="
echo clear > $DEV

echo "buffer_len = $(cat $SYS/buffer_len)"
echo "status     = $(cat $SYS/status)"
echo "Expected: 0 / EMPTY"
echo


echo "=== Q16: configurable max_size ==="
echo 64 > $SYS/max_size

echo "max_size = $(cat $SYS/max_size)"

ioctl_test

echo "Expected write() return = 64"
echo "Expected GET_BUFFER_LEN = 64"

echo 128 > $SYS/max_size
echo "Restored max_size = $(cat $SYS/max_size)"
echo


echo "=== Q17: last_write_size ==="
printf clear > $DEV
printf '12345' > $DEV

echo "last_write_size = $(cat $SYS/last_write_size)"
echo "Expected: 5"
echo


echo "=== Q18: extended /proc/lab2_info ==="
cat $PROC
echo


echo "=== Q19: one-shot read ==="
printf clear > $DEV
printf 'ONCE' > $DEV

FIRST="$(cat $DEV)"
SECOND="$(cat $DEV)"

echo "First read  = [$FIRST]"
echo "Second read = [$SECOND]"
echo "Expected first = ONCE"
echo "Expected second = empty"
echo


echo "=== Q20: ioctl ==="
echo 128 > $SYS/max_size

ioctl_test

echo "Expected:"
echo " GET_BUFFER_LEN after write = 128"
echo " Length after CLEAR_BUFFER  = 0"
echo


echo "======================================"
echo " ALL PRACTICE TESTS FINISHED"
echo "======================================"

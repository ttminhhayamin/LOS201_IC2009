#!/bin/sh

MODULE="/lib/modules/5.15.0/counter_driver.ko"
DEVICE="/dev/counter"

CREATED_NODE=0
FAILED=0

echo "======================================"
echo "    ASSIGNMENT 01 - COUNTER TEST"
echo "======================================"

echo ""
echo "[1] Loading counter_driver..."

if grep -q '^counter_driver ' /proc/modules; then
    echo "ERROR: Driver is already loaded."
    exit 1
fi

insmod "$MODULE" || exit 1

echo ""
echo "[2] Creating device node..."

# Misc devices use major 10 and a dynamic minor.
MINOR=$(awk '$2 == "counter" {print $1}' /proc/misc)

if [ -z "$MINOR" ]; then
    echo "FAIL: Counter minor number not found."
    rmmod counter_driver
    exit 1
fi

if [ ! -c "$DEVICE" ]; then
    mknod "$DEVICE" c 10 "$MINOR" || {
        rmmod counter_driver
        exit 1
    }
    CREATED_NODE=1
fi

ls -l "$DEVICE"

echo ""
echo "[3] Checking initial statistics..."

cat /proc/counter_info

echo ""
echo "[4] Testing auto-increment READ..."

A=$(cat "$DEVICE")
B=$(cat "$DEVICE")

echo "First read  : $A"
echo "Second read : $B"

if [ "$A" = "0" ] && [ "$B" = "1" ]; then
    echo "PASS: Auto-increment works."
else
    echo "FAIL: Auto-increment incorrect."
    FAILED=1
fi

echo ""
echo "[5] Testing WRITE 100..."

echo "100" > "$DEVICE"
C=$(cat "$DEVICE")

echo "Read after write: $C"

if [ "$C" = "100" ]; then
    echo "PASS: Counter set to 100."
else
    echo "FAIL: Counter did not become 100."
    FAILED=1
fi

echo ""
echo "[6] Statistics after WRITE..."

cat /proc/counter_info

echo ""
echo "[7] Testing RESET..."

echo "0" > "$DEVICE"
D=$(cat "$DEVICE")

echo "Read after reset: $D"

if [ "$D" = "0" ]; then
    echo "PASS: Reset works."
else
    echo "FAIL: Reset incorrect."
    FAILED=1
fi

echo ""
echo "[8] Statistics after RESET..."

cat /proc/counter_info

echo ""
echo "[9] Testing invalid input..."

if echo "-5" > "$DEVICE" 2>/dev/null; then
    echo "FAIL: Negative value accepted."
    FAILED=1
else
    echo "PASS: Negative value rejected."
fi

if echo "abc" > "$DEVICE" 2>/dev/null; then
    echo "FAIL: Invalid text accepted."
    FAILED=1
else
    echo "PASS: Invalid text rejected."
fi

echo ""
echo "[10] Unloading driver..."

if ! rmmod counter_driver; then
    echo "FAIL: Could not unload driver."
    FAILED=1
fi

if [ "$CREATED_NODE" -eq 1 ]; then
    rm -f "$DEVICE"
fi

if grep -q '^counter_driver ' /proc/modules; then
    echo "FAIL: Module still loaded."
    FAILED=1
fi

if [ -e /proc/counter_info ]; then
    echo "FAIL: procfs entry still exists."
    FAILED=1
fi

echo ""
echo "[11] Kernel log..."

dmesg | grep counter_driver | tail -30

echo ""
echo "======================================"

if [ "$FAILED" -eq 0 ]; then
    echo "   ALL COUNTER TESTS PASSED"
    echo "======================================"
    exit 0
else
    echo "   COUNTER TEST FAILED"
    echo "======================================"
    exit 1
fi

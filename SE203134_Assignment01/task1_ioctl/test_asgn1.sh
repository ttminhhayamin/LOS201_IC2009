#!/bin/sh

# ============================================
# ASSIGNMENT 01 - AUTOMATED DRIVER TEST
# ============================================

MODULE="/lib/modules/5.15.0/asgn1_driver.ko"
DEVICE="/dev/asgn1"
CREATED_NODE=0

echo "======================================"
echo "   ASSIGNMENT 01 - DRIVER TEST"
echo "======================================"

# Step 1: Load kernel module

echo ""
echo "[1] Loading asgn1_driver..."

if grep -q '^asgn1_driver ' /proc/modules; then
    echo "Module already loaded."
else
    insmod "$MODULE" || exit 1
fi

echo "Module loaded successfully."

# Step 2: Create device node

echo ""
echo "[2] Checking device node..."

if [ ! -c "$DEVICE" ]; then
    mknod "$DEVICE" c 241 0 || exit 1
    CREATED_NODE=1
fi

ls -l "$DEVICE"

# Step 3: Verify loaded module

echo ""
echo "[3] Checking loaded modules..."

lsmod | grep asgn1_driver

# Step 4: Run ioctl tests

echo ""
echo "[4] Running ioctl tests..."

test_asgn1
TEST_RESULT=$?

# Step 5: Show kernel logs

echo ""
echo "[5] Checking kernel messages..."

dmesg | grep asgn1_driver | tail -30

# Step 6: Unload module

echo ""
echo "[6] Unloading driver..."

rmmod asgn1_driver
UNLOAD_RESULT=$?

# Remove node only if this script created it
if [ "$CREATED_NODE" -eq 1 ]; then
    rm -f "$DEVICE"
fi

# Step 7: Verify cleanup

echo ""
echo "[7] Checking cleanup..."

if [ "$UNLOAD_RESULT" -ne 0 ]; then
    echo "FAIL: Could not unload module."
    exit 1
fi

if grep -q '^asgn1_driver ' /proc/modules; then
    echo "FAIL: Module still loaded."
    exit 1
fi

if [ -e /proc/asgn1_info ]; then
    echo "FAIL: procfs entry still exists."
    exit 1
fi

echo "Module unloaded successfully."
echo "Cleanup verified."

# Final result

echo ""
echo "======================================"

if [ "$TEST_RESULT" -eq 0 ]; then
    echo "   ALL ASSIGNMENT 01 TESTS PASSED"
    echo "======================================"
    exit 0
else
    echo "   ASSIGNMENT 01 TEST FAILED"
    echo "======================================"
    exit 1
fi

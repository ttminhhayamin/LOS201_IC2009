#!/bin/bash
# Build driver + app (ARM) moi, nhet vao initramfs, san sang cho ./boot_qemu.sh
# Chay:  bash update_initramfs.sh      (can sudo de giai nen/dong goi initramfs)
set -e
P="$(cd "$(dirname "$0")" && pwd)"
IMG="$P/output/initramfs_asgn2_app.cpio.gz"

echo "[1/4] Build driver (ARM)"
( cd "$P/driver" && { make clean >/dev/null 2>&1 || true; } && make )
echo "[2/4] Build app (ARM)"
( cd "$P/app" && make clean && make )
file "$P/app/sms_app" "$P/driver/sms_sensor_driver.ko"

[ -f "$IMG.orig" ] || cp "$IMG" "$IMG.orig"      # sao luu initramfs goc 1 lan

echo "[3/4] Giai nen initramfs, thay sms_app va .ko"
W=$(mktemp -d); cd "$W"
zcat "$IMG.orig" | sudo cpio -idm --quiet
sudo cp "$P/app/sms_app" usr/bin/sms_app && sudo chmod 755 usr/bin/sms_app
sudo cp "$P/driver/sms_sensor_driver.ko" lib/modules/5.15.0/sms_sensor_driver.ko

echo "[4/4] Dong goi lai"
sudo sh -c "find . | cpio -H newc -o -R 0:0 --quiet | gzip > '$IMG'"
ls -lh "$IMG"
sudo rm -rf "$W"
echo "XONG. Tiep theo: ./boot_qemu.sh   (trong QEMU: shell la root, khong can sudo)"

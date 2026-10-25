#!/bin/bash
# LAB-02 Phan C - MTD / nandsim / JFFS2 (chay tren HOST Ubuntu, khong phai QEMU)
# Cach chay (ghi log luon):
#   mkdir -p ~/embedded_lab2/logs
#   sudo bash partC_mtd.sh 2>&1 | tee ~/embedded_lab2/logs/mtd_jffs2.txt
set -x

rmmod nandsim 2>/dev/null          # don dep neu lan truoc con sot
# CHI load nandsim MOT LAN, kem ID (0x20 0x35 = NAND 32MiB, page 512B, erase block 16KiB)
modprobe nandsim first_id_byte=0x20 second_id_byte=0x35 || { echo "KHONG co nandsim (WSL/kernel cloud?) -> dung Ubuntu VM kernel generic"; exit 1; }
modprobe mtdblock
modprobe jffs2
cat /proc/mtd
ls /dev/mtd* /dev/mtdblock*

# Tu tim dung mtd cua "NAND simulator partition 0" (khong gia dinh mtd0)
MTD=$(awk '/NAND simulator partition 0/{sub(":","",$1); print $1}' /proc/mtd)
N=${MTD#mtd}
DEV=/dev/$MTD
BLK=/dev/mtdblock$N
echo "Dung: $DEV / $BLK"

mtdinfo $DEV
mtdinfo -a

# C.3 - xoa, tao anh JFFS2, ghi vao flash
flash_erase $DEV 0 0
rm -rf /tmp/jffs2_data /tmp/rootfs.jffs2
mkdir -p /tmp/jffs2_data/logs
echo 'Hello from JFFS2!' > /tmp/jffs2_data/hello.txt
echo 'Embedded Linux Lab2' > /tmp/jffs2_data/info.txt
date > /tmp/jffs2_data/logs/boot.log
# (khong chen comment giua cac dong co dau \ - se lam hong lenh)
mkfs.jffs2 --root=/tmp/jffs2_data --eraseblock=0x4000 --pagesize=0x200 --no-cleanmarkers --pad --output=/tmp/rootfs.jffs2
ls -lh /tmp/rootfs.jffs2
nandwrite -p $DEV /tmp/rootfs.jffs2

# C.4 - mount, doc, ghi, kiem tra persistence
mkdir -p /mnt/jffs2
mount -t jffs2 $BLK /mnt/jffs2
ls -la /mnt/jffs2/
cat /mnt/jffs2/hello.txt
cat /mnt/jffs2/info.txt
cat /mnt/jffs2/logs/boot.log
sh -c 'echo "Written at $(date)" >> /mnt/jffs2/logs/boot.log'
sh -c 'echo "New runtime file" > /mnt/jffs2/runtime.txt'
cat /mnt/jffs2/logs/boot.log
umount /mnt/jffs2
mount -t jffs2 $BLK /mnt/jffs2
echo "--- SAU KHI UMOUNT/MOUNT LAI (persistence) ---"
cat /mnt/jffs2/logs/boot.log
cat /mnt/jffs2/runtime.txt
umount /mnt/jffs2

# C.5 - dump raw (JFFS2 magic = 0x1985 -> little-endian hien thi "85 19")
nanddump -o -l 512 -f /tmp/dump.bin $DEV
hexdump -C /tmp/dump.bin | head -20

# Cleanup
rmmod mtdblock 2>/dev/null
rmmod nandsim
ls /dev/mtd* 2>/dev/null || echo 'MTD devices removed'

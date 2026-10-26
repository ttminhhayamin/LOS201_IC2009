# SE203134 — Embedded Linux Exam: R1 / R2 / R3

Bộ source tham khảo/tổng hợp từ môi trường thực hành ngày 26/10/2026. Không sửa các thư mục LAB 1/2 gốc.

## Chức năng

- **R1:** `total_bytes_read` là tổng số byte trả về thành công qua `read()`, xuất tại `/proc/sms_stats`.
- **R2:** Lưu min/max nhiệt độ theo đơn vị x100; trước lần đọc đầu tiên trả `N/A`, sau đó hiển thị hai chữ số thập phân.
- **R3:** `sms_app` in `[SUMMARY] produced=<P> consumed=<C> size=<S> overflow=<O>` từ các getter circular buffer sau khi dừng mọi thread; kiểm tra `P = C + S` và `O = 0`.

## File

- `driver/sms_sensor.c`, `driver/sms_sensor.h`, `driver/Makefile`: kernel module ARMv7.
- `app/sms_app.c`, `app/circular_buffer.c`, `app/circular_buffer.h`, `app/Makefile`: userspace đa luồng.
- `check_exam3.sh`: script kiểm thử R1/R2/R3 chạy trong QEMU.
- `boot-qemu.sh`: chạy QEMU bằng kernel có `CONFIG_POSIX_MQUEUE=y`, yêu cầu file `output/zImage_exam3` tại workspace riêng.

## Các bước build trên Ubuntu Host

Giả sử các file trong thư mục `~/embedded_exam3_fresh` tương ứng cấu trúc repo, có kernel LAB 1 tại `~/embedded_lab1/kernel/linux-5.15` và rootfs LAB 2 tại `~/embedded_lab2/rootfs/initramfs`.

```bash
# Kernel QEMU cần POSIX message queues cho mq_open().
cd ~/embedded_lab1/kernel/linux-5.15
./scripts/config --enable POSIX_MQUEUE
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
grep CONFIG_POSIX_MQUEUE .config  # CONFIG_POSIX_MQUEUE=y
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCFLAGS="-march=armv7-a" -j4 zImage

W="$HOME/embedded_exam3_fresh"
mkdir -p "$W/output"
cp arch/arm/boot/zImage "$W/output/zImage_exam3"

make -C "$W/driver" clean
make -C "$W/driver"
make -C "$W/app" arm
file "$W/driver/sms_sensor.ko" "$W/app/sms_app"
modinfo -F vermagic "$W/driver/sms_sensor.ko"
```

Đóng gói trong rootfs độc lập (không ghi đè LAB 2):

```bash
W="$HOME/embedded_exam3_fresh"
ROOT="$W/rootfs/initramfs"
mkdir -p "$ROOT" "$W/output"
sudo cp -a "$HOME/embedded_lab2/rootfs/initramfs/." "$ROOT/"
sudo install -D -m 644 "$W/driver/sms_sensor.ko" "$ROOT/lib/modules/5.15.0/sms_sensor.ko"
sudo install -D -m 755 "$W/app/sms_app" "$ROOT/usr/bin/sms_app"
sudo install -D -m 755 "$W/check_exam3.sh" "$ROOT/usr/bin/check_exam3.sh"
(cd "$ROOT" && sudo find . -print0 | sudo cpio --null -H newc -o) | gzip > "$W/output/initramfs_exam3.cpio.gz"
cd "$W"
bash boot-qemu.sh
```

Trong QEMU (dấu nhắc `/ #`):

```sh
sh /usr/bin/check_exam3.sh
# Test bổ sung bắt buộc: SIGINT
/usr/bin/sms_app -d 0
# nhấn Ctrl+C sau ~4 giây; kiểm tra [SUMMARY]
echo "exit=$?"
```

## Kết quả đã quan sát trong hội thoại

- R1: **PASS**, ví dụ `A=0 bytes_read_by_wc=190 B=190`.
- R2: **PASS**, `AWK: n=20 min=15.07 max=44.43`; procfs khớp.
- R3: **NORMAL PASS**, `[SUMMARY] produced=8 consumed=8 size=0 overflow=0`, `exit=0`.
- R3 SIGINT: **chưa thấy bằng chứng kiểm thử trong hội thoại**, cần chạy `-d 0` rồi Ctrl+C.

## Trạng thái file nhị phân

Không có `driver/sms_sensor.ko` hoặc `app/sms_app` trong commit này: binary đã build thực tế đang nằm trên Ubuntu của người thực hành. Hãy bổ sung bản binary đã xác nhận đúng kiến trúc ARMv7 và kernel `vermagic` từ máy Ubuntu nếu yêu cầu nộp bài.

**Lưu ý:** Repository là public. Chỉ đưa lên các file được phép công khai.

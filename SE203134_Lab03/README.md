# SE203134 — LAB 03 | Debugging Kernel & Performance Analysis

**Người thực hành:** SE203134  
**Đề gốc:** LAB-03 Embedded Linux Systems — Debugging Kernel & Phân tích Hiệu năng, các mục IV–VI.  
**Nguồn dữ liệu:** 2 archive người dùng cung cấp ngày 26/10/2026. Đây là bản phân loại bằng chứng, **không tự bổ sung số liệu hay ảnh**.

## Cấu trúc (file được TÁCH RIÊNG)

- `01_Source/`: mã nguồn `.c` xếp theo Part A/B/C.
- `02_Evidence/`: log, số liệu, biểu đồ **gốc** xếp theo từng bài/tiểu mục.
- `03_Screenshots/`: **34 screenshot liên quan LAB 03**, chia sâu theo A.1–A.9, B.1–B.5, C.4 và D.2–D.4.
- `99_Not_Lab03_Archive/`: **45 screenshot không thuộc LAB 03**, chia riêng LAB 02, Assignment 01 và Assignment 02 Exam; lưu để đảm bảo **không làm mất bất kỳ ảnh nào** trong archive gốc, nhưng **không phải bằng chứng LAB 03**.
- `SCREENSHOT_MANIFEST.csv`: bản đồ 79 file ảnh gốc → file mới, gồm SHA256 để xác thực không bị chỉnh sửa.
- `FILE_SHA256SUMS.txt`: checksum file để kiểm tra sau khi upload.

**Phân loại ảnh:** 34 LAB03 / 45 không liên quan, tổng 79 ảnh.  
**Bằng chứng & source:** 17 file từ archive `.tar.gz`, giữ nguyên byte.  
**Số liệu và ảnh:** không crop, không đổi kích thước; chỉ đổi tên và di chuyển vào thư mục tương ứng.

## Liên hệ với đề LAB03

| Mục trong đề | Evidence | Screenshot | Ghi chú |
|---|---|---|---|
| A.1–A.4 Môi trường, ARM build, QEMU và gdbserver | `01_Source/Part_A_GDB/` | `03_Screenshots/Part_A_GDB/A01_*` → `A04_*` | Có ảnh setup/kết nối lỗi rồi sửa. |
| A.5 GDB từ Host, breakpoints | `02_Evidence/Part_A_GDB/A05_A09_*/gdb_session.txt` | `03_Screenshots/Part_A_GDB/A05_*` | GDB log **chỉ bắt đầu từ breakpoint 2**, không đủ toàn bộ connection. |
| A — Bug 1 `data[5]` out-of-bounds | GDB log | `A06_Bug1_array_out_of_bounds/` | `i=5` với len=5. |
| A — Bug 2 use-after-free | GDB log | `A07_Bug2_use_after_free/` | Con trỏ tồn tại sau free; dereference không hợp lệ. |
| A — Bug 3 integer overflow | GDB log | `A08_Bug3_integer_overflow/` | 100000×100000 = 10^10, int 32-bit tràn. |
| B.1–B.3 Tạo app có leak, Valgrind | `01_Source/Part_B_Valgrind/B01_*/`, `02_Evidence/Part_B_Valgrind/B01_*/` | `03_Screenshots/Part_B_Valgrind/B01_*/` | 16900 B / 250 blocks definitely lost. |
| B.4 Sửa và chạy lại | `01_Source/Part_B_Valgrind/B02_*/`, `02_Evidence/Part_B_Valgrind/B02_*/` | `B02_Fix_and_recheck/` | Cuối cùng `0 errors` (giữa quá trình có lỗi double free, ảnh cũng giữ). |
| B.5 VmRSS/VmSize & maps ARM QEMU | `02_Evidence/Part_B_Valgrind/B03_B04_*/` | `B03_*`, `B04_*` | Dữ liệu VmRSS không phải bằng chứng không leak; Valgrind là bằng chứng chính. |
| C.1 app profiling | `01_Source/Part_C_Profiling/` | Không có screenshot riêng | Source có; `perf_target_profile` bản build riêng không có trong archive. |
| C.2 Host strace | `02_Evidence/Part_C_Profiling/C02_*/` | Không có screenshot riêng | `strace_summary.txt` đủ bảng syscall. |
| C.3 Host perf | `02_Evidence/Part_C_Profiling/C03_*/` | Không có screenshot riêng | `perf_report.txt`, `perf_stat.txt`; không có `perf.data`. |
| C.4 ARM QEMU + strace | `02_Evidence/Part_C_Profiling/C04_*/` | `03_Screenshots/Part_C_Profiling/C04_*/` | Có console trace và ảnh màn hình. |
| D.2 Standard / stress cyclictest | `02_Evidence/Part_D_RealTime/D02_*/` | `03_Screenshots/Part_D_RealTime/D02_*/` | Max bình thường 293µs; under stress 701µs. |
| D.3 Histogram | `02_Evidence/Part_D_RealTime/D03_*/` | `03_Screenshots/Part_D_RealTime/D03_*/` | PNG dữ liệu gốc đã lưu. |
| D.4 Normal vs Stress | `02_Evidence/Part_D_RealTime/D04_*/` | `03_Screenshots/Part_D_RealTime/D04_*/` | Không đo kernel PREEMPT_RT riêng; chỉ so sánh lý thuyết. |

## Các phát hiện được xác minh từ các file được cung cấp

- Valgrind **trước sửa**: `definitely lost: 16,900 bytes in 250 blocks`, `ERROR SUMMARY: 3 errors from 3 contexts`.
- Valgrind **sau sửa**: `All heap blocks were freed`, `ERROR SUMMARY: 0 errors from 0 contexts`.
- `perf report`: 383 samples; top CPU hotspot `__ieee754_log_fma` (42.56%) và `heavy_compute` (42.30%).
- cyclictest **standard**: Max per-thread 124, 16, 257, 293 µs; **stress**: 194, 110, 701, 107 µs.
- GDB log ghi 3 bug nhưng **không bao gồm toàn bộ màn hình từ lúc kết nối**, nên không khẳng định log hoàn chỉnh.

## Các mục chưa có trong hai archive

- **Binary**: `apps/buggy_app`, `apps/leaky_app`, `apps/leaky_app_fixed`, `apps/perf_target` và ARM binaries (archive chỉ có source).
- **Bootable image**: `output/initramfs_lab3.cpio.gz`.
- **Part C data**: `perf.data`, `strace_full.txt` không có; bản `.txt` tóm tắt có.
- **Final report**: `bao_cao/SE203134_Lab03_BaoCao.pdf` chưa được cung cấp.
- **Full GDB transcript** từ thời điểm kết nối chưa được cung cấp.

Không đánh dấu bản bàn giao này là gói nộp hoàn chỉnh. Các file thiếu cần lấy từ máy Ubuntu nếu giảng viên yêu cầu.

## Lệnh kiểm tra sau khi clone

```bash
cd SE203134_Lab03
sha256sum -c FILE_SHA256SUMS.txt
```

## Lưu ý khi upload repo public

`99_Not_Lab03_Archive` gồm ảnh từ bài khác theo yêu cầu giữ tất cả ảnh. Không viện dẫn những ảnh này như bằng chứng LAB03. Hãy kiểm tra repo public trước khi chia sẻ.

## Bổ sung ngày 26/10/2026 (bản có báo cáo)

- `bao_cao/SE203134_Lab03_BaoCao.pdf`: báo cáo 13 trang, tóm tắt A–D và trả lời 5 câu hỏi phân tích, **cần điền họ tên**.
- `03_Screenshots/Part_A_GDB/A09_GDB_log/79_A09_GDB_session_full_new_2026-10-26_19-18.png`: màn hình xác nhận đã tạo GDB session full.
- Screenshot mới **chỉ xác nhận file log tồn tại trên Ubuntu**; nội dung `gdb_session_full.txt` vẫn phải copy từ Ubuntu để nộp.
- Tổng ảnh trong bản đóng gói này: **80 (35 thuộc LAB03 và 45 ảnh lưu trữ khác)**; 17 file source/evidence ban đầu giữ nguyên.

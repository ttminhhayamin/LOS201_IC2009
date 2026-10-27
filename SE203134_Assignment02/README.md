# SE203134 — Assignment 02: Sensor Monitoring System

**Student:** Trần Tuấn Minh (SE203134)  
**Course:** Embedded Linux Systems — Assignment 02  
**Target:** Linux 5.15, ARMv7 / QEMU `vexpress-a9`; Ubuntu x86-64 for Valgrind, GDB and profiling  
**Source state:** Final 500 ms sampling retry version (2026-10-27)

## Contents

| Directory | Purpose |
| --- | --- |
| `01_Source/Application/` | Six application `.c` sources, `.h` headers and the ARM/x86 Makefile |
| `01_Source/Kernel_Driver/` | `sms_sensor` character-driver source, header and Makefile |
| `02_Evidence/Task_01_Sensor_Monitoring/` | Basic functionality and refactoring run logs |
| `02_Evidence/Task_02_Memory_and_Driver/` | Valgrind, three memory snapshots, ioctl/read tests, procfs stats |
| `02_Evidence/Task_03_Debugging_and_Profiling/` | GDB sessions, strace summaries, actual/supplementary perf results and raw captures |
| `02_Evidence/Task_04_RealTime_Jitter/` | SCHED_OTHER/FIFO benchmark, jitter stats and QEMU output |
| `02_Evidence/Final_Reliability_Fixes/` | Shutdown, rotation, injected error and profiler-retry evidence |
| `02_Evidence/Selected_Screenshots/` | Representative screenshots from tests in the conversation |
| `03_Binaries/` | ARM modules/app/tests and x86 binaries (including mock backend variants) |
| `04_QEMU/` | Final initramfs, with earlier images kept separately for provenance |
| `05_Final_Report/` | Final 28-page report, PDF and editable Word version |
| `06_Demo/` | Video status and recording instructions |
| `07_Test_and_Build_Tools/` | Task 2/3/4 scripts, mock sensor and fault-injection harness |
| `99_Build_Artifacts/` | Compiler-generated kernel module artifacts, retained separately |

## Implementation summary

- **Task 1:** Sensor, Processor, Alert, Logger threads; mutex/condition variable circular buffer; POSIX message queues; timestamped logging, rotation, SIGINT shutdown and alarm counters.
- **Task 2:** Valgrind reports no memory leaks; VmSize and VmRSS measured at 5, 30 and 55 seconds; ioctl and read/EOF/EAGAIN integration tests passed.
- **Task 3:** GDB and strace profiling; live `perf` recording has only **one sample**, insufficient for a reliable hotspot ranking. The separate CPU-intensive hotpath benchmark is clearly labeled as *supplementary*, not live-app data.
- **Task 4:** 100 ms benchmark: OTHER P99 9,939 µs, FIFO P99 9,736 µs (2.04% decrease); both collected at least 1,000 samples. This is a **100 ms test**, not the Task 1 500 ms sampling demonstration.
- **Final 500 ms test:** **116 produced / 116 consumed**, size=0, overflow=0, uptime 59.54 seconds; driver read delta=116. Evidence shown in selected screenshot and final PDF. The original unabridged **text log for this final test is not in the uploaded archive**.

## Quick build (Ubuntu host)

The source paths in `01_Source/` are for convenient review, not the original build workspace. Copy application and driver directories into the usual sibling `app/` and `driver/` layout before compiling, because `sms_app.c` includes `../driver/sms_sensor.h`.

```bash
mkdir -p ~/embedded_asgn2_rebuild/{app,driver}
cp -a 01_Source/Application/. ~/embedded_asgn2_rebuild/app/
cp -a 01_Source/Kernel_Driver/. ~/embedded_asgn2_rebuild/driver/
cd ~/embedded_asgn2_rebuild/app
make host
make arm
```

Building the kernel module requires a configured Linux 5.15 ARM kernel build tree at the path specified in its Makefile. Run `make` from `driver/` only after verifying the toolchain/kernel tree.

## Running on ARM QEMU

The included `04_QEMU/Current_Boot_Image/initramfs_asgn2.cpio.gz` is the final bundled rootfs. The QEMU `zImage` and DTB are **not inside this archive**; the original developer host used `~/embedded_exam3_fresh/output/zImage_exam3` and `~/embedded_lab1/output/vexpress-v2p-ca9.dtb`.

Start QEMU with the matching ARM kernel and DTB, load `/lib/modules/5.15.0/sms_sensor.ko` if needed, then run:

```sh
/usr/bin/sms_app -i 500 -t 40 -h 80 -d 60 -v
cat /proc/sms_stats
tail /tmp/sms.log
```

## Integrity and provenance

`SOURCE_TO_DESTINATION.csv` traces each source-archive file and copied final report/screenshot to its sorted destination. `SHA256SUMS.txt` allows checking file integrity after transfer:

```bash
(cd SE203134_Assignment02 && sha256sum -c SHA256SUMS.txt)
```

**Submission note:** This is the *organized GitHub copy*, not necessarily the exact LMS submission directory layout. See `SUBMISSION_CHECKLIST.md` for missing demo and official submission requirements.

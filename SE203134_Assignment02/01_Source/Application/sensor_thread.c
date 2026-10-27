/* Assignment 02 sensor_thread | SE203134 | 2026-10-27 */
#include "sms_shared.h"
#include "sensor_thread.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Task 4 / SE203134: absolute periodic sleep; jitter measured at wake-up.
 * The single sensor thread writes samples; main reads stats only after pthread_join.
 */
void *sensor_thread(void *unused)
{
    (void)unused;
    struct timespec deadline, actual;
    while (!atomic_load(&sensor_run_gate) && !atomic_load(&stop_requested))
        ms_sleep(5);
    if (atomic_load(&stop_requested)) return NULL;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        perror("clock_gettime sensor");
        atomic_store(&runtime_error,1);
        atomic_store(&stop_requested,1);
        return NULL;
    }
    while (!atomic_load(&stop_requested)) {
        deadline.tv_nsec += (long)(sample_ms % 1000u) * 1000000L;
        deadline.tv_sec += (time_t)(sample_ms / 1000u) + deadline.tv_nsec / 1000000000L;
        deadline.tv_nsec %= 1000000000L;

        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
        } while (rc == EINTR && !atomic_load(&stop_requested) && !sigint_requested);
        if (atomic_load(&stop_requested) || sigint_requested) break;
        if (rc != 0) {
            errno = rc;
            perror("clock_nanosleep sensor");
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &actual) != 0) {
            perror("clock_gettime wake");
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
        long delta = (long)(actual.tv_sec - deadline.tv_sec) * 1000000L
            + (long)(actual.tv_nsec - deadline.tv_nsec) / 1000L;
        if (delta < 0) delta = 0;
        if (jitter_count < JITTER_CAP) jitter_values_us[jitter_count++] = delta;

        int fd = open(DEVICE_PATH, O_RDONLY);
        char line[128];
        ssize_t n;
        if (fd < 0) {
            perror("sensor open");
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
        /* SE203134_EAGAIN_RETRY_FIX_V2 */
        /* Absolute wake-ups can precede the driver's rate-limit gate.
         * Retry transient EAGAIN/EINTR on the SAME open descriptor.
         * Bound the extra wait to 200 ms or half the sampling interval,
         * whichever is smaller. Avoid busy-spinning and honor shutdown.
         */
        int saved_errno = 0;
        unsigned int retry_waited_ms = 0;
        const unsigned int max_retry_ms = sample_ms / 2u < 200u
                                             ? sample_ms / 2u : 200u;
        for (;;) {
            n = read(fd, line, sizeof(line)-1);
            saved_errno = n < 0 ? errno : 0;
            if (n >= 0 || (saved_errno != EAGAIN && saved_errno != EINTR)
                || atomic_load(&stop_requested) || sigint_requested
                || retry_waited_ms >= max_retry_ms)
                break;
            ms_sleep(5);
            retry_waited_ms += 5;
        }
        close(fd);
        if (n > 0) {
            SensorData x;
            line[n] = '\0';
            if (sscanf(line, "%" SCNu64 ",%lf,%lf", &x.timestamp_ms,
                       &x.temperature, &x.humidity) == 3) {
                int pushed = cb_push(&shared_buffer, x);
                if (verbose) printf("[SENSOR] %s", line);
                if (pushed == 0)
                    fprintf(stderr, "[WARN] circular buffer overflow, sample dropped\n");
            }
        } else if (n < 0 && saved_errno != EAGAIN && saved_errno != EINTR) {
            errno = saved_errno;
            perror("sensor read");
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
    }
    return NULL;
}

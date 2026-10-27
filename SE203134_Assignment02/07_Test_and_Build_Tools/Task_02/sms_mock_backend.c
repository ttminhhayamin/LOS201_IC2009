/* Test-only x86 sensor backend using linker --wrap; real ARM app untouched.
 * Author: Tran Tuan Minh / SE203134. 2026-10-27.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "sms_sensor.h"

#define MAX_TRACK_FDS 8192
static pthread_mutex_t fake_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char marked[MAX_TRACK_FDS];
static unsigned char already_read[MAX_TRACK_FDS];
static unsigned int sample_interval = 500;
static uint32_t seed = 123456789u;

extern int __real_open(const char *, int, ...);
extern ssize_t __real_read(int, void *, size_t);
extern int __real_close(int);
extern int __real_ioctl(int, unsigned long, ...);
static int is_fake(int fd) {
    int yes = 0;
    pthread_mutex_lock(&fake_lock);
    if (fd >= 0 && fd < MAX_TRACK_FDS) yes = marked[fd];
    pthread_mutex_unlock(&fake_lock);
    return yes;
}
int __wrap_open(const char *path, int flags, ...) {
    int fd;
    if (!strcmp(path, "/dev/sms_sensor")) {
        fd = __real_open("/dev/null", O_RDONLY);
        if (fd >= 0 && fd < MAX_TRACK_FDS) {
            pthread_mutex_lock(&fake_lock);
            marked[fd] = 1;
            already_read[fd] = 0;
            pthread_mutex_unlock(&fake_lock);
        } else if (fd >= MAX_TRACK_FDS) {
            __real_close(fd);
            errno = EMFILE;
            return -1;
        }
        return fd;
    }
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = (mode_t)va_arg(args, int);
        va_end(args);
        return __real_open(path, flags, mode);
    }
    return __real_open(path, flags);
}
ssize_t __wrap_read(int fd, void *buffer, size_t count) {
    char line[100];
    size_t n;
    struct timespec ts;
    unsigned int t, h;
    if (!is_fake(fd)) return __real_read(fd, buffer, count);
    pthread_mutex_lock(&fake_lock);
    if (already_read[fd]) {
        pthread_mutex_unlock(&fake_lock);
        return 0;
    }
    already_read[fd] = 1;
    seed = seed * 1664525u + 1013904223u;
    t = 1500 + seed % 3001;
    seed = seed * 1664525u + 1013904223u;
    h = 3000 + seed % 6001;
    pthread_mutex_unlock(&fake_lock);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long now = (unsigned long long)ts.tv_sec * 1000ull +
                             (unsigned long long)ts.tv_nsec / 1000000ull;
    n = (size_t)snprintf(line, sizeof(line), "%llu,%u.%02u,%u.%02u\n",
                         now, t/100, t%100, h/100, h%100);
    if (n > count) n = count;
    memcpy(buffer, line, n);
    return (ssize_t)n;
}
int __wrap_ioctl(int fd, unsigned long request, ...) {
    va_list args;
    void *arg;
    va_start(args, request);
    arg = va_arg(args, void *);
    va_end(args);
    if (!is_fake(fd)) return __real_ioctl(fd, request, arg);
    if (request != (unsigned long)SMS_SET_INTERVAL) {
        errno = ENOTTY;
        return -1;
    }
    unsigned int ms = *(unsigned int *)arg;
    if (ms == 0 || ms > 60000) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&fake_lock);
    sample_interval = ms;
    pthread_mutex_unlock(&fake_lock);
    return 0;
}
int __wrap_close(int fd) {
    pthread_mutex_lock(&fake_lock);
    if (fd >= 0 && fd < MAX_TRACK_FDS) {
        marked[fd] = 0;
        already_read[fd] = 0;
    }
    pthread_mutex_unlock(&fake_lock);
    return __real_close(fd);
}

/* SMS 4-thread sensor monitor: producer/processor/alert/logger.
 * Author: Tran Tuan Minh - SE203134 | Date: 2026-10-26
 * Userland supports floats. Kernel keeps fixed point integers x100.
 */
#define _POSIX_C_SOURCE 200809L
#include "sms_shared.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"
#include "../driver/sms_sensor.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
/* SE203134_TASK4_PATCH_v1 */


CircularBuffer shared_buffer;
mqd_t alert_queue = (mqd_t)-1, log_queue = (mqd_t)-1;
char alert_name[64], log_name[64];
volatile sig_atomic_t sigint_requested;
atomic_int stop_requested;
atomic_int processor_done;
atomic_int alert_done;
atomic_int runtime_error;
atomic_int log_consumer_failed, alert_consumer_failed;
unsigned int sample_ms = 500, duration_sec = 60;
double temp_threshold = 40.0, humid_threshold = 80.0;
int verbose;
atomic_int sensor_run_gate;
long jitter_values_us[JITTER_CAP];
size_t jitter_count;
int rt_mode; /* 0=default (legacy), 1=OTHER, 2=FIFO50 */
unsigned int alert_temp_high, alert_humid_high, alert_spike;

static void on_sigint(int signo) { (void)signo; sigint_requested = 1; }

void ms_sleep(unsigned int millis)
{
    struct timespec req = {(time_t)(millis/1000), (long)(millis%1000)*1000000L};
    while (nanosleep(&req, &req) < 0 && errno == EINTR && !atomic_load(&stop_requested) && !sigint_requested) { }
}

/* SE203134_FINAL_FIX_V1: bounded send prevents hang after a consumer fails.
 * The 2-second deadline is an error threshold, not normal polling.
 */
int mq_send_text(mqd_t q, const char *s)
{
    if (q == (mqd_t)-1) {errno=EBADF; atomic_store(&runtime_error,1);return -1;}
    if ((q==log_queue && atomic_load(&log_consumer_failed)) ||
        (q==alert_queue && atomic_load(&alert_consumer_failed))) {
        errno=EPIPE; atomic_store(&runtime_error,1);return -1;
    }
    struct timespec until;
    if (clock_gettime(CLOCK_REALTIME,&until)!=0) goto error;
    until.tv_sec += 2;
    for (;;) {
        if (mq_timedsend(q,s,strlen(s)+1,0,&until)==0) return 0;
        if (errno==EINTR) continue;
        break;
    }
error:
    perror("mq_timedsend");
    if (q==log_queue) atomic_store(&log_consumer_failed,1);
    if (q==alert_queue) atomic_store(&alert_consumer_failed,1);
    atomic_store(&runtime_error,1);
    atomic_store(&stop_requested,1);
    return -1;
}
/* STOP messages have priority as the final item AFTER all producers have joined.
 * A cancellation fallback is only for the fatal path when STOP cannot be sent.
 */
static void stop_consumer(mqd_t q,pthread_t tid)
{
    if (mq_send_text(q,SMS_QUEUE_STOP_MSG)!=0) {
        atomic_store(&runtime_error,1);
        (void)pthread_cancel(tid);
    }
}






static int compare_jitter(const void *aa, const void *bb)
{
    long a = *(const long *)aa, b = *(const long *)bb;
    return (a > b) - (a < b);
}

static void print_jitter_report(int requested, const char *effective,
                                int effective_prio, const char *mlock_status)
{
    size_t n = jitter_count, hist[4] = {0};
    long double sum = 0;
    for (size_t i = 0; i < n; i++) {
        long j = jitter_values_us[i];
        sum += j;
        if (j < 100) hist[0]++;
        else if (j < 500) hist[1]++;
        else if (j < 1000) hist[2]++;
        else hist[3]++;
    }
    qsort(jitter_values_us, n, sizeof(jitter_values_us[0]), compare_jitter);
    const char *wanted = requested == 2 ? "FIFO" : requested == 1 ? "OTHER" : "DEFAULT";
    size_t idx99 = n ? (99*n + 99)/100 - 1 : 0;
    printf("[RT_REPORT] requested=%s effective=%s priority=%d mlock=%s "
           "period_ms=%u n=%zu min_us=%ld avg_us=%.2Lf max_us=%ld p99_us=%ld\n",
           wanted, effective, effective_prio, mlock_status, sample_ms, n,
           n ? jitter_values_us[0] : 0, n ? sum/n : 0.0L,
           n ? jitter_values_us[n-1] : 0, n ? jitter_values_us[idx99] : 0);
    printf("[JITTER_HIST] 0-99us=%zu 100-499us=%zu 500-999us=%zu >=1000us=%zu\n",
           hist[0], hist[1], hist[2], hist[3]);
    fflush(stdout);
}

static void usage(const char *prog)
{
    fprintf(stderr,"Usage: %s [-i ms] [-t C] [-h humidity] [-d sec] [-v] [-r fifo|other]\n",prog);
}

int main(int argc, char **argv)
{
    int opt, fd, status=0;
    const char *mlock_status = "not_requested";
    pthread_t sensor, processor, alert, logger;
    struct mq_attr attr={.mq_flags=0, .mq_maxmsg=MSG_MAX, .mq_msgsize=MQ_TEXT_SIZE, .mq_curmsgs=0};
    while ((opt=getopt(argc,argv,"i:t:h:d:vr:"))!=-1) {
        char *end;
        switch(opt) {
        case 'i': {unsigned long v=strtoul(optarg,&end,10);
            if (*end || v<100 || v>60000) {usage(argv[0]);return 2;}
            sample_ms=(unsigned int)v; break;}
        case 't': temp_threshold=strtod(optarg,&end); if (*end){usage(argv[0]);return 2;} break;
        case 'h': humid_threshold=strtod(optarg,&end); if (*end){usage(argv[0]);return 2;} break;
        case 'd': {unsigned long v=strtoul(optarg,&end,10);
            if (*end || v>86400) {usage(argv[0]);return 2;}
            duration_sec=(unsigned int)v;break;}
        case 'v': verbose=1;break;
        case 'r':
            if (!strcmp(optarg,"fifo")) rt_mode=2;
            else if (!strcmp(optarg,"other")) rt_mode=1;
            else {usage(argv[0]);return 2;}
            break;
        default: usage(argv[0]);return 2;
        }
    }
    fd=open(DEVICE_PATH,O_RDONLY);
    if (fd<0) {perror("open " DEVICE_PATH);return 1;}
    if (ioctl(fd,SMS_SET_INTERVAL,&sample_ms)<0) {
        perror("ioctl SMS_SET_INTERVAL");close(fd);return 1;
    }
    close(fd);
    if (cb_init(&shared_buffer,BUFCAP)) {perror("cb_init");return 1;}
    snprintf(alert_name,sizeof(alert_name),"/sms_alert_%ld",(long)getpid());
    snprintf(log_name,sizeof(log_name),"/sms_log_%ld",(long)getpid());
    alert_queue=mq_open(alert_name,O_CREAT|O_RDWR,0600,&attr);
    log_queue=mq_open(log_name,O_CREAT|O_RDWR,0600,&attr);
    if (alert_queue==(mqd_t)-1 || log_queue==(mqd_t)-1) {
        perror("mq_open (verify CONFIG_POSIX_MQUEUE)"); status=1;goto cleanup;
    }
    struct sigaction sa={0};sa.sa_handler=on_sigint;
    sigemptyset(&sa.sa_mask);sigaction(SIGINT,&sa,NULL);
    /* RT benchmark modes request page locking before creating worker threads.
     * The guest may require a larger RLIMIT_MEMLOCK. Always report the outcome.
     */
    if (rt_mode) {
        struct rlimit unlimited = {RLIM_INFINITY, RLIM_INFINITY};
        if (setrlimit(RLIMIT_MEMLOCK, &unlimited) != 0) {
            perror("[WARN] setrlimit RLIMIT_MEMLOCK");
            mlock_status = "FAILED"; /* Avoid MCL_FUTURE causing pthread_create ENOMEM under a tiny limit */
        } else if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {
            mlock_status = "OK";
        } else {
            mlock_status = "FAILED";
            perror("[WARN] mlockall");
        }
    }
    /* Threads started in reverse dependency order. */
    if (pthread_create(&logger,NULL,logger_thread,NULL)) {status=1;goto cleanup;}
    if (pthread_create(&alert,NULL,alert_thread,NULL)) {
        status=1;
        /* Alert never started; send stop directly to the logger. */
        atomic_store(&alert_done,1);
        stop_consumer(log_queue,logger);
        pthread_join(logger,NULL);
        goto cleanup;
    }
    if (pthread_create(&processor,NULL,processor_thread,NULL)) {
        status=1;
        atomic_store(&processor_done,1);
        stop_consumer(alert_queue,alert);
        pthread_join(alert,NULL);
        atomic_store(&alert_done,1);
        stop_consumer(log_queue,logger);
        pthread_join(logger,NULL);
        goto cleanup;
    }
    if (pthread_create(&sensor,NULL,sensor_thread,NULL)) {
        status=1;atomic_store(&stop_requested,1);cb_close(&shared_buffer);
        pthread_join(processor,NULL);
        atomic_store(&processor_done,1);
        stop_consumer(alert_queue,alert);
        pthread_join(alert,NULL);
        atomic_store(&alert_done,1);
        stop_consumer(log_queue,logger);
        pthread_join(logger,NULL);
        goto cleanup;
    }
    struct sched_param sp={.sched_priority = rt_mode == 2 ? 50 : rt_mode == 1 ? 0 : 10};
    int target_policy = rt_mode == 1 ? SCHED_OTHER : SCHED_FIFO;
    int prc = pthread_setschedparam(sensor, target_policy, &sp);
    if (prc) {
        fprintf(stderr,"[RT] sched request failed: %s\n",strerror(prc));
        if (rt_mode) { status = 1; atomic_store(&stop_requested,1); }
    }
    struct sched_param effective_sp={0};
    int effective_policy = -1;
    int grc = pthread_getschedparam(sensor, &effective_policy, &effective_sp);
    if (grc) { fprintf(stderr,"[RT] cannot query effective policy: %s\n",strerror(grc)); status=1; }
    const char *effective_name = effective_policy == SCHED_FIFO ? "FIFO" :
        effective_policy == SCHED_OTHER ? "OTHER" : "UNKNOWN";
    printf("[RT] requested=%s effective=%s priority=%d mlock=%s\n",
        rt_mode == 2 ? "FIFO" : rt_mode == 1 ? "OTHER" : "DEFAULT",
        effective_name, effective_sp.sched_priority, mlock_status);
    fflush(stdout);
    atomic_store(&sensor_run_gate,1);
    struct timespec started_mono, ended_mono;
    clock_gettime(CLOCK_MONOTONIC, &started_mono);
    time_t start=time(NULL);
    while (!atomic_load(&stop_requested) && !sigint_requested && (!duration_sec || (unsigned int)(time(NULL)-start)<duration_sec))
        ms_sleep(100);
    atomic_store(&stop_requested,1);
    pthread_join(sensor,NULL);
    cb_close(&shared_buffer); /* wake processor, then drain remaining items */
    pthread_join(processor,NULL);
    atomic_store(&processor_done,1);
    /* No more producers for alert_queue: drain, then stop Alert. */
    stop_consumer(alert_queue,alert);
    pthread_join(alert,NULL);
    atomic_store(&alert_done,1);
    /* No more producers for log_queue: drain, then stop Logger. */
    stop_consumer(log_queue,logger);
    pthread_join(logger,NULL);
    clock_gettime(CLOCK_MONOTONIC, &ended_mono);
    /* Exam R3: strictly after ALL joins, read getter values (no hardcode). */
    printf("[SUMMARY] produced=%" PRIu64 " consumed=%" PRIu64
           " size=%zu overflow=%" PRIu64 "\n",
           cb_get_total_produced(&shared_buffer),
           cb_get_total_consumed(&shared_buffer),
           cb_get_current_size(&shared_buffer),
           cb_get_overflow_count(&shared_buffer));
    printf("[SUMMARY] alerts_total=%u TEMP_HIGH=%u HUMID_HIGH=%u TEMP_SPIKE=%u uptime_sec=%.2f\n",
           alert_temp_high + alert_humid_high + alert_spike,
           alert_temp_high, alert_humid_high, alert_spike,
           (double)(ended_mono.tv_sec - started_mono.tv_sec) +
           (double)(ended_mono.tv_nsec - started_mono.tv_nsec) / 1000000000.0);
    print_jitter_report(rt_mode, effective_name, effective_sp.sched_priority, mlock_status);
    fflush(stdout);
cleanup:
    if (alert_queue!=(mqd_t)-1) mq_close(alert_queue);
    if (log_queue!=(mqd_t)-1) mq_close(log_queue);
    mq_unlink(alert_name);mq_unlink(log_name);
    cb_destroy(&shared_buffer);
    if (rt_mode && !strcmp(mlock_status, "OK")) munlockall();
    return (status || atomic_load(&runtime_error)) ? 1 : 0;
}

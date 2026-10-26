/* SMS 4-thread sensor monitor: producer/processor/alert/logger.
 * Author: Tran Tuan Minh - SE203134 | Date: 2026-10-26
 * Userland supports floats. Kernel keeps fixed point integers x100.
 */
#define _POSIX_C_SOURCE 200809L
#include "circular_buffer.h"
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

#define DEVICE_PATH "/dev/sms_sensor"
#define LOG_PATH "/tmp/sms.log"
#define BUFCAP 64
#define MQ_TEXT_SIZE 192
#define MSG_MAX 10

static CircularBuffer shared_buffer;
static mqd_t alert_queue = (mqd_t)-1, log_queue = (mqd_t)-1;
static char alert_name[64], log_name[64];
static volatile sig_atomic_t sigint_requested;
static atomic_int stop_requested;
static atomic_int processor_done;
static atomic_int alert_done;
static atomic_int runtime_error;
static unsigned int sample_ms = 500, duration_sec = 60;
static double temp_threshold = 40.0, humid_threshold = 80.0;
static int verbose;
static unsigned int alert_temp_high, alert_humid_high, alert_spike;

static void on_sigint(int signo) { (void)signo; sigint_requested = 1; }

static void ms_sleep(unsigned int millis)
{
    struct timespec req = {(time_t)(millis/1000), (long)(millis%1000)*1000000L};
    while (nanosleep(&req, &req) < 0 && errno == EINTR && !atomic_load(&stop_requested) && !sigint_requested) { }
}

static void mq_send_text(mqd_t q, const char *s)
{
    if (q != (mqd_t)-1 && mq_send(q, s, strlen(s)+1, 0) && errno != EAGAIN)
        perror("mq_send");
}

static void *sensor_thread(void *unused)
{
    (void)unused;
    while (!atomic_load(&stop_requested)) {
        int fd = open(DEVICE_PATH, O_RDONLY);
        char line[128];
        ssize_t n;
        if (fd < 0) { perror("sensor open"); atomic_store(&runtime_error,1); atomic_store(&stop_requested,1); break; }
        n = read(fd, line, sizeof(line)-1);
        close(fd);
        if (n > 0) {
            SensorData x;
            line[n] = '\0';
            if (sscanf(line, "%" SCNu64 ",%lf,%lf", &x.timestamp_ms,
                       &x.temperature, &x.humidity) == 3) {
                int pushed = cb_push(&shared_buffer, x);
                if (verbose)
                    printf("[SENSOR] %s", line);
                if (pushed == 0)
                    fprintf(stderr, "[WARN] circular buffer overflow, sample dropped\n");
            }
        } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
            perror("sensor read");
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
        /* Sleep in short increments to react promptly to Ctrl+C. */
        for (unsigned int elapsed=0; elapsed<sample_ms && !atomic_load(&stop_requested); elapsed+=100)
            ms_sleep((sample_ms-elapsed < 100) ? sample_ms-elapsed : 100);
    }
    return NULL;
}

static void *processor_thread(void *unused)
{
    (void)unused;
    SensorData x, last={0};
    int have_last=0;
    double temp_history[10]={0}, humid_history[10]={0};
    unsigned int index=0, filled=0;
    while (cb_pop(&shared_buffer, &x)) {
        char msg[MQ_TEXT_SIZE];
        temp_history[index]=x.temperature;
        humid_history[index]=x.humidity;
        index=(index+1)%10;
        if (filled<10) filled++;
        double avg_t=0, avg_h=0;
        for (unsigned int k=0;k<filled;k++) {
            avg_t+=temp_history[k]; avg_h+=humid_history[k];
        }
        avg_t/=filled; avg_h/=filled;
        snprintf(msg, sizeof(msg), "[DATA][%" PRIu64 "] temp=%.2f humid=%.2f avg_t=%.2f avg_h=%.2f",
                 x.timestamp_ms,x.temperature,x.humidity,avg_t,avg_h);
        mq_send_text(log_queue,msg);
        if (x.temperature > temp_threshold) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] TEMP_HIGH: VALUE=%.2f THRESHOLD=%.2f",
                     x.timestamp_ms,x.temperature,temp_threshold);
            mq_send_text(alert_queue,msg);
        }
        if (x.humidity > humid_threshold) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] HUMID_HIGH: VALUE=%.2f THRESHOLD=%.2f",
                     x.timestamp_ms,x.humidity,humid_threshold);
            mq_send_text(alert_queue,msg);
        }
        if (have_last && (x.temperature-last.temperature > 5.0 ||
                          last.temperature-x.temperature > 5.0)) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] TEMP_SPIKE: VALUE=%.2f THRESHOLD=5.00",
                     x.timestamp_ms, x.temperature);
            mq_send_text(alert_queue,msg);
        }
        last=x; have_last=1;
    }
    return NULL;
}

static void *alert_thread(void *unused)
{
    (void)unused;
    char msg[MQ_TEXT_SIZE];
    for (;;) {
        ssize_t n=mq_receive(alert_queue,msg,sizeof(msg),NULL);
        if (n >= 0) {
            msg[(n<(ssize_t)sizeof(msg))?(size_t)n:sizeof(msg)-1]='\0';
            fprintf(stderr,"%s\n",msg);
            if (strstr(msg,"TEMP_HIGH")) alert_temp_high++;
            else if (strstr(msg,"HUMID_HIGH")) alert_humid_high++;
            else if (strstr(msg,"TEMP_SPIKE")) alert_spike++;
            mq_send_text(log_queue,msg);
        } else if (errno==EAGAIN) {
            if (atomic_load(&processor_done)) break;
            ms_sleep(50);
        } else if (errno!=EINTR) {perror("alert mq_receive"); break;}
    }
    return NULL;
}

static void *logger_thread(void *unused)
{
    (void)unused;
    FILE *out=fopen(LOG_PATH,"a");
    if (!out) {perror("open log"); return NULL;}
    char msg[MQ_TEXT_SIZE];
    time_t last_flush=time(NULL);
    for (;;) {
        ssize_t n=mq_receive(log_queue,msg,sizeof(msg),NULL);
        if (n>=0) {
            msg[(n<(ssize_t)sizeof(msg))?(size_t)n:sizeof(msg)-1]='\0';
            fprintf(out,"%s\n",msg);
            if (ftell(out)>1024*1024) {
                fclose(out);
                rename(LOG_PATH,"/tmp/sms.log.1");
                out=fopen(LOG_PATH,"w");
                if (!out) {perror("rotate log"); return NULL;}
            }
        } else if (errno==EAGAIN) {
            if (atomic_load(&alert_done)) break;
            ms_sleep(50);
        } else if (errno!=EINTR) {perror("logger mq_receive"); break;}
        if (time(NULL)-last_flush>=5) {fflush(out);last_flush=time(NULL);}
    }
    fflush(out);
    fclose(out);
    return NULL;
}

static void usage(const char *prog)
{
    fprintf(stderr,"Usage: %s [-i ms] [-t C] [-h humidity] [-d sec] [-v]\n",prog);
}

int main(int argc, char **argv)
{
    int opt, fd, status=0;
    pthread_t sensor, processor, alert, logger;
    struct mq_attr attr={.mq_flags=0, .mq_maxmsg=MSG_MAX, .mq_msgsize=MQ_TEXT_SIZE, .mq_curmsgs=0};
    while ((opt=getopt(argc,argv,"i:t:h:d:v"))!=-1) {
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
    alert_queue=mq_open(alert_name,O_CREAT|O_RDWR|O_NONBLOCK,0600,&attr);
    log_queue=mq_open(log_name,O_CREAT|O_RDWR|O_NONBLOCK,0600,&attr);
    if (alert_queue==(mqd_t)-1 || log_queue==(mqd_t)-1) {
        perror("mq_open (verify CONFIG_POSIX_MQUEUE)"); status=1;goto cleanup;
    }
    struct sigaction sa={0};sa.sa_handler=on_sigint;
    sigemptyset(&sa.sa_mask);sigaction(SIGINT,&sa,NULL);
    /* Threads started in reverse dependency order. */
    if (pthread_create(&logger,NULL,logger_thread,NULL)) {status=1;goto cleanup;}
    if (pthread_create(&alert,NULL,alert_thread,NULL)) {
        status=1;atomic_store(&alert_done,1);pthread_join(logger,NULL);goto cleanup;
    }
    if (pthread_create(&processor,NULL,processor_thread,NULL)) {
        status=1;atomic_store(&processor_done,1);pthread_join(alert,NULL);atomic_store(&alert_done,1);pthread_join(logger,NULL);goto cleanup;
    }
    if (pthread_create(&sensor,NULL,sensor_thread,NULL)) {
        status=1;atomic_store(&stop_requested,1);cb_close(&shared_buffer);
        pthread_join(processor,NULL);atomic_store(&processor_done,1);
        pthread_join(alert,NULL);atomic_store(&alert_done,1);pthread_join(logger,NULL);goto cleanup;
    }
    struct sched_param sp={.sched_priority=10};
    int prc=pthread_setschedparam(sensor,SCHED_FIFO,&sp);
    if (prc) fprintf(stderr,"[INFO] SCHED_FIFO unavailable (%s), continuing SCHED_OTHER\n",strerror(prc));
    time_t start=time(NULL);
    while (!atomic_load(&stop_requested) && !sigint_requested && (!duration_sec || (unsigned int)(time(NULL)-start)<duration_sec))
        ms_sleep(100);
    atomic_store(&stop_requested,1);
    pthread_join(sensor,NULL);
    cb_close(&shared_buffer); /* wake processor, then drain remaining items */
    pthread_join(processor,NULL);
    atomic_store(&processor_done,1);
    pthread_join(alert,NULL);
    atomic_store(&alert_done,1);
    pthread_join(logger,NULL);
    /* Exam R3: strictly after ALL joins, read getter values (no hardcode). */
    printf("[SUMMARY] produced=%" PRIu64 " consumed=%" PRIu64
           " size=%zu overflow=%" PRIu64 "\n",
           cb_get_total_produced(&shared_buffer),
           cb_get_total_consumed(&shared_buffer),
           cb_get_current_size(&shared_buffer),
           cb_get_overflow_count(&shared_buffer));
    fflush(stdout);
cleanup:
    if (alert_queue!=(mqd_t)-1) mq_close(alert_queue);
    if (log_queue!=(mqd_t)-1) mq_close(log_queue);
    mq_unlink(alert_name);mq_unlink(log_name);
    cb_destroy(&shared_buffer);
    return (status || atomic_load(&runtime_error)) ? 1 : 0;
}

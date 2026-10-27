/* Assignment 02, shared state declarations | SE203134 | 2026-10-27 */
#ifndef SMS_SHARED_H
#define SMS_SHARED_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "circular_buffer.h"
#include <mqueue.h>
#include <stdatomic.h>
#include <signal.h>

#define DEVICE_PATH "/dev/sms_sensor"
#define LOG_PATH "/tmp/sms.log"
#define BUFCAP 64
#define MQ_TEXT_SIZE 192
#define MSG_MAX 10
#define JITTER_CAP 4096
/* Only the main thread sends this marker, after each producer has joined. */
#define SMS_QUEUE_STOP_MSG "\x01SMS_STOP_MSG"

extern CircularBuffer shared_buffer;
extern mqd_t alert_queue, log_queue;
extern char alert_name[64], log_name[64];
extern volatile sig_atomic_t sigint_requested;
extern atomic_int stop_requested, processor_done, alert_done, runtime_error;
extern atomic_int log_consumer_failed, alert_consumer_failed;
extern unsigned int sample_ms, duration_sec;
extern double temp_threshold, humid_threshold;
extern int verbose;
extern atomic_int sensor_run_gate;
extern long jitter_values_us[JITTER_CAP];
extern size_t jitter_count;
extern int rt_mode;
extern unsigned int alert_temp_high, alert_humid_high, alert_spike;

void ms_sleep(unsigned int millis);
int mq_send_text(mqd_t q, const char *s);
#endif

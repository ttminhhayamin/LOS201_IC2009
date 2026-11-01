/*
 * File: sms_app.c
 * Purpose: Main application for the Sensor Monitoring System.
 * Author: Hoang Trung Hai
 * Student ID: SS204779
 */

#define _POSIX_C_SOURCE 200809L

#include "alert_thread.h"
#include "circular_buffer.h"
#include "logger_thread.h"
#include "processor_thread.h"
#include "sensor_thread.h"
#include "sms_ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define APP_DEFAULT_INTERVAL_MS 500U
#define APP_QUEUE_MAX_MESSAGES 10L
#define APP_SENSOR_PRIORITY 50

/*
 * Req 3: 4 ham getter cua circular buffer. SUA 4 TEN NAY cho khop
 * circular_buffer.h cua ban (grep: total_produced|total_consumed|current_size|overflow_count).
 */
#define BUF_TOTAL_PRODUCED(b)  circular_buffer_total_produced(b)
#define BUF_TOTAL_CONSUMED(b)  circular_buffer_total_consumed(b)
#define BUF_CURRENT_SIZE(b)    circular_buffer_current_size(b)
#define BUF_OVERFLOW_COUNT(b)  circular_buffer_overflow_count(b)

static volatile sig_atomic_t shutdown_requested = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    shutdown_requested = 1;
}

static int create_message_queues(
    mqd_t *log_receiver,
    mqd_t *log_sender,
    mqd_t *alarm_receiver,
    mqd_t *alarm_sender)
{
    struct mq_attr log_attributes;
    struct mq_attr alarm_attributes;

    *log_receiver = (mqd_t)-1;
    *log_sender = (mqd_t)-1;
    *alarm_receiver = (mqd_t)-1;
    *alarm_sender = (mqd_t)-1;

    memset(&log_attributes, 0, sizeof(log_attributes));
    log_attributes.mq_maxmsg = APP_QUEUE_MAX_MESSAGES;
    log_attributes.mq_msgsize = sizeof(sms_log_message_t);

    memset(&alarm_attributes, 0, sizeof(alarm_attributes));
    alarm_attributes.mq_maxmsg = APP_QUEUE_MAX_MESSAGES;
    alarm_attributes.mq_msgsize = sizeof(sms_alarm_message_t);

    /*
     * Remove stale names from a previous run.
     * Existing descriptors, if any, remain valid until closed.
     */
    (void)mq_unlink(SMS_LOG_QUEUE_NAME);
    (void)mq_unlink(SMS_ALARM_QUEUE_NAME);

    *log_receiver = mq_open(
        SMS_LOG_QUEUE_NAME,
        O_CREAT | O_RDONLY,
        0644,
        &log_attributes);

    if (*log_receiver == (mqd_t)-1) {
        goto error;
    }

    *log_sender = mq_open(
        SMS_LOG_QUEUE_NAME,
        O_WRONLY | O_NONBLOCK);

    if (*log_sender == (mqd_t)-1) {
        goto error;
    }

    *alarm_receiver = mq_open(
        SMS_ALARM_QUEUE_NAME,
        O_CREAT | O_RDONLY,
        0644,
        &alarm_attributes);

    if (*alarm_receiver == (mqd_t)-1) {
        goto error;
    }

    *alarm_sender = mq_open(
        SMS_ALARM_QUEUE_NAME,
        O_WRONLY | O_NONBLOCK);

    if (*alarm_sender == (mqd_t)-1) {
        goto error;
    }

    return 0;

error:
    fprintf(
        stderr,
        "[Main] Cannot create message queues: %s\n",
        strerror(errno));

    if (*alarm_sender != (mqd_t)-1) {
        mq_close(*alarm_sender);
        *alarm_sender = (mqd_t)-1;
    }

    if (*alarm_receiver != (mqd_t)-1) {
        mq_close(*alarm_receiver);
        *alarm_receiver = (mqd_t)-1;
    }

    if (*log_sender != (mqd_t)-1) {
        mq_close(*log_sender);
        *log_sender = (mqd_t)-1;
    }

    if (*log_receiver != (mqd_t)-1) {
        mq_close(*log_receiver);
        *log_receiver = (mqd_t)-1;
    }

    (void)mq_unlink(SMS_LOG_QUEUE_NAME);
    (void)mq_unlink(SMS_ALARM_QUEUE_NAME);

    return -1;
}

static void close_message_queues(
    mqd_t log_receiver,
    mqd_t log_sender,
    mqd_t alarm_receiver,
    mqd_t alarm_sender)
{
    if (log_receiver != (mqd_t)-1) {
        mq_close(log_receiver);
    }

    if (log_sender != (mqd_t)-1) {
        mq_close(log_sender);
    }

    if (alarm_receiver != (mqd_t)-1) {
        mq_close(alarm_receiver);
    }

    if (alarm_sender != (mqd_t)-1) {
        mq_close(alarm_sender);
    }

    (void)mq_unlink(SMS_LOG_QUEUE_NAME);
    (void)mq_unlink(SMS_ALARM_QUEUE_NAME);
}

/*
 * Try to create Sensor Thread with SCHED_FIFO priority 50.
 * If real-time scheduling is not permitted, fall back to the
 * normal scheduling policy so the application can still run.
 */
static int create_sensor_thread(
    pthread_t *thread,
    void *(*start_routine)(void *),
    void *arg)
{
    pthread_attr_t attributes;
    struct sched_param scheduling;
    int ret;
    int realtime_ready = 1;

    ret = pthread_attr_init(&attributes);
    if (ret != 0) {
        return ret;
    }

    memset(&scheduling, 0, sizeof(scheduling));
    scheduling.sched_priority = APP_SENSOR_PRIORITY;

    if (pthread_attr_setinheritsched(
            &attributes,
            PTHREAD_EXPLICIT_SCHED) != 0 ||
        pthread_attr_setschedpolicy(
            &attributes,
            SCHED_FIFO) != 0 ||
        pthread_attr_setschedparam(
            &attributes,
            &scheduling) != 0) {
        realtime_ready = 0;
    }

    if (realtime_ready) {
        ret = pthread_create(
            thread,
            &attributes,
            start_routine,
            arg);
    } else {
        ret = EPERM;
    }

    (void)pthread_attr_destroy(&attributes);

    if (ret == 0) {
        fprintf(
            stderr,
            "[Main] Sensor Thread uses SCHED_FIFO priority %d\n",
            APP_SENSOR_PRIORITY);
        return 0;
    }

    fprintf(
        stderr,
        "[Main] SCHED_FIFO unavailable (%s); "
        "using default scheduling policy\n",
        strerror(ret));

    return pthread_create(thread, NULL, start_routine, arg);
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s [-i interval_ms] [-d duration_s | -d device_path]\n"
        "  -i interval_ms  Sampling interval (1..60000 ms; default 500)\n"
        "  -d duration_s   Run for duration_s seconds, 0 = until Ctrl+C (default 0)\n"
        "  -d device_path  Sensor device (an argument starting with '/'; default /dev/sms_sensor)\n",
        program);
}

int main(int argc, char *argv[])
{
    uint32_t interval_ms = APP_DEFAULT_INTERVAL_MS;
    const char *device_path = "/dev/sms_sensor";
    unsigned long duration_s = 0;   /* 0 = chay den khi co SIGINT */

    circular_buffer_t buffer;
    int buffer_initialized = 0;

    mqd_t log_receiver = (mqd_t)-1;
    mqd_t log_sender = (mqd_t)-1;
    mqd_t alarm_receiver = (mqd_t)-1;
    mqd_t alarm_sender = (mqd_t)-1;

    atomic_bool sensor_stop = false;
    atomic_bool alert_stop = false;
    atomic_bool logger_stop = false;

    sensor_thread_args_t sensor_args;
    processor_thread_args_t processor_args;
    alert_thread_args_t alert_args;
    logger_thread_args_t logger_args;

    pthread_t sensor_tid;
    pthread_t processor_tid;
    pthread_t alert_tid;
    pthread_t logger_tid;

    int sensor_started = 0;
    int processor_started = 0;
    int alert_started = 0;
    int logger_started = 0;
    int exit_status = EXIT_FAILURE;
    int opt;

    while ((opt = getopt(argc, argv, "i:d:h")) != -1) {
        switch (opt) {
        case 'i': {
            char *end = NULL;
            unsigned long value;

            errno = 0;
            value = strtoul(optarg, &end, 10);

            if (errno != 0 || end == optarg ||
                *end != '\0' || value < 1 ||
                value > 60000) {
                fprintf(stderr, "[Main] Invalid interval: %s\n", optarg);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }

            interval_ms = (uint32_t)value;
            break;
        }

        case 'd':
            /*
             * -d <duong dan bat dau bang '/'>  : thiet bi (giu tuong thich cu)
             * -d <so>                          : thoi gian chay (giay), 0 = vo han
             */
            if (optarg[0] == '/') {
                device_path = optarg;
            } else {
                char *end = NULL;

                errno = 0;
                duration_s = strtoul(optarg, &end, 10);

                if (errno != 0 || end == optarg || *end != '\0' ||
                    duration_s > 86400UL) {
                    fprintf(stderr, "[Main] Invalid duration: %s\n", optarg);
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            }
            break;

        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;

        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind != argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("========================================\n");
    printf(" Sensor Monitoring System\n");
    printf(" Student: Hoang Trung Hai\n");
    printf(" Student ID: SS204779\n");
    printf(" Device: %s\n", device_path);
    printf(" Sampling interval: %u ms\n", interval_ms);
    printf("========================================\n");

    if (create_message_queues(
            &log_receiver,
            &log_sender,
            &alarm_receiver,
            &alarm_sender) != 0) {
        goto cleanup;
    }

    if (circular_buffer_init(&buffer) != 0) {
        fprintf(stderr, "[Main] Cannot initialize circular buffer\n");
        goto cleanup;
    }

    buffer_initialized = 1;

    {
        struct sigaction action;

        memset(&action, 0, sizeof(action));
        action.sa_handler = handle_sigint;
        sigemptyset(&action.sa_mask);

        if (sigaction(SIGINT, &action, NULL) == -1 ||
            sigaction(SIGTERM, &action, NULL) == -1) {
            fprintf(
                stderr,
                "[Main] Cannot install signal handlers: %s\n",
                strerror(errno));
            goto shutdown;
        }
    }

    logger_args.log_queue = log_receiver;
    logger_args.stop_requested = &logger_stop;
    logger_args.log_path = SMS_LOG_DEFAULT_PATH;

    alert_args.alarm_queue = alarm_receiver;
    alert_args.log_queue = log_sender;
    alert_args.stop_requested = &alert_stop;

    processor_args.input_buffer = &buffer;
    processor_args.alarm_queue = alarm_sender;
    processor_args.log_queue = log_sender;
    processor_args.stop_requested = &sensor_stop;

    sensor_args.buffer = &buffer;
    sensor_args.device_path = device_path;
    sensor_args.interval_ms = interval_ms;
    sensor_args.stop_requested = &sensor_stop;
    sensor_args.log_queue = log_sender;

    /*
     * Start consumers before producers so no queue consumer
     * is missing when the first messages are generated.
     */
    {
        int ret = pthread_create(
            &logger_tid,
            NULL,
            logger_thread,
            &logger_args);

        if (ret != 0) {
            fprintf(
                stderr,
                "[Main] Cannot start Logger Thread: %s\n",
                strerror(ret));
            goto shutdown;
        }

        logger_started = 1;
    }

    {
        int ret = pthread_create(
            &alert_tid,
            NULL,
            alert_thread,
            &alert_args);

        if (ret != 0) {
            fprintf(
                stderr,
                "[Main] Cannot start Alert Thread: %s\n",
                strerror(ret));
            goto shutdown;
        }

        alert_started = 1;
    }

    {
        int ret = pthread_create(
            &processor_tid,
            NULL,
            processor_thread,
            &processor_args);

        if (ret != 0) {
            fprintf(
                stderr,
                "[Main] Cannot start Processor Thread: %s\n",
                strerror(ret));
            goto shutdown;
        }

        processor_started = 1;
    }

    {
        int ret = create_sensor_thread(
            &sensor_tid,
            sensor_thread,
            &sensor_args);

        if (ret != 0) {
            fprintf(
                stderr,
                "[Main] Cannot start Sensor Thread: %s\n",
                strerror(ret));
            goto shutdown;
        }

        sensor_started = 1;
    }

    printf("[Main] All threads started. Press Ctrl+C to stop.\n");

    struct timespec run_start;
    clock_gettime(CLOCK_MONOTONIC, &run_start);

    while (!shutdown_requested) {
        /* Het thoi gian -d thi thoat vong lap, ket thuc binh thuong. */
        if (duration_s > 0) {
            struct timespec now;
            long long elapsed_ms;

            clock_gettime(CLOCK_MONOTONIC, &now);
            elapsed_ms =
                (long long)(now.tv_sec - run_start.tv_sec) * 1000LL +
                (now.tv_nsec - run_start.tv_nsec) / 1000000LL;

            if (elapsed_ms >= (long long)duration_s * 1000LL) {
                break;
            }
        }

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 100000000L
        };

        while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
            if (shutdown_requested) {
                break;
            }
        }
    }

    exit_status = EXIT_SUCCESS;

shutdown:
    fprintf(stderr, "[Main] Beginning orderly shutdown\n");

    /*
     * 1. Stop acquisition and wait for the Sensor Thread.
     */
    atomic_store(&sensor_stop, true);

    if (sensor_started) {
        (void)pthread_join(sensor_tid, NULL);
        sensor_started = 0;
    }

    /*
     * 2. Close the buffer. Processor drains existing samples
     *    before circular_buffer_pop() returns ECANCELED.
     */
    if (buffer_initialized) {
        (void)circular_buffer_close(&buffer);
    }

    if (processor_started) {
        (void)pthread_join(processor_tid, NULL);
        processor_started = 0;
    }

    /*
     * 3. Let Alert Thread drain pending alarms before exiting.
     */
    atomic_store(&alert_stop, true);

    if (alert_started) {
        (void)pthread_join(alert_tid, NULL);
        alert_started = 0;
    }

    /*
     * 4. Stop Logger last, after all producers have stopped.
     *    It drains the log queue until a receive timeout finds
     *    the queue empty.
     */
    atomic_store(&logger_stop, true);

    if (logger_started) {
        (void)pthread_join(logger_tid, NULL);
        logger_started = 0;
    }

    /*
     * Req 3: tat ca thread da join xong -> in 1 dong tong ket ra stdout,
     * lay so lieu tu getter cua circular buffer (phai in TRUOC khi destroy).
     */
    if (buffer_initialized) {
        printf("[SUMMARY] produced=%llu consumed=%llu size=%llu overflow=%llu\n",
               (unsigned long long)BUF_TOTAL_PRODUCED(&buffer),
               (unsigned long long)BUF_TOTAL_CONSUMED(&buffer),
               (unsigned long long)BUF_CURRENT_SIZE(&buffer),
               (unsigned long long)BUF_OVERFLOW_COUNT(&buffer));
        fflush(stdout);
    }

    if (buffer_initialized) {
        (void)circular_buffer_destroy(&buffer);
        buffer_initialized = 0;
    }

cleanup:
    /*
     * Defensive cleanup for startup failures.
     */
    atomic_store(&sensor_stop, true);
    atomic_store(&alert_stop, true);
    atomic_store(&logger_stop, true);

    if (sensor_started) {
        (void)pthread_join(sensor_tid, NULL);
    }

    if (buffer_initialized) {
        (void)circular_buffer_close(&buffer);
    }

    if (processor_started) {
        (void)pthread_join(processor_tid, NULL);
    }

    if (alert_started) {
        (void)pthread_join(alert_tid, NULL);
    }

    if (logger_started) {
        (void)pthread_join(logger_tid, NULL);
    }

    close_message_queues(
        log_receiver,
        log_sender,
        alarm_receiver,
        alarm_sender);

    if (exit_status == EXIT_SUCCESS) {
        fprintf(stderr, "[Main] Shutdown complete\n");
    }

    return exit_status;
}

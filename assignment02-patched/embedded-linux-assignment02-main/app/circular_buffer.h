#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Capacity of the circular buffer.
 * Can be overridden by compiler flags if required.
 */
#ifndef CIRCULAR_BUFFER_CAPACITY
#define CIRCULAR_BUFFER_CAPACITY 64
#endif

/*
 * One sensor sample.
 *
 * Temperature and humidity use integer tenths:
 * 324 means 32.4 degrees Celsius.
 * 738 means 73.8 percent humidity.
 */
typedef struct {
    uint64_t timestamp_ms;
    int32_t temperature_tenths;
    int32_t humidity_tenths;
} sensor_sample_t;

/*
 * Thread-safe FIFO circular buffer.
 */
typedef struct {
    sensor_sample_t data[CIRCULAR_BUFFER_CAPACITY];

    size_t head;
    size_t tail;
    size_t count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    int initialized;
    int closed;

    /* Thong ke cho dong [SUMMARY] cua sms_app (bao ve boi mutex). */
    uint64_t total_produced;   /* so mau da push thanh cong            */
    uint64_t total_consumed;   /* so mau da pop thanh cong             */
    uint64_t overflow_count;   /* so lan try_push that bai vi day      */
} circular_buffer_t;

/*
 * Initialize the buffer.
 * Returns 0 on success, otherwise an error number.
 */
int circular_buffer_init(circular_buffer_t *buffer);

/*
 * Add one sample.
 * Blocks when the buffer is full.
 * Returns 0 on success, otherwise an error number.
 */
int circular_buffer_push(
    circular_buffer_t *buffer,
    const sensor_sample_t *sample
);

int circular_buffer_try_push(
    circular_buffer_t *buffer,
    const sensor_sample_t *sample
);

/*
 * Remove one sample.
 * Blocks when the buffer is empty.
 * Returns 0 on success, otherwise an error number.
 */
int circular_buffer_pop(
    circular_buffer_t *buffer,
    sensor_sample_t *sample
);

/* Getter thong ke (thread-safe, khoa mutex ben trong). */
uint64_t circular_buffer_total_produced(circular_buffer_t *buffer);
uint64_t circular_buffer_total_consumed(circular_buffer_t *buffer);
size_t   circular_buffer_current_size(circular_buffer_t *buffer);
uint64_t circular_buffer_overflow_count(circular_buffer_t *buffer);

/*
 * Close the buffer and wake waiting threads.
 * After closure, new samples cannot be pushed.
 * Already buffered samples can still be consumed.
 */
int circular_buffer_close(circular_buffer_t *buffer);

/*
 * Destroy synchronization objects.
 * Call only after all threads using the buffer have stopped.
 */
int circular_buffer_destroy(circular_buffer_t *buffer);

#endif /* CIRCULAR_BUFFER_H */

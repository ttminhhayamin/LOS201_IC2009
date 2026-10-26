/* Thread-safe circular buffer | Tran Tuan Minh - SE203134 | 2026-10-26 */
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t timestamp_ms;
    double temperature;
    double humidity;
} SensorData;

typedef struct {
    SensorData *items;
    size_t capacity, head, tail, current_size;
    uint64_t total_produced, total_consumed, overflow_count;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t nonempty;
} CircularBuffer;

int cb_init(CircularBuffer *b, size_t cap);
void cb_close(CircularBuffer *b);
void cb_destroy(CircularBuffer *b);
int cb_push(CircularBuffer *b, SensorData x); /* 1 success, 0 full, -1 closed */
int cb_pop(CircularBuffer *b, SensorData *out); /* 1 success, 0 finished */
uint64_t cb_get_total_produced(CircularBuffer *b);
uint64_t cb_get_total_consumed(CircularBuffer *b);
size_t cb_get_current_size(CircularBuffer *b);
uint64_t cb_get_overflow_count(CircularBuffer *b);
#endif

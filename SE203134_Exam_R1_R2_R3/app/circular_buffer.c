/* Mutex + condvar circular buffer | Tran Tuan Minh - SE203134 | 2026-10-26 */
#include "circular_buffer.h"
#include <stdlib.h>
#include <string.h>

int cb_init(CircularBuffer *b, size_t cap)
{
    memset(b, 0, sizeof(*b));
    b->items = calloc(cap, sizeof(*b->items));
    if (!b->items) return -1;
    b->capacity = cap;
    if (pthread_mutex_init(&b->mutex, NULL)) {
        free(b->items); return -1;
    }
    if (pthread_cond_init(&b->nonempty, NULL)) {
        pthread_mutex_destroy(&b->mutex);
        free(b->items);
        return -1;
    }
    return 0;
}
void cb_close(CircularBuffer *b)
{
    pthread_mutex_lock(&b->mutex);
    b->closed = 1;
    pthread_cond_broadcast(&b->nonempty);
    pthread_mutex_unlock(&b->mutex);
}
void cb_destroy(CircularBuffer *b)
{
    pthread_cond_destroy(&b->nonempty);
    pthread_mutex_destroy(&b->mutex);
    free(b->items);
}
int cb_push(CircularBuffer *b, SensorData x)
{
    int ret = 1;
    pthread_mutex_lock(&b->mutex);
    if (b->closed) ret = -1;
    else if (b->current_size == b->capacity) {
        b->overflow_count++;
        ret = 0;
    } else {
        b->items[b->tail] = x;
        b->tail = (b->tail + 1) % b->capacity;
        b->current_size++;
        b->total_produced++;
        pthread_cond_signal(&b->nonempty);
    }
    pthread_mutex_unlock(&b->mutex);
    return ret;
}
int cb_pop(CircularBuffer *b, SensorData *out)
{
    pthread_mutex_lock(&b->mutex);
    while (!b->current_size && !b->closed)
        pthread_cond_wait(&b->nonempty, &b->mutex);
    if (!b->current_size) {
        pthread_mutex_unlock(&b->mutex);
        return 0;
    }
    *out = b->items[b->head];
    b->head = (b->head + 1) % b->capacity;
    b->current_size--;
    b->total_consumed++;
    pthread_mutex_unlock(&b->mutex);
    return 1;
}
uint64_t cb_get_total_produced(CircularBuffer *b)
{
    uint64_t v; pthread_mutex_lock(&b->mutex); v=b->total_produced;
    pthread_mutex_unlock(&b->mutex); return v;
}
uint64_t cb_get_total_consumed(CircularBuffer *b)
{
    uint64_t v; pthread_mutex_lock(&b->mutex); v=b->total_consumed;
    pthread_mutex_unlock(&b->mutex); return v;
}
size_t cb_get_current_size(CircularBuffer *b)
{
    size_t v; pthread_mutex_lock(&b->mutex); v=b->current_size;
    pthread_mutex_unlock(&b->mutex); return v;
}
uint64_t cb_get_overflow_count(CircularBuffer *b)
{
    uint64_t v; pthread_mutex_lock(&b->mutex); v=b->overflow_count;
    pthread_mutex_unlock(&b->mutex); return v;
}

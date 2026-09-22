#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "common.h"
#include <stddef.h>

typedef struct {
    ControlFrame *buffer;
    ControlFrame *buffer_end;
    size_t capacity;
    size_t count;
    ControlFrame *head;
    ControlFrame *tail;
} RingBuffer;

// Initialize the ring buffer
void rb_init(RingBuffer *rb, size_t capacity);

// Free the ring buffer
void rb_free(RingBuffer *rb);

// Push back a frame into the ring buffer
int rb_push_back(RingBuffer *rb, const ControlFrame *frame);

// Pop a frame from the front of a ring buffer
int rb_pop_front(RingBuffer *rb, ControlFrame *frame);

#endif
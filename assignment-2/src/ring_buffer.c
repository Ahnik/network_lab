#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>

void rb_init(RingBuffer *rb, size_t capacity) {
    rb->buffer = (ControlFrame *) calloc(capacity, CONTROL_FRAME_SIZE);
    if (rb->buffer == NULL)
        return;
    rb->buffer_end = rb->buffer + capacity;
    rb->capacity = capacity;
    rb->count = 0;
    rb->head = rb->buffer;
    rb->tail = rb->buffer;
}

void rb_free(RingBuffer *rb) {
    free(rb->buffer);
}

int rb_push_back(RingBuffer *rb, const ControlFrame *frame) {
    if (rb->count == rb->capacity)
        return -1;
    memcpy(rb->head, frame, CONTROL_FRAME_SIZE);
    rb->head++;
    if (rb->head == rb->buffer_end)
        rb->head = rb->buffer;
    rb->count++;
    return 0;
}

int rb_pop_front(RingBuffer *rb, ControlFrame *frame) {
    if (rb->count == 0)
        return -1;
    memcpy(frame, rb->tail, CONTROL_FRAME_SIZE);
    rb->tail++;
    if (rb->tail == rb->buffer_end)
        rb->tail = rb->buffer;
    rb->count--;
    return 0;
}
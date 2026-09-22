#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"
#include "ring_buffer.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    struct timespec expiry_time;
    uint32_t index;
    bool acked;
    bool timer_active;
} WindowSlot;

// All possible states the receiver can be in
typedef enum {
    ACK_RECEIVED = 0,
    INTERRUPTED,
    RECEIVER_EVENTS,
} ReceiverEvent;

// Receiver struct
typedef struct {
    pthread_t thread;
    pthread_mutex_t lock;
    ReceiverEvent event;
    RingBuffer rb;
    const int sockfd;
    bool client_connected;
} Receiver;

void *receiver_function(void *arg);
struct timespec get_deadline(int timeout_ms);
bool is_expired(struct timespec deadline);

#endif
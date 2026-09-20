#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"
#include <pthread.h>
#include <stdbool.h>

// All possible states the receiver can be in
typedef enum {
    ACK_RECEIVED = 0,
    TIMEOUT,
    INTERRUPTED,
    RECEIVER_EVENTS,
} ReceiverEvent;

// Receiver struct
typedef struct {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ReceiverEvent event;
    AckFrame ack;
    const int sockfd;
    const int stopfd;
    const int timeout;
    bool client_connected;
    bool is_running;
} Receiver;

void *receiver_function(void *arg);

#endif
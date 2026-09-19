#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"
#include <pthread.h>
#include <stdbool.h>

// All possible states the receiver can be in
typedef enum {
    RECEIVER_STOPPED = 0,
    RECEIVER_RUNNING,
    RECEIVER_STATES,
} ReceiverState;

// Receiver struct
typedef struct {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ReceiverState state;
    AckFrame ack;
    const int sockfd;
    const int stopfd;
    const int timeout;
    bool received_ack;
    bool client_connected;
} Receiver;

void *receiver_function(void *arg);

#endif
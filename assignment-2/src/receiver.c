#define _POSIX_C_SOURCE 200809L

#include "receiver.h"
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

void *receiver_function(void *arg) {
    Receiver *receiver = (Receiver *) arg;
    struct pollfd pfd;
    pfd.fd = receiver->sockfd;
    pfd.events = POLLIN;
    while (true) {
        int ret = poll(&pfd, 1, -1);
        if (ret > 0) {
            pthread_mutex_lock(&receiver->lock);
            ControlFrame frame;
            if (receive_in_buffer((uint8_t *) &frame, CONTROL_FRAME_SIZE, receiver->sockfd) < 0) {
                receiver->client_connected = false;
                pthread_mutex_unlock(&receiver->lock);
                break;
            }
            rb_push_back(&receiver->rb, &frame);
            pthread_mutex_unlock(&receiver->lock);
        }
    }

    return NULL;
}

struct timespec get_deadline(int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000LL;
    if (ts.tv_nsec >= 1000000000LL) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000LL;
    }
    return ts;
}

bool is_expired(struct timespec deadline) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > deadline.tv_sec) return true;
    if (now.tv_sec == deadline.tv_sec && now.tv_nsec > deadline.tv_nsec) return true;
    return false;
}
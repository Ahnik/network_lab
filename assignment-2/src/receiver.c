#include "receiver.h"
#include <poll.h>
#include <stdio.h>

void *receiver_function(void *arg) {
    Receiver *receiver = (Receiver *) arg;
    struct pollfd fds[2];
    fds[0].fd     = receiver->sockfd;
    fds[0].events = POLLIN;
    fds[1].fd     = receiver->stopfd;
    fds[1].events = POLLIN;

    while (true) {
        pthread_mutex_lock(&receiver->lock);
        if (receiver->state == RECEIVER_STOPPED) {
            pthread_cond_wait(&receiver->cond, &receiver->lock);
            if (receiver->state == RECEIVER_RUNNING) {
                pthread_mutex_unlock(&receiver->lock);
                // Try to receive client before timeout
                int ret = poll(fds, 2, receiver->timeout);
                if (ret < 0)
                    exit_with_error("Poll error!");

                pthread_mutex_lock(&receiver->lock);
                if (fds[0].revents & POLLIN) {     // ACK has arrived
                    if (receive_in_buffer((uint8_t *) &receiver->ack, ACK_SIZE, fds[0].fd) < 1) {
                        receiver->client_connected = false;
                        pthread_mutex_unlock(&receiver->lock);
                        break;
                    }
                    receiver->received_ack = true;
                }
                receiver->state = RECEIVER_STOPPED;
            }
        } else
            receiver->state = RECEIVER_STOPPED;
        pthread_mutex_unlock(&receiver->lock);
    }

    return NULL;
}
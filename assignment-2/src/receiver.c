#include "receiver.h"
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

void *receiver_function(void *arg) {
    Receiver *receiver = (Receiver *) arg;
    struct pollfd fds[2];
    fds[0].fd     = receiver->sockfd;
    fds[0].events = POLLIN;
    fds[1].fd     = receiver->stopfd;
    fds[1].events = POLLIN;

    while (true) {
        pthread_mutex_lock(&receiver->lock);
        receiver->is_running = false;
        pthread_cond_wait(&receiver->cond, &receiver->lock);
        if (receiver->is_running) {
            pthread_mutex_unlock(&receiver->lock);
            // Try to receive client before timeout
            int ret = poll(fds, 2, receiver->timeout);
            if (ret < 0)
                exit_with_error("Poll error!");

            pthread_mutex_lock(&receiver->lock);
            if (ret == 0)                           // Timeout
                receiver->event = TIMEOUT;
            else {
                if (fds[0].revents & POLLIN) {      // ACK has arrived
                    receiver->event = ACK_RECEIVED;
                    if (receive_in_buffer((uint8_t *) &receiver->frame, ACK_SIZE, fds[0].fd) < 0) {
                        perror("receive");
                        receiver->client_connected = false;
                        pthread_mutex_unlock(&receiver->lock);
                        break;
                    }
                }
                if (fds[1].revents & POLLIN) {       // Stop notification has arrived
                    receiver->event = INTERRUPTED;
                    char ch;
                    read(receiver->stopfd, &ch, sizeof(ch));
                }
            }
            receiver->is_running = false;
        }
        pthread_mutex_unlock(&receiver->lock);
    }

    return NULL;
}
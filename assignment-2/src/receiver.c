#include "receiver.h"
#include <poll.h>

void *receiver_function(void *arg) {
    Receiver *receiver = (Receiver *) arg;
    struct pollfd fds[2];
    fds[0].fd = receiver->sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = receiver->stopfd;
    fds[1].events = POLLIN;

    int ret = poll(fds, 2, receiver->timeout);
    if (ret < 0)                        // Error
        exit_with_error("Poll error!");
    if (fds[1].revents & POLLIN)        // Stop signal
        return NULL;
    if (fds[0].revents & POLLIN) {      // ACK has arrived
        receive_in_buffer((uint8_t *) receiver->ack_buffer, ACK_SIZE, fds[0].fd);
        receiver->received_ack = true;
    }
}
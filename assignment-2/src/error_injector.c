#include <stdlib.h>
#include <time.h>
#include "error_injector.h"
#include "common.h"

void inject_single_bit_error(uint8_t *buffer, unsigned int size) {
    unsigned int pos = rand() % (size << 3);
    buffer[pos >> 3] ^= 1 << (pos % 8);
}

void inject_two_isolated_error(uint8_t *buffer, unsigned int size) {
    int m = rand() % (size << 3);
    int n = m;
    while (abs(m - n) < 2) n = rand() % (size << 3);
    buffer[m >> 3] ^= 1 << (m % 8);
    buffer[n >> 3] ^= 1 << (n % 8);
}

void inject_odd_errors(uint8_t *buffer, unsigned int size) {
    unsigned int no_of_errors = ((rand() % 3) * 2) + 3;
    for (unsigned int i = 0; i < no_of_errors; i++) {
        unsigned int pos = rand() % (size << 3);
        buffer[pos >> 3] ^= 1 << (pos % 8);
    }
}

void inject_burst_error(uint8_t *buffer, unsigned int size) {
    unsigned int no_of_errors = (rand() % 32) + 3;
    unsigned int start = rand() % (size << 3);
    for (unsigned int i = 0; i < no_of_errors; i++) {
        unsigned int pos = start + i;
        if (pos >= (size << 3)) break;
        buffer[pos >> 3] ^= 1 << (pos % 8);
    }
}

void inject_error(uint8_t *frame, size_t length) {
    ErrorType error;
    error = rand() % ERROR_NUM;
    switch (error) {
        case SINGLE_BIT:
            inject_single_bit_error(frame, length);
            break;
        case TWO_ISOLATED:
            inject_two_isolated_error(frame, length);
            break;
        case ODD_ERRORS:
            inject_odd_errors(frame, length);
            break;
        case BURST:
            inject_burst_error(frame, length);
            break;
        case NO_ERROR:
            break;
        case ERROR_NUM:
            break;
    }
}

void send_ack_with_error(int ack_no, int sender_socket) {
    AckFrame frame;
    frame.frame_type = 0xFF;
    frame.ack_no = ack_no;
    uint32_t crc32 = compute_crc32((uint8_t *) &frame, ACK_SIZE - 4);
    frame.fcs[0] = (uint8_t) (crc32 >> 24);
    frame.fcs[1] = (uint8_t) (crc32 >> 16);
    frame.fcs[2] = (uint8_t) (crc32 >> 8);
    frame.fcs[3] = (uint8_t) (crc32);

#ifdef INJECT_ERROR
    inject_error((uint8_t *) &frame, ACK_SIZE);
#endif

    send_from_buffer((uint8_t *) &frame, ACK_SIZE, sender_socket);
}
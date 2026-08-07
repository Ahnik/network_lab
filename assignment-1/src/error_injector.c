#include <stdlib.h>
#include <time.h>
#include "error_injector.h"

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
    unsigned int no_of_errors = ((rand() % 3) << 2) + 3;
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
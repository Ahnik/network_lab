#include <stdlib.h>
#include <time.h>
#include "error_injector.h"
#include "crc.h"

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

void flip_two_words(uint16_t *buffer, unsigned int size) {
    unsigned int pos1 = rand() % size;
    unsigned int pos2 = pos1;
    while (pos1 == pos2) pos2 = rand() % size;
    uint16_t temp = buffer[pos1];
    buffer[pos1] = buffer[pos2];
    buffer[pos2] = temp;
}

void inject_crc8_proof_error(uint8_t *buffer, unsigned int size) {
    unsigned int pos = rand() % (size - 1);
    buffer[pos] ^= 0x01;
    buffer[pos+1] ^= CRC8_GENERATOR;
}

void inject_crc10_proof_error(uint8_t *buffer, unsigned int size) {
    unsigned int pos = rand() % (size - 2);
    buffer[pos] ^= 0x01;
    buffer[pos+1] ^= (uint8_t) (CRC10_GENERATOR >> 2);
    buffer[pos+2] ^= (uint8_t) (CRC10_GENERATOR << 6);
}

void inject_crc16_proof_error(uint8_t *buffer, unsigned int size) {
    unsigned int pos = rand() % (size - 2);
    buffer[pos] ^= 0x01;
    buffer[pos+1] ^= (uint8_t) (CRC16_GENERATOR >> 8);
    buffer[pos+2] ^= (uint8_t) (CRC16_GENERATOR);
}
void inject_crc32_proof_error(uint8_t *buffer, unsigned int size) {
    unsigned int pos = rand() % (size - 4);
    buffer[pos] ^= 0x01;
    buffer[pos+1] ^= (uint8_t) (CRC32_GENERATOR >> 24);
    buffer[pos+2] ^= (uint8_t) (CRC32_GENERATOR >> 16);
    buffer[pos+3] ^= (uint8_t) (CRC32_GENERATOR >> 8);
    buffer[pos+4] ^= (uint8_t) (CRC32_GENERATOR);
}
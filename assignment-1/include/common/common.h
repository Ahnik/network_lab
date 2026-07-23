#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PAYLOAD_SIZE 64      // Size of codewords in bytes

typedef struct {
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    uint64_t length;                // The actual size of the payload
    uint16_t checksum;              // Checksum of the payload
    /* TODO: Add fields for the CRC later on */
} Packet;

#endif
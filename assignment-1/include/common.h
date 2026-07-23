#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>

#define PACKET_SIZE 64      // Size of codewords in bytes

typedef struct {
    uint8_t payload[PACKET_SIZE];   // The actual payload buffer
    size_t length;                  // The actual size of the payload
    uint16_t checksum;              // Checksum of the payload
    /* TODO: Add fields for the CRC later on */
} Packet;

#endif
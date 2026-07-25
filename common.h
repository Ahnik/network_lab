#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define FRAME_SIZE       64        // Size of frame in bytes
#define MAC_ADDRESS_SIZE  6        // Size of MAC address in bytes

typedef struct {
    uint8_t  sender_addr[MAC_ADDRESS_SIZE];     // Sender address
    uint8_t  receiver_addr[MAC_ADDRESS_SIZE];   // Receiver address
    uint16_t length;                            // The length of the payload
    uint16_t type;                              // The type of error detection used
} Header;

typedef union {
    uint8_t  crc8;          // CRC-8 bits
    uint16_t crc10;         // CRC-16 bits
    uint16_t crc16;         // CRC-16 bits
    uint32_t crc32;         // CRC-32 bits
    uint16_t checksum;      // Checksum bits
} Trailer;

#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

typedef struct {
    Header header;
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    Trailer trailer;
} Frame;

#endif
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <limits.h>
#include <stdbool.h>

#define FRAME_SIZE       64        // Size of frame in bytes
#define MAC_ADDRESS_SIZE  6        // Size of MAC address in bytes

#pragma pack(push, 1)
typedef struct {
    uint8_t  sender_addr[MAC_ADDRESS_SIZE];     // Sender address
    uint8_t  receiver_addr[MAC_ADDRESS_SIZE];   // Receiver address
    uint16_t length;                            // The length of the payload
    uint16_t type;                              // The type of error detection used
} Header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union {
    uint8_t  crc8;          // CRC-8 bits
    uint16_t crc10;         // CRC-16 bits
    uint16_t crc16;         // CRC-16 bits
    uint32_t crc32;         // CRC-32 bits
    uint16_t checksum;      // Checksum bits
} Trailer;
#pragma pack(pop)

#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

typedef struct {
    Header header;
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    Trailer trailer;
} Frame;

// Function to terminate the program in case of an error and print it
void exit_with_error(const char *fmt, ...);

// Function to open a file, read it, chunk it into packets and return the first packet
Frame *chunk_file(const char *filename);

// Function to find the 16-bit checksum of a buffer
uint16_t find_checksum(uint16_t *buffer, size_t size);

// Function to verify 16-bit checksum
bool verify_checksum(uint16_t *buffer, size_t size, uint16_t checksum);

#endif
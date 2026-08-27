#ifndef COMMON_H
#define COMMON_H

#define INJECT_ERROR    /* Comment this line if you don't want to inject error */

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define RECEIVER_PORT  8989

#define FRAME_SIZE       64        // Size of frame in bytes
#define MAC_ADDRESS_SIZE  6        // Size of MAC address in bytes
#define HEADER_SIZE       4        // Size of the header containing length

#define CRC32_GENERATOR 0x04C11DB7
#define CRC_TABLE_SIZE (2 << 8)

#pragma pack(push, 1)
typedef struct {
    uint8_t  sender_addr[MAC_ADDRESS_SIZE];     // Sender address
    uint8_t  receiver_addr[MAC_ADDRESS_SIZE];   // Receiver address
    uint16_t length;                            // The length of the payload
    uint8_t  seq_no;                            // Frame sequence number
} Header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t fcs[4];         // Frame check sequence (CRC-32 bytes)
} Trailer;
#pragma pack(pop)

#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

#pragma pack(push, 1)
typedef struct {
    Header header;
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    Trailer trailer;
} Frame;
#pragma pack(pop)

// Lookup table for calculating CRC-32
static uint32_t crc32_table[CRC_TABLE_SIZE];

// Function to terminate the program in case of an error and print it
void exit_with_error(const char *fmt, ...);

// Fill up the sender and receiver MAC addresses with dummy data
void input_mac_address(Frame *frame);

// Function to read the header containing length of the message
uint32_t read_payload_len(int socketfd);

// Function to open a file, read it, chunk it into packets and return the first packet
Frame *chunk_file(const char *filename, uint32_t *num_of_frames);

// Function to create the CRC-32 lookup table
void create_crc32_table();

// Function to compute CRC-32 of a buffer
uint32_t compute_crc32(const uint8_t *buffer, size_t size);

#endif
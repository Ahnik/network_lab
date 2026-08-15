#ifndef COMMON_H
#define COMMON_H

#define BENCHMARK   /* Comment this line if you don't want to calculate time taken */

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define RECEIVER_PORT  8989

#define FRAME_SIZE       64        // Size of frame in bytes
#define MAC_ADDRESS_SIZE  6        // Size of MAC address in bytes
#define HEADER_SIZE       4        // Size of the header containing length
#ifdef BENCHMARK
    #define PRINT_BUFFER_SIZE 42
#endif

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
    uint8_t crc[4];         // CRC bits
    uint16_t checksum;      // Checksum bits
} Trailer;
#pragma pack(pop)

#define ERROR_RANGE (FRAME_SIZE - sizeof(Trailer))
#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

#pragma pack(push, 1)
typedef struct {
    Header header;
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    Trailer trailer;
} Frame;
#pragma pack(pop)

// Function to terminate the program in case of an error and print it
void exit_with_error(const char *fmt, ...);

// Fill up the sender and receiver MAC addresses with dummy data
void input_mac_address(Frame *frame);

// Function to read the header containing length of the message
uint32_t read_payload_len(int socketfd);

// Function to open a file, read it, chunk it into packets and return the first packet
Frame *chunk_file(const char *filename, uint32_t *num_of_frames);

// Function to find the 16-bit checksum of a buffer
uint16_t find_checksum(const uint16_t *buffer, size_t size);

#endif
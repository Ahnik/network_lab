#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define DEFAULT_PORT       8989

#define FRAME_SIZE         64       // Size of frame in bytes
#define MAC_ADDRESS_SIZE    6       // Size of MAC address in bytes
#define HEADER_SIZE         4       // Size of the header containing length
#define CONTROL_FRAME_SIZE  6       // Size of control frame

#define CRC32_GENERATOR 0x04C11DB7
#define CRC_TABLE_SIZE  (2 << 8)

// Control frame types
#define ACK_FRAME            0xFF
#define NAK_FRAME            0x00
#define REGISTER_REQ_TYPE    0x01
#define REGISTER_RESP_TYPE   0x02
#define SYNC_SLOT_TYPE       0x03
#define DATA_CHIP_TYPE       0x04
#define COMPOSITE_CHIP_TYPE  0x05
#define TERMINATE_TYPE       0x06

#pragma pack(push, 1)
typedef struct {
    uint8_t  sender_addr[MAC_ADDRESS_SIZE];     // Sender address
    uint8_t  receiver_addr[MAC_ADDRESS_SIZE];   // Receiver address
    uint16_t length;                            // The length of the payload
    uint8_t  seq_no;                            // Frame sequence number
    uint8_t  station_id;                        // Station ID (0 to N-1)
} Header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t fcs[4];                             // Frame check sequence (CRC-32)
} Trailer;
#pragma pack(pop)

#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

#pragma pack(push, 1)
typedef struct {
    Header  header;
    uint8_t payload[PAYLOAD_SIZE];              // The actual payload buffer
    Trailer trailer;
} Frame;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t frame_type;
    uint8_t seq_no;
    uint8_t fcs[4];
} ControlFrame;
#pragma pack(pop)

// Lookup table for calculating CRC-32
extern uint32_t crc32_table[CRC_TABLE_SIZE];

#ifdef __cplusplus
extern "C" {
#endif

// Function to terminate the program in case of an error and print it
void exit_with_error(const char *fmt, ...);

// Fill up the sender and receiver MAC addresses with dummy/station data
void input_mac_address(Frame *frame, uint8_t sender_id, uint8_t receiver_id);

// Function to read length prefix
uint32_t read_payload_len(int socketfd);

// Function to open a file, read it, chunk it into packets and return array of frames
Frame *chunk_file(const char *filename, uint32_t *num_of_frames);

// Function to create the CRC-32 lookup table
void create_crc32_table(void);

// Function to compute CRC-32 of a buffer
uint32_t compute_crc32(const uint8_t *buffer, size_t size);

// Function to send data from a buffer completely
int send_from_buffer(const uint8_t *buffer, long size, int socket);

// Function to receive data into a buffer completely
int receive_in_buffer(uint8_t *buffer, long size, int socket);

#ifdef __cplusplus
}
#endif

#endif // COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "common.h"
#include "crc.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./main.out <filename>\n");
        return -1;
    }

    // Seed the random number generator
    srand((unsigned int) time(NULL));

    // Chunk the input file into frames
    size_t total_frames = 0;
    Frame *frame_buffer = chunk_file(argv[1], &total_frames);
    ErrorDetectingCode code = rand() % NUM_OF_CODES;

    for (size_t i = 0; i < total_frames; i++) {
        switch (code) {
            case CHECKSUM:
                frame_buffer[i].trailer.checksum = htons(find_checksum(frame_buffer[i].payload, PAYLOAD_SIZE/2));
                break;
            case CRC8:
                frame_buffer[i].trailer.crc[0] = compute_crc8(frame_buffer[i].payload, PAYLOAD_SIZE);
                break;
            case CRC10:
                uint16_t crc10 = compute_crc10(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc10 >> 2);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc10 << 6);
                break;
            case CRC16:
                uint16_t crc16 = compute_crc16(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc16 >> 8);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc16);
                break;
            case CRC32:
                uint32_t crc32 = compute_crc32(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc32 >> 24);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc32 >> 16);
                frame_buffer[i].trailer.crc[2] = (uint8_t) (crc32 >> 8);
                frame_buffer[i].trailer.crc[3] = (uint8_t) (crc32);
                break;
        }
    }

    // Inject an error
    ErrorType error;
    for (int i = 0; i < total_frames; i++) {
        error = rand() % ERROR_NUM;
        switch (error) {
            case SINGLE_BIT:
                inject_single_bit_error((uint8_t *) frame_buffer, FRAME_SIZE);
                break;
            case TWO_ISOLATED:
                inject_two_isolated_error((uint8_t *) frame_buffer, FRAME_SIZE);
                break;
            case ODD_ERRORS:
                inject_odd_errors((uint8_t *) frame_buffer, FRAME_SIZE);
                break;
            case BURST:
                inject_burst_error((uint8_t *) frame_buffer, FRAME_SIZE);
                break;
        }
    }

    // Create the socket to communicate with the receiver
    int receiver_socket;
    if ((receiver_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        exit_with_error("Failed to create socket!");

    // Initialize the fill up the receiver address struct
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port   = htons(RECEIVER_PORT);

    if (inet_pton(AF_INET, argv[1], &receiver_addr.sin_addr) <= 0)
        exit_with_error("inet_pton error for %s", argv[1]);

    // Try to connect to the receiver
    if (connect(receiver_socket, (struct sockaddr *) &receiver_addr, (socklen_t) sizeof(receiver_addr)) < 0)
        exit_with_error("Connection failed!");

    // Sending the total message to the receiver
    ssize_t total_bytes_sent = 0;
    ssize_t total_size = total_frames * FRAME_SIZE;
    uint8_t *buffer_ptr = (uint8_t *) frame_buffer;
    while (total_bytes_sent < total_size) {
        ssize_t bytes_sent = send(receiver_socket, buffer_ptr + total_bytes_sent, total_size - total_bytes_sent, 0);
        if (bytes_sent < 0)
            exit_with_error("Send Failed!");
        total_bytes_sent += bytes_sent;
    }

    return 0;
}
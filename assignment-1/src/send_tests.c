#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "common.h"
#include "crc.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    /* argv[1] = Error detection code, argv[2] = IP address, argv[3] = file */
    if (argc < 4) {
        printf("Usage: ./main.out <filename>\n");
        return -1;
    }

    ErrorDetectingCode code = string_to_code(argv[1]);
    if (code == NUM_OF_CODES) {
        printf("Invalid error detection code!\n");
        return -1;
    }

    // Create CRC lookup table
    switch (code) {
        case CRC8:
            create_crc8_table();
            break;
        case CRC10:
            create_crc10_table();
            break;
        case CRC16:
            create_crc16_table();
            break;
        case CRC32:
            create_crc32_table();
    }

    // Seed the random number generator
    srand((unsigned int) time(NULL));

    // Chunk the input file into frames
    uint32_t total_frames = 0;
    Frame *frame_buffer = chunk_file(argv[3], &total_frames);

    for (uint32_t i = 0; i < total_frames; i++) {
        input_mac_address(&frame_buffer[i]);
        frame_buffer[i].header.type = htons(code);
        switch (code) {
            case CHECKSUM:
                frame_buffer[i].trailer.checksum = htons(find_checksum((uint16_t *) &frame_buffer[i], (FRAME_SIZE - sizeof(Trailer)) >> 1));
                break;
            case CRC8:
                frame_buffer[i].trailer.crc[0] = compute_crc8((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
                break;
            case CRC10:
                uint16_t crc10 = compute_crc10((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc10 >> 2);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc10 << 6);
                break;
            case CRC16:
                uint16_t crc16 = compute_crc16((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc16 >> 8);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc16);
                break;
            case CRC32:
                uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc32 >> 24);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc32 >> 16);
                frame_buffer[i].trailer.crc[2] = (uint8_t) (crc32 >> 8);
                frame_buffer[i].trailer.crc[3] = (uint8_t) (crc32);
        }
    }

    // Inject an error
    ErrorType error;
    for (uint32_t i = 0; i < total_frames; i++) {
        printf("--- FRAME #%u ---\n", i+1);
        switch (i % 3) {
            case 0:
                inject_single_bit_error((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
                break;
            case 1:
                break;
            case 2:
                flip_two_words((uint16_t *) &frame_buffer[i], (FRAME_SIZE - sizeof(Trailer)) >> 1);
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

    if (inet_pton(AF_INET, argv[2], &receiver_addr.sin_addr) <= 0)
        exit_with_error("inet_pton error for %s!", argv[1]);

    // Try to connect to the receiver
    if (connect(receiver_socket, (struct sockaddr *) &receiver_addr, (socklen_t) sizeof(receiver_addr)) < 0)
        exit_with_error("Connection failed!");

    // Send the header containing total number of frames in the message
    uint32_t net_length = htonl(total_frames);
    uint8_t *buffer_ptr = (uint8_t *) &net_length;
    ssize_t total_bytes_sent = 0;
    while (total_bytes_sent < HEADER_SIZE) {
        ssize_t bytes_sent = send(receiver_socket, buffer_ptr + total_bytes_sent, HEADER_SIZE - total_bytes_sent, 0);
        if (bytes_sent < 0)
            exit_with_error("Send Failed!");
        total_bytes_sent += bytes_sent;
    }

    // Sending the total message to the receiver
    total_bytes_sent = 0;
    ssize_t total_size = total_frames * FRAME_SIZE;
    buffer_ptr = (uint8_t *) frame_buffer;
    while (total_bytes_sent < total_size) {
        ssize_t bytes_sent = send(receiver_socket, buffer_ptr + total_bytes_sent, total_size - total_bytes_sent, 0);
        if (bytes_sent < 0)
            exit_with_error("Send Failed!");
        total_bytes_sent += bytes_sent;
    }
    close(receiver_socket);
    free(frame_buffer);

    return 0;
}
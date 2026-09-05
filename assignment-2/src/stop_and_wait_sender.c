#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "common.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = max_delay_ms */
    if (argc < 4) {
        printf("Usage: ./main.out <IP address> <file> <max_delay_ms>\n");
        return 1;
    }

    // Create CRC-32 table
    create_crc32_table();

    // Seed the random number generator
    srand((unsigned int) time(NULL));

    // Chunk the input file into frames
    uint32_t total_frames = 0;
    Frame *frame_buffer = chunk_file(argv[3], &total_frames);

    for (uint32_t i = 0; i < total_frames; i++) {
        input_mac_address(&frame_buffer[i]);

        uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
        frame_buffer[i].trailer.fcs[0] = (uint8_t) (crc32 >> 24);
        frame_buffer[i].trailer.fcs[1] = (uint8_t) (crc32 >> 16);
        frame_buffer[i].trailer.fcs[2] = (uint8_t) (crc32 >> 8);
        frame_buffer[i].trailer.fcs[3] = (uint8_t) (crc32);
    }

#ifdef INJECT_ERROR
    // Inject an error
    ErrorType error;
    for (uint32_t i = 0; i < total_frames; i++) {
        printf("--- FRAME #%u ---\n", i+1);
        error = rand() % ERROR_NUM;
        printf("ERROR: ");
        switch (error) {
            case SINGLE_BIT:
                inject_single_bit_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("SINGLE\n");
                break;
            case TWO_ISOLATED:
                inject_two_isolated_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("ISOLATED\n");
                break;
            case ODD_ERRORS:
                inject_odd_errors((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("ODD\n");
                break;
            case BURST:
                inject_burst_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("BURST\n");
                break;
            case NO_ERROR:
                printf("NONE\n");
        }
    }
#endif

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

    /* Implement the Stop-and-Wait sender-side logic here */
    // Sending the total message to the receiver
    // total_bytes_sent = 0;
    // ssize_t total_size = total_frames * FRAME_SIZE;
    // buffer_ptr = (uint8_t *) frame_buffer;
    // while (total_bytes_sent < total_size) {
    //     ssize_t bytes_sent = send(receiver_socket, buffer_ptr + total_bytes_sent, total_size - total_bytes_sent, 0);
    //     if (bytes_sent < 0)
    //         exit_with_error("Send Failed!");
    //     total_bytes_sent += bytes_sent;
    // }
    close(receiver_socket);
    free(frame_buffer);

    return 0;
}
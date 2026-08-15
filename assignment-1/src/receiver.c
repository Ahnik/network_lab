#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "common.h"
#include "crc.h"

int main() {
    // Create the CRC lookup tables
    create_crc8_table();
    create_crc10_table();
    create_crc16_table();
    create_crc32_table();

    // Create the socket to listen to connection requests
    int receiver_socket;
    if ((receiver_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        exit_with_error("Failed to create socket!");

    // Set the reuse socket option
    int opt = 1;
    if (setsockopt(receiver_socket, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t) sizeof(opt)) < 0)
        exit_with_error("Set socket options failed!");

    // Initialize the address struct
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family      = AF_INET;
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_addr.sin_port        = htons(RECEIVER_PORT);

    // Bind the address to the socket created
    if (bind(receiver_socket, (struct sockaddr *) &receiver_addr, (socklen_t) sizeof(receiver_addr)) < 0)
        exit_with_error("Bind Failed!");

    // Listen to connection requests
    if (listen(receiver_socket, 1) < 0)
        exit_with_error("Listen Failed!");

    int sender_socket;
    struct sockaddr_in sender_addr;
    socklen_t addr_size = (socklen_t) sizeof(sender_addr);
    while (true) {
        printf("Waiting for connection...\n");

        // Accept connection from sender
        if ((sender_socket = accept(receiver_socket, (struct sockaddr *) &sender_addr, &addr_size)) < 0)
            exit_with_error("Accept Failed!");

        // Receive the header containing number of frames in the message
        uint32_t total_frames = read_payload_len(sender_socket);

        Frame *frame_buffer = (Frame *) calloc(total_frames, sizeof(Frame));
        if (frame_buffer == NULL)
            exit_with_error("Memory allocation error!");

        for (uint32_t i = 0; i < total_frames; i++) {
            uint8_t *frame_ptr = (uint8_t *) &frame_buffer[i];
            ssize_t total_bytes_read = 0;
            while (total_bytes_read < FRAME_SIZE) {
                ssize_t bytes_read = recv(sender_socket, frame_ptr + total_bytes_read, FRAME_SIZE - total_bytes_read, 0);
                if (bytes_read <= 0)
                    exit_with_error("recv failed!");
                total_bytes_read += bytes_read;
            }

            /* TODO: Check what error detection scheme is used */
            printf("--- FRAME #%u ---\n", i+1);
            printf("Payload extracted : %hu bytes\n", frame_buffer[i].header.length);
            uint16_t code = ntohs(frame_buffer[i].header.type);
            printf("%s : ", code_to_string(code));
            switch (code) {
                case CHECKSUM:
                    if (find_checksum((uint16_t *) &frame_buffer[i], (PAYLOAD_SIZE + sizeof(Header) + 2) >> 1) == 0)
                        printf("VALID\n");
                    else
                        printf("CORRUPTED\n");
                    break;
                case CRC8:
                    if (compute_crc8(&frame_buffer[i], PAYLOAD_SIZE + sizeof(Header) + 1) == 0) 
                        printf("VALID\n");
                    else
                        printf("CORRUPTED\n");
                    break;
                case CRC10:
                    if (compute_crc10(&frame_buffer[i], PAYLOAD_SIZE + sizeof(Header) + 2) == 0)
                        printf("VALID\n");
                    else
                        printf("CORRUPTED\n");
                    break;
                case CRC16:
                    if (compute_crc16(&frame_buffer[i], PAYLOAD_SIZE + sizeof(Header) + 2) == 0)
                        printf("VALID\n");
                    else
                        printf("CORRUPTED\n");
                    break;
                case CRC32:
                    if (compute_crc32(&frame_buffer[i], PAYLOAD_SIZE + sizeof(Header) + 4) == 0)
                        printf("VALID\n");
                    else
                        printf("CORRUPTED\n");
                    break;
                default:
                    printf("CORRUPTED\n");
            }
        }
        close(sender_socket);
        free(frame_buffer);
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    }

    return 0;
}
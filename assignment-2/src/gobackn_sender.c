#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "common.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = m, argv[4] = max_delay_ms, argv[5] = timeout_ms, argv[6] = probability of error per frame */
    if (argc < 7) {
        printf("Usage: ./stop_and_wait_sender <IP address> <file> <seq no. bits> <max_delay_ms> <timeout_ms> <per_frame_error>\n");
        return 1;
    }

    // Set the max delay and timer
    int timeout_ms = atoi(argv[5]);
    int max_delay_ms = atoi(argv[4]);
    int m = atoi(argv[3]);
    double per_frame_error;
    sscanf(argv[6], "%lf", &per_frame_error);

    // Create CRC-32 table
    create_crc32_table();

    // Seed the random number generator
    srand((unsigned int) time(NULL));

    // Chunk the input file into frames
    uint32_t total_frames = 0;
    Frame *frame_buffer = chunk_file(argv[2], &total_frames);

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

    /* Implement Go-Back-N logic here */
    uint8_t sw = (1 << m) - 1;
    uint8_t sf = 0;
    uint8_t sn = 0;
    uint32_t index = 0;
    AckFrame ack;
    Frame temp_frame;

    while (index < total_frames) {
        if (sn - sf < sw) {
            // Enter MAC address, sequence number and CRC
            frame_buffer[index].header.seq_no = sn;
            input_mac_address(&frame_buffer[index]);
            uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[index], FRAME_SIZE - sizeof(Trailer));
            frame_buffer[index].trailer.fcs[0] = (uint8_t) (crc32 >> 24);
            frame_buffer[index].trailer.fcs[1] = (uint8_t) (crc32 >> 16);
            frame_buffer[index].trailer.fcs[2] = (uint8_t) (crc32 >> 8);
            frame_buffer[index].trailer.fcs[3] = (uint8_t) (crc32);

            memcpy(&temp_frame, &frame_buffer[index], FRAME_SIZE);
            inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
            inject_random_delay(max_delay_ms);
            if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                index++;
                sn = (sn + 1) % (1 << m);
            }
        }

        // Receive acknowledgement from receiver
        int ret = receive_ack_with_timeout(&ack, receiver_socket, timeout_ms);
        if (ret > 0) {
            if (compute_crc32((uint8_t *) &ack, ACK_SIZE) != 0)
                continue;
            else if (ack.ack_no > sf && ack.ack_no <= sn)
                sf = ack.ack_no + 1;
        } else {
            // If ACK is not received, resend all frames that are not acknowledged
            size_t offset = (size_t) (sn >= sf) ? (sn - sf) : (sn + (1 << m) - sf);
            for (size_t i = index - offset; i < index; i++) {
                memcpy(&temp_frame, &frame_buffer[index], FRAME_SIZE);
                inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
                inject_random_delay(max_delay_ms);
                send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket);
            }
        }
    }

    close(receiver_socket);
    free(frame_buffer);

    return 0;
}
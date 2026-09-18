#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "common.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = max_delay_ms, argv[4] = timeout_ms, argv[5] = probability of error per frame */
    if (argc < 6) {
        printf("Usage: ./stop_and_wait_sender <IP address> <file> <max_delay_ms> <timeout_ms> <per_frame_error>\n");
        return 1;
    }

    // Set the max delay and timer
    int timeout_ms = atoi(argv[4]);
    int max_delay_ms = atoi(argv[3]);
    double per_frame_error;
    sscanf(argv[5], "%lf", &per_frame_error);

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

    /* Implement the Stop-and-Wait sender-side logic here */
    uint8_t sn = 0;
    uint32_t index = 0;
    AckFrame ack;
    Frame temp_frame;
    uint32_t frames_sent = 0;
    uint32_t ack_received = 0;
    bool can_send = true;

    while (index < total_frames) {
        frame_buffer[index].header.seq_no = sn;
        input_mac_address(&frame_buffer[index]);
        uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[index], FRAME_SIZE - sizeof(Trailer));
        frame_buffer[index].trailer.fcs[0] = (uint8_t) (crc32 >> 24);
        frame_buffer[index].trailer.fcs[1] = (uint8_t) (crc32 >> 16);
        frame_buffer[index].trailer.fcs[2] = (uint8_t) (crc32 >> 8);
        frame_buffer[index].trailer.fcs[3] = (uint8_t) (crc32);

        // Send the frame if it can be sent
        if (can_send) {
            memcpy(&temp_frame, &frame_buffer[index], FRAME_SIZE);
            inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
            inject_random_delay(max_delay_ms);
            if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                index++;
                sn = (sn + 1) % 2;
            }
            frames_sent++;
            can_send = false;
        }

        // Receive acknowledgement from sender
        int ret = receive_ack_with_timeout(&ack, receiver_socket, timeout_ms);
        if (ret == 0) {
            // Resend the frame
            memcpy(&temp_frame, &frame_buffer[index-1], FRAME_SIZE);
            inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
            inject_random_delay(max_delay_ms);
            send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket);
            frames_sent++;
        } else {
            ack_received++;
            if (compute_crc32((uint8_t *) &ack, ACK_SIZE) == 0 && ack.ack_no == sn)
                can_send = true;
        }
    }

    // Print statistics
    printf("Total frames: %u\n", total_frames);
    printf("Frames sent: %u\n", frames_sent);
    printf("Acknowledgements received: %u\n", ack_received);

    // for (uint32_t i = 0; i < total_frames; i++) {
    //     // Enter MAC address, sequence number and CRC
    //     frame_buffer[i].header.seq_no = seq_no;
    //     input_mac_address(&frame_buffer[i]);
    //     uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[i], FRAME_SIZE - sizeof(Trailer));
    //     frame_buffer[i].trailer.fcs[0] = (uint8_t) (crc32 >> 24);
    //     frame_buffer[i].trailer.fcs[1] = (uint8_t) (crc32 >> 16);
    //     frame_buffer[i].trailer.fcs[2] = (uint8_t) (crc32 >> 8);
    //     frame_buffer[i].trailer.fcs[3] = (uint8_t) (crc32);

    //     seq_no = (seq_no + 1) % 2;
    //     int ret = 0;
    //     int count = 0;

    //     do {
    //         count++;
    //         memcpy(&temp_frame, &frame_buffer[i], FRAME_SIZE);
    //         inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
    //         inject_random_delay(max_delay_ms);
    //         send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket);

    //         ret = receive_ack_with_timeout(&ack, receiver_socket, timeout_ms);
    //         if (ret > 0 && (compute_crc32((uint8_t *) &ack, ACK_SIZE) != 0 || ack.ack_no != seq_no))
    //             ret = 0;
    //     } while (ret == 0);

    //     printf("Frame #%u successfully received! Seq no - %d! No. of frame transmissions - %d!\n", i+1, frame_buffer[i].header.seq_no, count);
    // }

    close(receiver_socket);
    free(frame_buffer);

    return 0;
}
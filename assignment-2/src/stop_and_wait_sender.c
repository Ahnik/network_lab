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
#include "receiver.h"
#include "ring_buffer.h"

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = max_delay_ms, argv[4] = timeout_ms, argv[5] = probability of error per frame */
    if (argc < 6) {
        printf("Usage: ./stop_and_wait_sender <IP address> <file> <max_delay_ms> <timeout_ms> <per_frame_error>\n");
        return 1;
    }

    // Set the initial variables
    int timeout_ms = atoi(argv[4]);
    int max_delay_ms = atoi(argv[3]);
    double per_frame_error;
    sscanf(argv[5], "%lf", &per_frame_error);

    struct timespec start, end;
    long seconds, nanoseconds;
    long long total_ns;
    double total_ms;
    clock_gettime(CLOCK_MONOTONIC, &start);

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
    ControlFrame ack;
    Frame temp_frame;
    uint32_t frames_sent = 0;
    uint32_t ack_received = 0;
    uint32_t erroneous_ack = 0;
    uint32_t out_of_order_ack = 0;
    Receiver receiver = {
        .sockfd           = receiver_socket,
        .client_connected = true,
    };
    rb_init(&receiver.rb, 1);
    bool can_send = true;

    struct timespec expiry_time;
    bool timer_active = false;
    pthread_create(&receiver.thread, NULL, receiver_function, &receiver);

    while (index < total_frames) {
        // If the timer is off, turn it on
        if (timer_active == false) {
            // Start the timer
            timer_active = true;
            expiry_time = get_deadline(timeout_ms);
        }

        // Send the frame if it can be sent
        if (can_send) {
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
            send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket);
            printf("Frame #%u sent! Seq no - %u!\n", index+1, frame_buffer[index].header.seq_no);
            frames_sent++;
            index++;
            sn = (sn + 1) % 2;
            can_send = false;
        }

        // Check if the client is connected
        pthread_mutex_lock(&receiver.lock);
        if (receiver.client_connected == false) {
            pthread_mutex_unlock(&receiver.lock);
            break;
        }

        // Check if there is an ACK frame or not
        while (receiver.rb.count > 0) {
            rb_pop_front(&receiver.rb, &ack);
            ack_received++;
            if (compute_crc32((uint8_t *) &ack, CONTROL_FRAME_SIZE) == 0) {
                if (ack.seq_no == sn) {
                    printf("Received ACK %u!\n", ack.seq_no);
                    can_send = true;
                } else {
                    out_of_order_ack++;
                    printf("Out-of-order ACK discarded! Seq no - %u!\n", ack.seq_no);
                }
            } else {
                erroneous_ack++;
                printf("Corrupt ACK discarded! Seq no - %u!\n", ack.seq_no);
            }
            // Stop the timer
            timer_active = false;
        }
        pthread_mutex_unlock(&receiver.lock);

        if (timer_active && is_expired(expiry_time)) {
            // Restart the timer
            expiry_time = get_deadline(timeout_ms);

            // Resend the frame
            memcpy(&temp_frame, &frame_buffer[index-1], FRAME_SIZE);
            inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
            inject_random_delay(max_delay_ms);
            send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket);
            printf("Frame #%u resent! Seq no - %u!\n", index, frame_buffer[index-1].header.seq_no);
            frames_sent++;
        }
    }

    // Print statistics
    double efficiency = (double) total_frames / (double) frames_sent;
    fprintf(stderr, "Total frames: %u\n", total_frames);
    fprintf(stderr, "Frames sent: %u\n", frames_sent);
    fprintf(stderr, "Efficiency: %lf\n", efficiency);
    fprintf(stderr, "ACKs received: %u\n", ack_received);
    fprintf(stderr, "Erroneous ACKs: %u\n", erroneous_ack);
    fprintf(stderr, "Out-of-order ACKs: %u\n", out_of_order_ack);

    // Record the end time
    clock_gettime(CLOCK_MONOTONIC, &end);
    nanoseconds = end.tv_nsec - start.tv_nsec;
    seconds = end.tv_sec - start.tv_sec;

    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000L;
    }

    total_ns = (seconds * 1000000000LL) + nanoseconds;
    total_ms = (double) total_ns / 1000000.0;
    fprintf(stderr, "Execution time: %.6f ms\n", total_ms);

    pthread_join(receiver.thread, NULL);
    close(receiver_socket);
    free(frame_buffer);

    return 0;
}
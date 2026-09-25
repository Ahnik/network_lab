#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "common.h"
#include "error_injector.h"
#include "receiver.h"
#include "ring_buffer.h"

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = max_delay_ms, argv[4] = timeout_ms, argv[5] = probability of error per frame, argv[6] = seq. no. bits */
    if (argc < 7) {
        printf("Usage: ./sr_sender <IP address> <file> <max_delay_ms> <timeout_ms> <per_frame_error> <seq no. bits>\n");
        return 1;
    }

    // Set the initial variables
    int timeout_ms   = atoi(argv[4]);
    int max_delay_ms = atoi(argv[3]);
    int m = atoi(argv[6]);
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

    /* Implement Go-Back-N logic here */
    uint8_t sw = 1 << (m-1);
    uint8_t max_seq_no = (1 << m) - 1;
    uint8_t sf = 0;
    uint8_t sn = 0;
    uint32_t index = 0;
    Frame temp_frame;
    uint32_t frames_sent                = 0;
    uint32_t ack_accepted               = 0;
    uint32_t nak_accepted               = 0;
    uint32_t erroneous_control_frame    = 0;
    uint32_t out_of_order_control_frame = 0;
    ControlFrame control_frame;
    Receiver receiver = {
        .sockfd           = receiver_socket,
        .client_connected = true,
    };
    rb_init(&receiver.rb, 1 << m);
    if (receiver.rb.buffer == NULL)
        exit_with_error("Memory allocation error!\n");

    WindowSlot *send_window  = (WindowSlot *) calloc(1 << m, sizeof(WindowSlot));
    if (send_window == NULL)
        exit_with_error("Memory allocation error!");
    for (int i = 0; i < (1 << m); i++) {
        send_window[i].timer_active = false;
        send_window[i].acked = false;
    }
    pthread_create(&receiver.thread, NULL, receiver_function, &receiver);

    while (index < total_frames || sf != index) {
        if (((sn - sf) & max_seq_no) < sw && index < total_frames) {
            // Enter MAC address, sequence number and CRC
            frame_buffer[index].header.seq_no = sn;
            input_mac_address(&frame_buffer[index]);
            uint32_t crc32 = compute_crc32((uint8_t *) &frame_buffer[index], FRAME_SIZE - sizeof(Trailer));
            frame_buffer[index].trailer.fcs[0] = (uint8_t) (crc32 >> 24);
            frame_buffer[index].trailer.fcs[1] = (uint8_t) (crc32 >> 16);
            frame_buffer[index].trailer.fcs[2] = (uint8_t) (crc32 >> 8);
            frame_buffer[index].trailer.fcs[3] = (uint8_t) (crc32);
            send_window[sn].index = index;

            // If timer is not active, turn it back on
            send_window[sn].timer_active = true;
            send_window[sn].expiry_time = get_deadline(timeout_ms);

            memcpy(&temp_frame, &frame_buffer[index], FRAME_SIZE);
            inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
            inject_random_delay(max_delay_ms);
            if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                printf("Frame #%u sent! Seq no - %u!\n", index+1, frame_buffer[index].header.seq_no);
                frames_sent++;
                index++;
                sn = (sn + 1) & max_seq_no;
            }
        }

        // Check if the client is connected
        pthread_mutex_lock(&receiver.lock);
        if (receiver.client_connected == false) {
            pthread_mutex_unlock(&receiver.lock);
            break;
        }

        // Check if there is any ACK or NAK frames received
        while (receiver.rb.count > 0) {
            rb_pop_front(&receiver.rb, &control_frame);
            uint8_t diff = (control_frame.seq_no - sf) & max_seq_no;
            if (compute_crc32((uint8_t *) &control_frame, CONTROL_FRAME_SIZE) == 0) {
                if (diff < ((sn - sf) & max_seq_no)) {
                    if (control_frame.frame_type == ACK_FRAME) {
                        ack_accepted++;
                        printf("ACK %u received!\n", control_frame.seq_no);
                        send_window[control_frame.seq_no].acked = true;
                        send_window[control_frame.seq_no].timer_active = false;
                        while (send_window[sf].acked && sf != sn) {
                            send_window[sf].acked = false;
                            send_window[sf].timer_active = false;
                            sf = (sf + 1) & max_seq_no;
                        }
                    } else if (control_frame.frame_type == NAK_FRAME) {
                        nak_accepted++;
                        printf("NAK %u received!\n", control_frame.seq_no);
                        uint32_t i = send_window[control_frame.seq_no].index;
                        memcpy(&temp_frame, &frame_buffer[i], FRAME_SIZE);
                        inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
                        inject_random_delay(max_delay_ms);
                        if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                            printf("Frame #%u resent! Seq no - %u!\n", i+1, frame_buffer[i].header.seq_no);
                            frames_sent++;
                        }
                        send_window[control_frame.seq_no].timer_active = true;
                        send_window[control_frame.seq_no].expiry_time = get_deadline(timeout_ms);
                    }
                } else {
                    out_of_order_control_frame++;
                    printf("Out-of-order control frame #%u discarded!\n", control_frame.seq_no);
                }
            } else {
                erroneous_control_frame++;
                printf("Corrupt control frame #%u discarded!\n", control_frame.seq_no);
            }
        }
        pthread_mutex_unlock(&receiver.lock);

        // Check if timeout occurred, then retransmit the frame
        for (uint8_t i = sf; i != sn; i = (i + 1) & max_seq_no) {
            if (send_window[i].timer_active && is_expired(send_window[i].expiry_time)) {
                // Restart the timer
                send_window[i].expiry_time = get_deadline(timeout_ms);

                // Resend the frame
                memcpy(&temp_frame, &frame_buffer[send_window[i].index], FRAME_SIZE);
                inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
                if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                    printf("Frame #%u resent! Seq no - %u!\n", send_window[i].index+1, frame_buffer[send_window[i].index].header.seq_no);
                    frames_sent++;
                }
            }
        }
    }

    // Print the statistics
    double efficiency = (double) total_frames / (double) frames_sent;
    fprintf(stderr, "Total frames: %u\n", total_frames);
    fprintf(stderr, "Frames sent: %u\n", frames_sent);
    fprintf(stderr, "Efficiency: %lf\n", efficiency);
    fprintf(stderr, "ACKs received: %u\n", ack_accepted);
    fprintf(stderr, "NAKs accepted: %u\n", nak_accepted);
    fprintf(stderr, "Erroneous control frames: %u\n", erroneous_control_frame);
    fprintf(stderr, "Out-of-order control frames: %u\n", out_of_order_control_frame);

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
    rb_free(&receiver.rb);
    close(receiver_socket);
    free(frame_buffer);
    free(send_window);

    return 0;
}
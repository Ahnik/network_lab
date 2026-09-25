#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include "common.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    int m;      // Number of bits used for sequence number
    if (argc < 3) {
        printf("Usage: ./gobackn_receiver <max_delay_ms> <per_frame_error> <seq no. bits>\n");
        return 1;
    }
    else if (argc == 3) m = 1;
    else                m = atoi(argv[3]);

    int max_delay_ms = atoi(argv[1]);
    double per_frame_error;
    sscanf(argv[2], "%lf", &per_frame_error);

    // Ignore the SIGPIPE interrupt so that the server process doesn't get terminated due to a broken pipe
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    // Create the CRC lookup tables
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
        // Accept connection from sender
        if ((sender_socket = accept(receiver_socket, (struct sockaddr *) &sender_addr, &addr_size)) < 0)
            exit_with_error("Accept Failed!");

        // Receive the header containing number of frames in the message
        uint32_t total_frames = read_payload_len(sender_socket);

        Frame *frame_buffer = (Frame *) calloc(total_frames, sizeof(Frame));
        if (frame_buffer == NULL)
            exit_with_error("Memory allocation error!");

        uint8_t rn = 0;
        uint8_t rsize = 1 << (m-1);
        uint8_t max_seq_no = (1 << m) - 1;
        uint32_t index = 0;
        uint32_t frames_received     = 0;
        uint32_t corrupt_frames      = 0;
        uint32_t out_of_order_frames = 0;
        uint32_t ack_sent            = 0;
        uint32_t nak_sent            = 0;
        bool is_nak_sent = false;
        bool *marked = (bool *) calloc(1 << m, sizeof(bool));
        if (marked == NULL) {
            fprintf(stderr, "Memory allocation error!\n");
            free(frame_buffer);
            continue;
        }
        for (int i = 0; i < (1 << m); i++) {
            marked[i] = false;
        }

        while (index < total_frames) {
            if (receive_in_buffer((uint8_t *) &frame_buffer[index], FRAME_SIZE, sender_socket) != 0) break;
            frames_received++;
            uint8_t diff = (frame_buffer[index].header.seq_no - rn) & max_seq_no;
            if (compute_crc32((uint8_t *) &frame_buffer[index], FRAME_SIZE) != 0) {
                printf("Corrupt frame #%u discarded!\n", frame_buffer[index].header.seq_no);
                corrupt_frames++;
                if (is_nak_sent == false) {
                    is_nak_sent = true;
                    inject_random_delay(max_delay_ms);
                    send_control_frame_with_error(NAK_FRAME, rn, sender_socket, per_frame_error);
                    printf("Sent NAK %u!\n", rn);
                    nak_sent++;
                }
            } else if (frame_buffer[index].header.seq_no == rn) {
                printf("Frame #%u received! Seq no - %d!\n", index+1, frame_buffer[index].header.seq_no);
                marked[rn] = true;
                inject_random_delay(max_delay_ms);
                send_control_frame_with_error(ACK_FRAME, rn, sender_socket, per_frame_error);
                printf("Sent ACK %u!\n", rn);
                ack_sent++;
                is_nak_sent = false;
                while (marked[rn]) {
                    marked[rn] = false;
                    rn = (rn + 1) & max_seq_no;
                    index++;
                }
            } else if (diff < rsize && index + diff < total_frames) {
                marked[frame_buffer[index].header.seq_no] = true;
                memcpy(&frame_buffer[index + diff], &frame_buffer[index], FRAME_SIZE);
                printf("Frame #%u received out-of-order! Seq no - %d!\n", index+diff+1, frame_buffer[index+diff].header.seq_no);
                inject_random_delay(max_delay_ms);
                send_control_frame_with_error(ACK_FRAME, frame_buffer[index + diff].header.seq_no, sender_socket, per_frame_error);
                printf("Sent ACK %u!\n", frame_buffer[index + diff].header.seq_no);
                if (is_nak_sent == false) {
                    is_nak_sent = true;
                    send_control_frame_with_error(NAK_FRAME, rn, sender_socket, per_frame_error);
                    printf("Sent NAK %u!\n", rn);
                }
                ack_sent++;
            } else {
                printf("Out-of-order frame #%u discarded!\n", frame_buffer[index].header.seq_no);
                out_of_order_frames++;
                send_control_frame_with_error(ACK_FRAME, frame_buffer[index].header.seq_no, sender_socket, per_frame_error);
                printf("Sent ACK %u!\n", frame_buffer[index].header.seq_no);
                ack_sent++;
            }
        }

        // Print statistics
        double efficiency = (double) total_frames / (double) frames_received;
        fprintf(stderr, "Total frames in the message: %u\n", total_frames);
        fprintf(stderr, "Frames received: %u\n", frames_received);
        fprintf(stderr, "Efficiency: %lf\n", efficiency);
        fprintf(stderr, "Corrupted frames: %u\n", corrupt_frames);
        fprintf(stderr, "Out-of-order invalid frames: %u\n", out_of_order_frames);
        fprintf(stderr, "ACKs sent: %u\n", ack_sent);
        fprintf(stderr, "NAKs sent: %u\n", nak_sent);
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

        close(sender_socket);
        free(frame_buffer);
        free(marked);
    }

    return 0;
}
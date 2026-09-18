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
    if (argc < 4) {
        printf("Usage: ./stop_and_wait_receiver <seq no. bits> <max_delay_ms> <per_frame_error>\n");
        return 1;
    }

    // Set the max delay
    int max_delay_ms = atoi(argv[2]);
    int m = atoi(argv[1]);
    double per_frame_error;
    sscanf(argv[3], "%lf", &per_frame_error);

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

        /* TODO: Write the algorithm for receiving the frames in Go-Back-N */
        uint8_t rn = 0;
        size_t index = 0;

        // Receive frame and send ACK
        while (index < total_frames) {
            receive_in_buffer((uint8_t *) &frame_buffer[index], FRAME_SIZE, sender_socket);
            if (compute_crc32((uint8_t *) &frame_buffer[index], FRAME_SIZE) == 0 && frame_buffer[index].header.seq_no == rn) {
                rn = (rn + 1) % (1 << m);
                index++;
                send_ack_with_error(rn, sender_socket, per_frame_error);
            }
        }

        close(sender_socket);
        free(frame_buffer);
    }

    return 0;
}
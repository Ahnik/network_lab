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

#define READ_END  0
#define WRITE_END 1

int main(int argc, char **argv) {
    /* argv[1] = IP address, argv[2] = file, argv[3] = max_delay_ms, argv[4] = timeout_ms, argv[5] = probability of error per frame, argv[6] = seq. no. bits */
    if (argc < 7) {
        printf("Usage: ./gobackn_sender <IP address> <file> <max_delay_ms> <timeout_ms> <per_frame_error> <seq no. bits>\n");
        return 1;
    }

    // Set the initial variables
    int timeout_ms   = atoi(argv[4]);
    int max_delay_ms = atoi(argv[3]);
    int m = atoi(argv[6]);
    double per_frame_error;
    sscanf(argv[5], "%lf", &per_frame_error);

    // Pipe for sending the stop signal to the receiver thread
    int stop_pipe[2];
    pipe(stop_pipe);

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
    Frame temp_frame;
    uint32_t frames_sent   = 0;
    uint32_t ack_received  = 0;
    uint32_t ack_discarded = 0;
    Receiver receiver = {
        .event            = RECEIVER_EVENTS,
        .client_connected = true,
        .is_running       = false,
        .sockfd           = receiver_socket,
        .stopfd           = stop_pipe[READ_END],
        .timeout          = timeout_ms
    };
    pthread_mutex_init(&receiver.lock, NULL);
    pthread_cond_init(&receiver.cond, NULL);
    pthread_create(&receiver.thread, NULL, receiver_function, &receiver);

    /* Implement the Selective Repeat ARQ algorithm */
    while (index < total_frames || sf != index) {
        if (((sn - sf) & sw) < sw && index < total_frames) {
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
                printf("Frame #%u sent! Seq no - %u!\n", index+1, frame_buffer[index].header.seq_no);
                frames_sent++;
                index++;
                sn = (sn + 1) & sw;
            }
        }

        // Check if the receiver thread is stopped and if it is, then start it.
        if (pthread_mutex_trylock(&receiver.lock) != EBUSY) {
            if (receiver.client_connected == false) {
                pthread_mutex_unlock(&receiver.lock);
                break;
            }
            if (receiver.is_running == false) {
                receiver.is_running = true;
                pthread_cond_signal(&receiver.cond);
            }
            pthread_mutex_unlock(&receiver.lock);
        }

        pthread_mutex_lock(&receiver.lock);
        // Check if there is a notification or not
        if (receiver.event == ACK_RECEIVED) {
            ack_received++;
            uint8_t diff = (receiver.frame.seq_no - sf) & sw;
            if (compute_crc32((uint8_t *) &receiver.frame, ACK_SIZE) == 0 && diff > 0 && diff <= ((sn - sf) & sw)) {
                if (receiver.frame.frame_type == NAK_FRAME) {
                    uint32_t offset = (uint32_t) ((sn - receiver.frame.seq_no) & sw);
                    memcpy(&temp_frame, &frame_buffer[index-offset], FRAME_SIZE);
                    inject_error((uint8_t *) &temp_frame, FRAME_SIZE, per_frame_error);
                    inject_random_delay(max_delay_ms);
                    if (send_from_buffer((uint8_t *) &temp_frame, FRAME_SIZE, receiver_socket) == 0) {
                        printf("Frame #%u resent! Seq no - %u!\n", index-offset+1, frame_buffer[index-offset].header.seq_no);
                        frames_sent++;
                    }
                } else if (receiver.frame.frame_type == ACK_FRAME) {
                    printf("ACK %u received!\n", receiver.frame.seq_no);
                    sf = receiver.frame.seq_no;
                }
            } else {
                ack_discarded++;
                printf("Corrupted ACK discarded! Seq no - %u!\n", receiver.frame.seq_no);
            }
            // Interrupt the receiver thread to stop the timer
            receiver.event = INTERRUPTED;
            if (receiver.is_running)
                write(stop_pipe[WRITE_END], "x", 1);
        }

        // If there is a timeout
        if (receiver.event == TIMEOUT) {
            // Restart the timer
            if (receiver.is_running == false) {
                receiver.is_running = true;
                pthread_cond_signal(&receiver.cond);
            }
            pthread_mutex_unlock(&receiver.lock);
        } else
            pthread_mutex_unlock(&receiver.lock);
    }

    // Print the statistics
    printf("Total frames: %u\n", total_frames);
    printf("Frames sent: %u\n", frames_sent);
    printf("Acknowledgements received: %u\n", ack_received);
    printf("Acknowledgements discarded: %u\n", ack_discarded);

    pthread_join(receiver.thread, NULL);
    close(receiver_socket);
    free(frame_buffer);
    close(stop_pipe[READ_END]);
    close(stop_pipe[WRITE_END]);

    return 0;
}
#define _POSIX_C_SOURCE 200809L

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

#ifdef BENCHMARK
    struct timespec start, end;
    long seconds, nanoseconds;
    long long total_ns;
    double total_ms;
    char print_buffer[PRINT_BUFFER_SIZE];
    size_t msg_size;
    memset(print_buffer, 0, PRINT_BUFFER_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

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

#ifdef BENCHMARK
    double average_time_ms;
    struct timespec begin_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &begin_compute);
#endif

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
#ifdef BENCHMARK
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
    seconds = end_compute.tv_sec - begin_compute.tv_sec;
    nanoseconds = end_compute.tv_nsec - begin_compute.tv_nsec;

    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000L;
    }
    total_ns = (seconds * 1000000000LL) + nanoseconds;
    total_ms = (double) total_ns / 1000000.0;
    average_time_ms = total_ms / total_frames;
    snprintf(print_buffer, PRINT_BUFFER_SIZE, "Average time to compute %s of frame", code_to_string(code));
    print_buffer[PRINT_BUFFER_SIZE-1] = 0;
    msg_size = strlen(print_buffer);
    fprintf(stderr, "%s : %.6f ms\n", print_buffer, average_time_ms);
#endif

    // Inject an error
    ErrorType error;
    for (uint32_t i = 0; i < total_frames; i++) {
        printf("--- FRAME #%u ---\n", i+1);
        error = rand() % ERROR_NUM;
        switch (error) {
            case SINGLE_BIT:
                inject_single_bit_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("Single bit error injected!\n");
                break;
            case TWO_ISOLATED:
                inject_two_isolated_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("Two isolated single bit errors injected!\n");
                break;
            case ODD_ERRORS:
                inject_odd_errors((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("Odd errors injected!\n");
                break;
            case BURST:
                inject_burst_error((uint8_t *) &frame_buffer[i], FRAME_SIZE);
                printf("Burst error injected!\n");
                break;
            case NO_ERROR:
                printf("No error injected!\n");
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

#ifdef BENCHMARK
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
    memset(print_buffer, PRINT_BUFFER_SIZE, 0);
    snprintf(print_buffer, PRINT_BUFFER_SIZE, "Execution time");
    size_t curr_size = strlen(print_buffer);
    if (msg_size > curr_size) {
        memset(&print_buffer[curr_size], ' ', msg_size - curr_size);
        print_buffer[msg_size] = 0;
    }
    print_buffer[PRINT_BUFFER_SIZE-1] = 0;
    fprintf(stderr, "%s : %.6f ms\n", print_buffer, total_ms);
#endif

    return 0;
}
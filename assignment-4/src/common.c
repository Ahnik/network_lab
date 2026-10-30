#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

uint32_t crc32_table[CRC_TABLE_SIZE];

void exit_with_error(const char *fmt, ...) {
    int errno_save = errno;
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (errno_save != 0) {
        fprintf(stderr, "(errno = %d) : %s\n\n", errno_save, strerror(errno_save));
        fflush(stderr);
    }
    va_end(ap);

    exit(EXIT_FAILURE);
}

void input_mac_address(Frame *frame, uint8_t sender_id, uint8_t receiver_id) {
    if (!frame) return;
    frame->header.sender_addr[0] = 0x02;
    frame->header.sender_addr[1] = 0x00;
    frame->header.sender_addr[2] = 0x00;
    frame->header.sender_addr[3] = 0x00;
    frame->header.sender_addr[4] = 0x00;
    frame->header.sender_addr[5] = sender_id;

    frame->header.receiver_addr[0] = 0x02;
    frame->header.receiver_addr[1] = 0x00;
    frame->header.receiver_addr[2] = 0x00;
    frame->header.receiver_addr[3] = 0x00;
    frame->header.receiver_addr[4] = 0x00;
    frame->header.receiver_addr[5] = receiver_id;

    frame->header.station_id = sender_id;
}

uint32_t read_payload_len(int socketfd) {
    if (socketfd < 0) return 0;
    uint32_t len_buf = 0;
    ssize_t bytesRead = 0;
    while (bytesRead < (ssize_t)sizeof(len_buf)) {
        ssize_t res = recv(socketfd, ((uint8_t*)&len_buf) + bytesRead, sizeof(len_buf) - bytesRead, 0);
        if (res < 0) return UINT32_MAX;
        else if (res == 0) return 0;
        bytesRead += res;
    }
    return ntohl(len_buf);
}

Frame *chunk_file(const char *filename, uint32_t *num_of_frames) {
    if (!filename || !num_of_frames) return NULL;
    FILE *filep = fopen(filename, "rb");
    if (filep == NULL)
        exit_with_error("Unable to open file '%s'", filename);

    if (fseek(filep, 0L, SEEK_END) != 0)
        exit_with_error("File seek error on '%s'", filename);
    long file_size = ftell(filep);
    if (file_size < 0)
        exit_with_error("Unable to determine size of file '%s'", filename);
    if (fseek(filep, 0L, SEEK_SET) != 0)
        exit_with_error("File seek error on '%s'", filename);

    long total_frames = (file_size + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;
    if (total_frames == 0) total_frames = 1; // At least one frame for 0-byte file

    Frame *frame_buffer = (Frame *) calloc(total_frames, sizeof(Frame));
    if (frame_buffer == NULL)
        exit_with_error("Memory allocation error for %ld frames", total_frames);

    for (long i = 0; i < total_frames; i++) {
        size_t bytesRead = fread(frame_buffer[i].payload, sizeof(uint8_t), PAYLOAD_SIZE, filep);
        frame_buffer[i].header.length = (uint16_t)bytesRead;
        frame_buffer[i].header.seq_no = (uint8_t)(i % 256);
        if (ferror(filep) != 0)
            exit_with_error("File read error on '%s'", filename);
    }

    fclose(filep);
    *num_of_frames = (uint32_t) total_frames;
    return frame_buffer;
}

void create_crc32_table(void) {
    for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint32_t reg = i << 24;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x80000000)
                reg = (reg << 1) ^ CRC32_GENERATOR;
            else
                reg <<= 1;
        }
        crc32_table[i] = reg;
    }
}

uint32_t compute_crc32(const uint8_t *buffer, size_t size) {
    if (buffer == NULL) return 0;
    uint32_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t pos = (uint8_t)((crc >> 24) ^ buffer[i]);
        crc = (crc << 8) ^ crc32_table[pos];
    }
    return crc;
}

int send_from_buffer(const uint8_t *buffer, long size, int socket) {
    long total_bytes_sent = 0;
    while (total_bytes_sent < size) {
        long bytes_sent = send(socket, buffer + total_bytes_sent, size - total_bytes_sent, 0);
        if (bytes_sent <= 0)
            return -1;
        total_bytes_sent += bytes_sent;
    }
    return 0;
}

int receive_in_buffer(uint8_t *buffer, long size, int socket) {
    long total_bytes_read = 0;
    while (total_bytes_read < size) {
        long bytes_read = recv(socket, buffer + total_bytes_read, size - total_bytes_read, 0);
        if (bytes_read <= 0)
            return -1;
        total_bytes_read += bytes_read;
    }
    return 0;
}

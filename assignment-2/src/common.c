#include "common.h"
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

void exit_with_error(const char *fmt, ...) {
    int errno_save = errno;
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (errno_save != 0) {
        fprintf(stderr, "(errno = %d) : %s\n", errno_save, strerror(errno_save));
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    va_end(ap);

    exit(1);
}

void input_mac_address(Frame *frame) {
    frame->header.sender_addr[0] = 0x70;
    frame->header.sender_addr[1] = 0x08;
    frame->header.sender_addr[2] = 0x94;
    frame->header.sender_addr[3] = 0x4a;
    frame->header.sender_addr[4] = 0x44;
    frame->header.sender_addr[5] = 0x1d;

    frame->header.receiver_addr[0] = 0xfe;
    frame->header.receiver_addr[1] = 0x80;
    frame->header.receiver_addr[2] = 0xde;
    frame->header.receiver_addr[3] = 0x3b;
    frame->header.receiver_addr[4] = 0xec;
    frame->header.receiver_addr[5] = 0x7a;
}

uint32_t read_payload_len(int sockfd){
    if(sockfd < 0) return 0;
    uint32_t *len_buf = (uint32_t *)calloc(1, sizeof(*len_buf));
    if(!len_buf) return 0;
    ssize_t bytesWritten = 0;
    while(bytesWritten < HEADER_SIZE){
        ssize_t bytesReceived = recv(sockfd, len_buf + bytesWritten, HEADER_SIZE - bytesWritten, 0);
        if(bytesReceived < 0)       return UINT32_MAX;
        else if(bytesReceived == 0) return 0;
        bytesWritten += bytesReceived;
    }
    uint32_t length = ntohl(*len_buf);
    free(len_buf);
    return length;
}

Frame *chunk_file(const char *filename, uint32_t *num_of_frames) {
    FILE *filep = fopen(filename, "rb");
    if (filep == NULL)
        exit_with_error("Unable to open file!");

    // Get the size of the file
    if (fseek(filep, 0L, SEEK_END) != 0)
        exit_with_error("File seek error!");
    long file_size = ftell(filep);
    if (file_size < 0)
        exit_with_error("Unable to tell size of file");
    if (fseek(filep, 0L, SEEK_SET) != 0)
        exit_with_error("File seek error!");

    long total_frames = (file_size + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;

    Frame *frame_buffer = (Frame *) calloc(total_frames, sizeof(Frame));
    if (frame_buffer == NULL)
        exit_with_error("Memory allocation error!");

    memset(frame_buffer, 0, FRAME_SIZE);

    for (int i = 0; i < total_frames; i++) {
        frame_buffer[i].header.length = fread(frame_buffer[i].payload, sizeof(uint8_t), PAYLOAD_SIZE, filep);
        if (ferror(filep) != 0)
            exit_with_error("File read error!");
    }

    fclose(filep);
    *num_of_frames = (size_t) total_frames;
    return frame_buffer;
}

void create_crc32_table() {
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
        uint8_t pos = (uint8_t) (crc >> 24) ^ buffer[i];
        crc = (crc << 8) ^ crc32_table[pos];
    }
    return crc;
}

void send_frame(const Frame *frame, int receiver_socket) {
    ssize_t total_bytes_sent = 0;
    while (total_bytes_sent < FRAME_SIZE) {
        ssize_t bytes_sent = send(receiver_socket, frame + total_bytes_sent, FRAME_SIZE - total_bytes_sent, 0);
        if (bytes_sent < 0)
            exit_with_error("Send Failed!");
        total_bytes_sent += bytes_sent;
    }
}

void send_ack(int ack_no, int receiver_socket) {
    AckFrame *frame = (AckFrame *) malloc(sizeof(AckFrame));
    if (frame == NULL) return;
    frame->frame_type = 0xFF;
    frame->ack_no = ack_no;

    ssize_t total_bytes_sent = 0;
    while (total_bytes_sent < sizeof(AckFrame)) {
        ssize_t bytes_sent = send(receiver_socket, frame + total_bytes_sent, FRAME_SIZE - total_bytes_sent, 0);
        if (bytes_sent < 0)
            exit_with_error("Send Failed!");
        total_bytes_sent += bytes_sent;
    }
}
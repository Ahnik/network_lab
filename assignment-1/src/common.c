#include "common.h"
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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

Frame *chunk_file(const char *filename, size_t *num_of_frames) {
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

    for (int i = 0; i <= total_frames; i++) {
        frame_buffer[i].header.length = fread(frame_buffer[i].payload, sizeof(uint8_t), PAYLOAD_SIZE, filep);
        if (ferror(filep) != 0)
            exit_with_error("File read error!");
    }

    fclose(filep);
    *num_of_frames = (size_t) total_frames;
    return frame_buffer;
}

uint16_t find_checksum(const uint16_t *buffer, size_t length) {
    uint16_t sum = 0;
    uint16_t carry = 0;
    uint16_t next_carry = 0;

    for (size_t i = 0; i < length; i++) {
        next_carry = 0;
        if (sum > UINT16_MAX - htons(buffer[i]) - carry)
            next_carry = 1;
        sum += htons(buffer[i]) + carry;
        carry = next_carry;
    }
    sum += carry;

    return ~sum;
}

/* TODO: Improve verify_checksum() function by using the find_checksum() function within it */
/* This function may not be needed. */
bool verify_checksum(const uint16_t *buffer, size_t length, uint16_t checksum) {
    uint16_t sum = 0;
    uint16_t carry = 0;
    uint16_t next_carry = 0;

    for (size_t i = 0; i < length; i++) {
        next_carry = 0;
        if (sum > UINT16_MAX - htons(buffer[i]) - carry)
            next_carry = 1;
        sum += htons(buffer[i]) + carry;
        carry = next_carry;
    }
    next_carry = 0;
    if (sum > UINT16_MAX - checksum - carry)
        next_carry = 1;
    sum += checksum + carry;
    sum += next_carry;

    return (sum == 0xFFFF);
}
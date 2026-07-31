#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include "common.h"
#include "crc.h"
#include "error_injector.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./main.out <filename>\n");
        return -1;
    }

    // Seed the random number generator
    srand((unsigned int) time(NULL));

    // Chunk the input file into frames
    size_t total_frames = 0;
    Frame *frame_buffer = chunk_file(argv[1], &total_frames);

    for (size_t i = 0; i < total_frames; i++) {
        ErrorDetectingCode code = rand() % NUM_OF_CODES;
        switch (code) {
            case CHECKSUM:
                frame_buffer[i].trailer.checksum = htons(find_checksum(frame_buffer[i].payload, PAYLOAD_SIZE/2));
                break;
            case CRC8:
                frame_buffer[i].trailer.crc[0] = compute_crc8(frame_buffer[i].payload, PAYLOAD_SIZE);
                break;
            case CRC10:
                uint16_t crc10 = compute_crc10(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc10 >> 2);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc10 << 6);
                break;
            case CRC16:
                uint16_t crc16 = compute_crc16(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc16 >> 8);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc16);
                break;
            case CRC32:
                uint32_t crc32 = compute_crc32(frame_buffer[i].payload, PAYLOAD_SIZE);
                frame_buffer[i].trailer.crc[0] = (uint8_t) (crc32 >> 24);
                frame_buffer[i].trailer.crc[1] = (uint8_t) (crc32 >> 16);
                frame_buffer[i].trailer.crc[2] = (uint8_t) (crc32 >> 8);
                frame_buffer[i].trailer.crc[3] = (uint8_t) (crc32);
                break;
        }
    }

    return 0;
}
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
                break;
            case CRC10:
                break;
            case CRC16:
                break;
            case CRC32:
                frame_buffer[i].trailer.crc32 = htonl(compute_crc32(frame_buffer[i].payload, PAYLOAD_SIZE));
                break;
        }
    }

    return 0;
}
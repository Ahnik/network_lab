#include <stdio.h>
#include "common.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./main.out <filename>\n");
        return -1;
    }

    // Chunk the input file into frames
    Frame *frame_buffer = chunk_file(argv[1]);

    return 0;
}
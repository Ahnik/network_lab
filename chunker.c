#include "chunker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Frame *chunk_file(const char *filename) {
    // Open the file in read mode as a binary file
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

    // Calculate the number of packets the file will get chunked into
    long total_frames = (file_size + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;

    // Allocate memory for the packets
    Frame *frame_buffer = (Frame *) calloc(total_frames, sizeof(Frame));
    if (frame_buffer == NULL)
        exit_with_error("Memory allocation error!");

    // Set all the bits in the buffer as zero
    memset(frame_buffer, 0, FRAME_SIZE);

    for (int i = 0; i <= total_frames; i++) {
        frame_buffer[i].header.length = fread(frame_buffer[i].payload, sizeof(uint8_t), PAYLOAD_SIZE, filep);
        if (ferror(filep) != 0)
            exit_with_error("File read error!");
    }

    // Close the file
    fclose(filep);
}
#ifndef CHUNKER_H
#define CHUNKER_H

#include "common.h"

// Function to open a file, read it, chunk it into packets and return the first packet
Packet *chunk_file(const char *filename);

#endif
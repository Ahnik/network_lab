#ifndef CHECKSUM_H
#define CHECKSUM_H

#include "common.h"

// Calculate the checksum of the payload and add it to trailer
void calc_checksum(Frame *frame);

#endif
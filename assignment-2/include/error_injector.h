#ifndef ERROR_INJECTOR_H
#define ERROR_INJECTOR_H

#include <stddef.h>
#include "common.h"

// Enumerate all types of errors supported
typedef enum {
    SINGLE_BIT = 0,
    TWO_ISOLATED,
    ODD_ERRORS,
    BURST,
    NO_ERROR,
    ERROR_NUM,
} ErrorType;

// Function to inject single-bit errors
void inject_single_bit_error(uint8_t *buffer, unsigned int size);

// Function to inject two isolated single-bit errors
void inject_two_isolated_error(uint8_t *buffer, unsigned int size);

// Function to inject odd errors
void inject_odd_errors(uint8_t *buffer, unsigned int size);

// Function to inject burst error
void inject_burst_error(uint8_t *buffer, unsigned int size);

// Function to inject error into a frame
void inject_error(uint8_t *frame, size_t length, double per_frame_error);

// Function to send an ACK frame with error injected within it according to probability
void send_control_frame_with_error(uint8_t frame_type, uint8_t seq_no, int sender_socket, double per_frame_error);

#endif
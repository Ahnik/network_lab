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

// Function to flip two 16-bit words
void flip_two_words(uint16_t *buffer, unsigned int size);

#endif
#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CRC_TABLE_SIZE (2 << 8)

// Generators for CRCs
#define CRC8_GENERATOR  0x07
#define CRC16_GENERATOR 0x1021
#define CRC32_GENERATOR 0x04C11DB7

// Lookup tables for calculating CRC
static uint8_t  crc8_table[CRC_TABLE_SIZE];
static uint16_t crc16_table[CRC_TABLE_SIZE];
static uint32_t crc32_table[CRC_TABLE_SIZE];

// Function to create the CRC-8 lookup table
void create_crc8_table();

// Function to create the CRC-16 lookup table
void create_crc16_table();

// Function to create the CRC-32 lookup table
void create_crc32_table();

// Function to compute CRC-8 of a buffer
uint8_t compute_crc8(const uint8_t *buffer, size_t size);

// Function to compute CRC-16 of a buffer
uint16_t compute_crc16(const uint8_t *buffer, size_t size);

#endif
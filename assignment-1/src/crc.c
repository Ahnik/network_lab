#include "crc.h"

void create_crc8_table() {
    for (uint16_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint8_t reg = (uint8_t) i;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x80)
                reg = (reg << 1) ^ CRC8_GENERATOR;
            else
                reg <<= 1;
        }
        crc8_table[i] = reg;
    }
}

void create_crc10_table() {
    for (uint16_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint16_t reg = i << 2;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x0200)
                reg = (reg << 1) ^ CRC10_GENERATOR;
            else
                reg <<= 1;
        }
        crc10_table[i] = reg & 0x03FF;
    }
}

void create_crc16_table() {
    for (uint16_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint16_t reg = i << 8;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x8000)
                reg = (reg << 1) ^ CRC16_GENERATOR;
            else
                reg <<= 1;
        }
        crc16_table[i] = reg;
    }
}

void create_crc32_table() {
    for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint32_t reg = i << 24;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x80000000)
                reg = (reg << 1) ^ CRC32_GENERATOR;
            else
                reg <<= 1;
        }
        crc32_table[i] = reg;
    }
}

uint8_t compute_crc8(const uint8_t *buffer, size_t size) {
    if (buffer == NULL) return 0;
    uint8_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t pos = crc ^ buffer[i];
        crc = crc8_table[pos];
    }
    return crc;
}

uint16_t compute_crc10(const uint8_t *buffer, size_t size) {
    if (buffer == NULL) return 0;
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t pos = (uint8_t) (crc >> 2) ^ buffer[i];
        crc = (crc << 8) ^ crc10_table[pos];
    }
    crc = crc & 0x03FF;
    return crc;
}

uint16_t compute_crc16(const uint8_t *buffer, size_t size) {
    if (buffer == NULL) return 0;
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t pos = (uint8_t) (crc >> 8) ^ buffer[i];
        crc = (crc << 8) ^ crc16_table[pos];
    }
    return crc;
}

uint32_t compute_crc32(const uint8_t *buffer, size_t size) {
    if (buffer == NULL) return 0;
    uint32_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t pos = (uint8_t) (crc >> 24) ^ buffer[i];
        crc = (crc << 8) ^ crc32_table[pos];
    }
    return crc;
}
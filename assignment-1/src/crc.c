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

uint8_t compute_crc8(const uint8_t *buffer, size_t size) {
    uint8_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t data = crc ^ buffer[i];
        crc = crc8_table[data];
    }
    return crc;
}

uint16_t compute_crc16(const uint8_t *buffer, size_t size) {
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        uint16_t byte = (uint16_t) buffer[i];
        uint16_t data = crc ^ (byte << 8);
        crc = crc16_table[data];
    }
    return crc;
}
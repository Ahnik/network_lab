#include "crc.h"

void create_crc8_table() {
    for (uint8_t i = 0; i < CRC_TABLE_SIZE; i++) {
        uint8_t reg = i;
        for (int j = 0; j < 8; j++) {
            if (reg & 0x80)
                reg = (reg << 1) ^ CRC8_GENERATOR;
            else
                reg <<= 1;
        }
        crc8_table[i] = reg;
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

bool verify_crc8(const uint8_t *buffer, size_t size, uint8_t crc) {
    uint8_t reg = 0;
    uint8_t data;
    for (size_t i = 0; i < size; i++) {
        data = reg ^ buffer[i];
        reg = crc8_table[data];
    }
    data = reg ^ crc;
    return crc8_table[data] == 0;
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
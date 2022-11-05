#pragma once

#include <cstdint>


/**
 * Calculate CRC-16/CCITT-FALSE, polynom 0x1021, see https://crccalc.com/
 * @param size size of data
 * @param data data
 * @param crc chained CRC, can be used to calculate crc in multiple sections
 * @return resulting CRC
 */
uint16_t crc16(int size, const void *data, uint16_t crc = 0xffff);

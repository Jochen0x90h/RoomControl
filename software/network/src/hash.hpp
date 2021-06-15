#pragma once

#include "DataBuffer.hpp"


/**
 * Cryptographic Hash Function
 * Reference: Sections B.1.3 and B.6. of https://zigbeealliance.org/wp-content/uploads/2019/12/docs-05-3474-21-0csg-zigbee-specification.pdf
 * Ported from wireshark/epan/dissectors/packet-zbee-security.c
 *
 * @param output output hash
 * @param input input data that should be hashed
 * @param input input_len length of input data
 */
void hash(DataBufferRef<16> output, uint8_t const *input, int input_len);

void keyHash(DataBufferRef<16> output, DataBufferConstRef<16> key, uint8_t input);

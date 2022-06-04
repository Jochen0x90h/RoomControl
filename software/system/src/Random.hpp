#pragma once

#include <cstdint>


/**
 * System random number generator, comparable to /dev/urandom.
 * The generator has an internal queue for random numbers that needs time to refill after numbers are used. If the get
 * methods are called too often in a row, the numbers are not random any more.
 */
namespace Random {

/**
 * Initialize system random number generator
 */
void init();

/**
 * Get an 8 bit random number
 */
uint8_t u8();

/**
 * Get an 16 bit random number
 */
uint16_t u16();

/**
 * Get an 32 bit random number
 */
uint32_t u32();

/**
 * Get a 64 bit random number
 */
uint64_t u64();

} // namespace Random

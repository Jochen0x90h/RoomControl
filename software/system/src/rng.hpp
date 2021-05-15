#pragma once

#include <cstdint>


/**
 * System random number generator, comparable to /dev/urandom.
 * The generator has an internal queue for random numbers that needs time to refill after numbers are used. If the get
 * methods are called too often in a row, the numbers are not random any more.
 */
namespace rng {

/**
 * Initialize system random number generator
 */
void init();

/**
 * Get an 8 bit random number
 */
uint8_t get8();

/**
 * Get a 64 bit random number
 */
uint64_t get64();

} // namespace rng

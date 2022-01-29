#pragma once


/**
 * Support for general purpose input/output pins
 */
namespace gpio {

void init();

/**
 * Set the digital output with the given index to the given value
 * @param index index of digital output
 * @param value value to set
 */
void set(int index, bool value = true);

/**
 * Clear the digital output with given index
 * @param index index of digital output
 */
inline void clear(int index) {set(index, false);}

/**
 * Toggle the digital output with given index
 * @param index index of digital output
 */
void toggle(int index);

} // namespace gpio

#pragma once

#include <Coroutine.hpp>


/**
 * Support for digital outputs
 * Note that an input and output can be mapped to the same pin in sysConfig.hpp
 */
namespace Output {

/**
 * Initialize the digital outputs
 */
void init();

/**
 * Enable an output
 * @param index index of output
 * @param enabled true to enable the output driver
 */
void enable(int index, bool enabled);

/**
 * Set an output
 * @param index index of output
 * @param value value to set
 */
void set(int index, bool value = true);

/**
 * Clear an output
 * @param index index of output
 */
inline void clear(int index) {set(index, false);}

/**
 * Toggle an output
 * @param index index of output
 */
void toggle(int index);


} // namespace Output

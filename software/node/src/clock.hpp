#pragma once

#include "ClockTime.hpp"
#include <functional>


namespace clock {

/**
 * Initialze the clock
 */
void init();

/**
 * Handle clock events
 */
void handle();

/**
 * Get current wall clock time
 * @return clock time
 */
ClockTime getTime();

/**
 * Set callback for second tick
 * onElapsed called when a second has elapsed
 */
void setSecondTick(std::function<void ()> onElapsed);

} // namespace system

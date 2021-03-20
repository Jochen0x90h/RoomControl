#pragma once

#include "ClockTime.hpp"
#include <functional>


namespace calendar {

/**
 * Initialze the clock
 */
void init();

/**
 * Handle clock events
 */
void handle();

/**
 * Get current wall clock time including weekday
 * @return clock time
 */
ClockTime getTime();

/**
 * Set callback that gets called once per second
 * @param onSecond called when a second has elapsed, must not be null
 * @return id of handler to be able to remove it later
 */
int8_t addSecondHandler(std::function<void ()> const &onSecond);

} // namespace calendar

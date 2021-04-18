#pragma once

#include "ClockTime.hpp"
#include <functional>


namespace calendar {

/**
 * Initialize the calendar and wall clock
 */
void init();

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
uint8_t addSecondHandler(std::function<void ()> const &onSecond);

} // namespace calendar

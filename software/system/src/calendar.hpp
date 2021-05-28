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
ClockTime now();

/**
 * Set callback that gets called once per second
 * @param index handler index (number of handlers defined by CALENDAR_SECOND_HANDLER_COUNT in sysConfig.hpp)
 * @param onSecond called when a second has elapsed, must not be null
 */
void setSecondHandler(int index, std::function<void ()> const &onSecond);

} // namespace calendar

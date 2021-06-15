#pragma once

#include "ClockTime.hpp"
#include <Coroutine.hpp>
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
 * Suspend execution using co_await until next second tick
 */
Awaitable<> secondTick();

} // namespace calendar

#pragma once

#include "SystemTime.hpp"
#include <Coroutine.hpp>
#include <functional>


namespace timer {

/**
 * Initialize the timer
 */
void init();

/**
 * Get current time in 1/1024 seconds
 * @return current time
 */
SystemTime now();

/**
 * Suspend execution using co_await until a given time. Only up to TIMER_COUNT coroutines can wait simultaneously.
 * @param time time point
 */
[[nodiscard]] Awaitable<SystemTime> sleep(SystemTime time);

/**
 * Suspend execution using co_await for a given duration. Only up to TIMER_COUNT coroutines can wait simultaneously.
 * @param duration duration
 */
[[nodiscard]] inline Awaitable<SystemTime> sleep(SystemDuration duration) {return sleep(now() + duration);}

} // namespace timer

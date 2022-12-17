#pragma once

#include "SystemTime.hpp"
#include <Coroutine.hpp>
#include <functional>


namespace Timer {

/**
 * Initialize the timer
 */
void init();

/**
 * Get current time in milliseconds
 * @return current time
 */
SystemTime now();

/**
 * Suspend execution using co_await until a given time.
 * @param time time point
 */
[[nodiscard]] Awaitable<SystemTime> sleep(SystemTime time);

/**
 * Suspend execution using co_await for a given duration.
 * @param duration duration
 */
[[nodiscard]] inline Awaitable<SystemTime> sleep(SystemDuration duration) {return sleep(now() + duration);}

} // namespace Timer

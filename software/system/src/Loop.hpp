#pragma once

#include "SystemTime.hpp"
#include <Coroutine.hpp>


namespace loop {

/**
 * Initialize the event loop
 */
void init();

/**
 * Run the event loop
 */
void run();

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

// busy waiting, only for debug purposes, not very precise
void sleepBlocking(int us);

} // namespace loop

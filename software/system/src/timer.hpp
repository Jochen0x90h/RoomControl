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
Awaitable<SystemTime> wait(SystemTime time);

/**
 * Suspend execution using co_await for a given duration. Only up to TIMER_COUNT coroutines can wait simultaneously.
 * @param duration duration
 */
inline Awaitable<SystemTime> delay(SystemDuration duration) {return wait(now() + duration);}


/**
 * Set timeout handler
 * @param index timer index (number of timer defined by TIMER_COUNT in sysConfig.hpp)
 * @param onTimeout called when the timeout time was reached
 */
void setHandler(int index, std::function<void ()> const &onTimeout);

/**
 * Start timer
 * @param index timer index (number of timer defined by TIMER_COUNT in sysConfig.hpp)
 * @param time timeout time
 */
void start(int index, SystemTime time);

inline void start(int index, SystemTime time, std::function<void ()> const &onTimeout) {
	setHandler(index, onTimeout);
	start(index, time);
}

/**
 * Stop timer
 * @param index timer index (number of timer defined by TIMER_COUNT in sysConfig.hpp)
 */ 
void stop(int index);

} // namespace timer

#pragma once

#include "SystemTime.hpp"
#include <functional>


namespace timer {

/**
 * Initialze the timer
 */
void init();

/**
 * Wait for event and handle timer events 
 */
void handle();

/**
 * Get current time in 1/1024 seconds
 */
SystemTime getTime();

/**
 * Set timeout handler
 * @param onTimeout called when the timeout time was reached
 */
void setHandler(int index, std::function<void ()> const &onTimeout);

/**
 * Start timer
 * @param index timer channel
 * @param time timeout time
 */
void start(int index, SystemTime time);

inline void start(int index, SystemTime time, std::function<void ()> const &onTimeout) {
	setHandler(index, onTimeout);
	start(index, time);
}

/**
 * Stop timer
 * @param index timer channel
 */ 
void stop(int index);
	
} // namespace timer

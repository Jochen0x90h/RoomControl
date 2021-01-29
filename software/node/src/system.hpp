#pragma once

#include "SystemTime.hpp"
#include <functional>


namespace system {

/**
 * Initialze the system
 */
void init();

/**
 * Wait for event and handle system events 
 */
void handle();

/**
 * Get current time in 1/1024 seconds
 */
SystemTime getTime();

/**
 * Set timer based on system time
 * @param index timer channel
 * @param time timeout time
 * @param onTimeout called when the timeout time was reached
 */
void setTimer(int index, SystemTime time, std::function<void ()> onTimeout);

/**
 * Stop timer
 * @param index timer channel
 */ 
void stopTimer(int index);
	
} // namespace system

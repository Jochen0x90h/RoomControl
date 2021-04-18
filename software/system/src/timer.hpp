#pragma once

#include "SystemTime.hpp"
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
SystemTime getTime();

/**
 * Allocate a timer
 * @return timer id
 */
uint8_t allocate();

/**
 * Set timeout handler
 * @param id timer id
 * @param onTimeout called when the timeout time was reached
 */
void setHandler(uint8_t id, std::function<void ()> const &onTimeout);

/**
 * Start timer
 * @param id timer id
 * @param time timeout time
 */
void start(uint8_t id, SystemTime time);

inline void start(uint8_t id, SystemTime time, std::function<void ()> const &onTimeout) {
	setHandler(id, onTimeout);
	start(id, time);
}

/**
 * Stop timer
 * @param id timer id
 */ 
void stop(uint8_t id);
	
} // namespace timer

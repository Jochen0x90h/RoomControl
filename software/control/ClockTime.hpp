#pragma once

#include "defines.hpp"


struct ClockTime {
	static constexpr uint32_t SECONDS_MASK = 0x0000003f; // secons
	static constexpr uint32_t MINUTES_MASK = 0x00003f00; // minutes
	static constexpr uint32_t HOURS_MASK   = 0x001f0000; // hours

	static constexpr int MINUTES_SHIFT = 8;
	static constexpr int HOURS_SHIFT   = 16;
	static constexpr int WEEKDAY_SHIFT = 24;
	
	uint32_t time;

	/**
	 * Get seconds
	 * @return seconds: 0 - 59
	 */
	int getSeconds() const {return this->time & SECONDS_MASK;}

	/**
	 * Get minutes
	 * @return minutes: 0 - 59
	 */
	int getMinutes() const {return (this->time & MINUTES_MASK) >> MINUTES_SHIFT;}

	/**
	 * Get hours
	 * @return hours: 0 - 23
	 */
	int getHours() const {return (this->time & HOURS_MASK) >> HOURS_SHIFT;}
	
	/**
	 * Get weekday: 0 (Monday) to 6 (Sunday)
	 */
	int getWeekday() const {return this->time >> WEEKDAY_SHIFT;}
	
	/**
	 * Get weekday flags 
	 */
	int getWeekdays() const {return this->time >> WEEKDAY_SHIFT;}

	//void addSeconds(int delta);
	//void addMinutes(int delta);
	//void addHours(int delta);
};

constexpr ClockTime makeClockTime(int weekday, int hours, int minutes, int seconds = 0) {
	return {uint32_t(seconds
		| minutes << ClockTime::MINUTES_SHIFT
		| hours << ClockTime::HOURS_SHIFT
		| weekday << ClockTime::WEEKDAY_SHIFT)};
}

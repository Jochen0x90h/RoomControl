#pragma once

#include <cstdint>


class ClockTime {
public:
	static constexpr uint32_t SECONDS_MASK = 0x0000003f;
	static constexpr uint32_t MINUTES_MASK = 0x00003f00;
	static constexpr uint32_t HOURS_MASK   = 0x001f0000;
	static constexpr uint32_t WEEKDAY_MASK = 0x07000000;

	static constexpr int MINUTES_SHIFT = 8;
	static constexpr int HOURS_SHIFT   = 16;
	static constexpr int WEEKDAY_SHIFT = 24;
	
	uint32_t time = 0;

	ClockTime() = default;

	explicit ClockTime(uint32_t time) : time(time) {}

	constexpr ClockTime(int weekday, int hours, int minutes, int seconds = 0)
		: time(seconds
			| minutes << ClockTime::MINUTES_SHIFT
			| hours << ClockTime::HOURS_SHIFT
			| weekday << ClockTime::WEEKDAY_SHIFT)
	{
	}

	/**
	 * Get seconds
	 * @return seconds, range 0 - 59
	 */
	int getSeconds() const {return this->time & SECONDS_MASK;}

	/**
	 * Set seconds
	 * @param seconds seconds, range 0 - 59
	 */
	void setSeconds(int seconds) {this->time = (this->time & ~SECONDS_MASK) | seconds;}

	/**
	 * Add to seconds with overflow handling
	 * @param delta delta to add to seconds
	 */
	void addSeconds(int delta) {
		setSeconds((getSeconds() + 6000 + delta) % 60);
	}

	/**
	 * Get minutes
	 * @return minutes, range 0 - 59
	 */
	int getMinutes() const {return (this->time & MINUTES_MASK) >> MINUTES_SHIFT;}

	/**
	 * Set minutes
	 * @param minutes minutes, range 0 - 59
	 */
	void setMinutes(int minutes) {this->time = (this->time & ~MINUTES_MASK) | (minutes << MINUTES_SHIFT);}

	/**
	 * Add to minutes with overflow handling
	 * @param delta delta to add to minutes
	 */
	void addMinutes(int delta) {
		setMinutes((getMinutes() + 6000 + delta) % 60);
	}

	/**
	 * Get hours
	 * @return hours, range 0 - 23
	 */
	int getHours() const {return (this->time & HOURS_MASK) >> HOURS_SHIFT;}

	/**
	 * Set minutes
	 * @param hours hours, range 0 - 23
	 */
	void setHours(int hours) {this->time = (this->time & ~HOURS_MASK) | (hours << HOURS_SHIFT);}

	/**
	 * Add to hours with overflow handling
	 * @param delta delta to add to hours
	 */
	void addHours(int delta) {
		setHours((getHours() + 2400 + delta) % 24);
	}

	/**
	 * Get weekday: 0 (Monday) to 6 (Sunday)
	 */
	int getWeekday() const {return this->time >> WEEKDAY_SHIFT;}
};


class AlarmTime : public ClockTime {
public:
	static constexpr uint32_t WEEKDAYS_MASK = 0x7f000000; // weekday flags
	static constexpr int WEEKDAYS_SHIFT = WEEKDAY_SHIFT;

	/**
	 * Constructor. All weekday flags are on by default
	 */
	AlarmTime() : ClockTime(0x7f << WEEKDAYS_SHIFT) {}

	constexpr AlarmTime(int weekdays, int hours, int minutes, int seconds = 0)
		: ClockTime(weekdays, hours, minutes, seconds) {
	}

	/**
	 * Get weekday flags, 0: Monday, 6: Sunday
	 */
	int getWeekdays() const {return this->time >> WEEKDAY_SHIFT;}


	/**
	 * Set weekday flags
	 * @param weekdays weekday flags, 0: Monday, 6: Sunday
	 */
	void setWeekdays(int weekdays) {this->time = (this->time & ~WEEKDAYS_MASK) | (weekdays << WEEKDAYS_SHIFT);}

	/**
	 * Check if the alarm time matches a clock time, i.e. if the alarm went off
	 * @param time clock time
	 * @return true when alarm should to off
	 */
	bool matches(ClockTime time) const {
		// check seconds, minutes and hours
		if (((this->time ^ time.time) & (SECONDS_MASK | MINUTES_MASK | HOURS_MASK)) != 0)
			return false;

		// check weekday
		return (this->time & 1 << (WEEKDAYS_SHIFT + time.getWeekday())) != 0;
	}
};

#pragma once

#include "global.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "ClockTime.hpp"



class Clock {
public:

	Clock();

	virtual ~Clock();

	/**
	 * Get time and weekday packed into one int, see mask and shift constants
	 */
	ClockTime getClockTime();

	/**
	 * Internal use only
	 */
	void setClockTimeout(boost::posix_time::ptime utc);

	/**
	 * Called when a second has elapsed
	 */
	virtual void onSecondElapsed() = 0;


	asio::deadline_timer emulatorClock;
};

#include "Clock.hpp"
#include <chrono>


Clock::Clock()
	: emulatorClock(global::context)
{
	auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
	setClockTimeout(utc);
}

Clock::~Clock() {}

ClockTime Clock::getClockTime() {
	auto time = boost::date_time::second_clock<boost::posix_time::ptime>::local_time();
	auto t = time.time_of_day();
	int seconds = t.seconds();
	int minutes = t.minutes();
	int hours = t.hours();
	auto d = time.date();
	int weekDay = (d.day_of_week() + 6) % 7;
	return makeClockTime(weekDay, hours, minutes, seconds);
}

void Clock::setClockTimeout(boost::posix_time::ptime utc) {
	this->emulatorClock.expires_at(utc);
	this->emulatorClock.async_wait([this] (boost::system::error_code error) {
		if (!error) {
			onSecondElapsed();
			
			// set next timeout in one second
			auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
			setClockTimeout(utc + boost::posix_time::seconds(1));
		}
	});
}

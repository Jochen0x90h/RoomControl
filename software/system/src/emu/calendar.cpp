#include "../calendar.hpp"
#include "loop.hpp"
#include <sysConfig.hpp>


namespace calendar {

// waiting coroutines
Waitlist<> waitlist;

asio::deadline_timer *timer;

void setTimeout(boost::posix_time::ptime utc) {
	calendar::timer->expires_at(utc);
	calendar::timer->async_wait([] (boost::system::error_code error) {
		if (!error) {
			// resume all waiting coroutines
			calendar::waitlist.resumeAll();

			// set next timeout in one second
			auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
			setTimeout(utc + boost::posix_time::seconds(1));
		}
	});
}


void init() {
	calendar::timer = new asio::deadline_timer(loop::context);
	auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
	setTimeout(utc);
}

void handle() {
}

ClockTime now() {
	auto time = boost::date_time::second_clock<boost::posix_time::ptime>::local_time();
	auto t = time.time_of_day();
	int seconds = t.seconds();
	int minutes = t.minutes();
	int hours = t.hours();
	auto d = time.date();
	int weekDay = (d.day_of_week() + 6) % 7;
	return ClockTime(weekDay, hours, minutes, seconds);
}

Awaitable<> secondTick() {
	return {calendar::waitlist};
}

} // namespace system

#include "../calendar.hpp"
#include "loop.hpp"
#include <config.hpp>


namespace calendar {

asio::deadline_timer *timer;
std::function<void ()> onSecond[CALENDAR_SECOND_HANDLER_COUNT];

void setTimeout(boost::posix_time::ptime utc) {
	calendar::timer->expires_at(utc);
	calendar::timer->async_wait([] (boost::system::error_code error) {
		if (!error) {
			//calendar::onSecondElapsed();
			// call second handlers
			for (auto const &h : calendar::onSecond) {
				if (h)
					h();
			}

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

ClockTime getTime() {
	auto time = boost::date_time::second_clock<boost::posix_time::ptime>::local_time();
	auto t = time.time_of_day();
	int seconds = t.seconds();
	int minutes = t.minutes();
	int hours = t.hours();
	auto d = time.date();
	int weekDay = (d.day_of_week() + 6) % 7;
	return ClockTime(weekDay, hours, minutes, seconds);
}

uint8_t addSecondHandler(std::function<void ()> const &onSecond) {
	// handler must not be null
	assert(onSecond);
	for (int i = 0; i < CALENDAR_SECOND_HANDLER_COUNT; ++i) {
		if (!calendar::onSecond[i]) {
			calendar::onSecond[i] = onSecond;
			return i + 1;
		}
	}
	
	// error: handler array is full
	assert(false);
	return 0;
}

} // namespace system

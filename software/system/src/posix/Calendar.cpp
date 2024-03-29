#include "../Calendar.hpp"
#include "Loop.hpp"
#include <boost/date_time.hpp>


namespace Calendar {

class Context : public Loop::Timeout {
public:
	void activate() override {
		// next activation in 1s
		this->time += 1s;

		// resume all waiting coroutines
		this->waitlist.resumeAll();
	}

	// waiting coroutines
	Waitlist<> waitlist;
};
Context context;

bool inited = false;


void init() {
	// check if already initialized
	if (Calendar::inited)
		return;
	Calendar::inited = true;

	Calendar::context.time = Loop::now() + 1s;
	Loop::timeouts.add(Calendar::context);
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
	// check if Timer::init() was called
	assert(Calendar::inited);

	return {Calendar::context.waitlist};
}

} // namespace Calendar

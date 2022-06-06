#include "../Calendar.hpp"
#include "Loop.hpp"
#include <boost/date_time.hpp>


namespace Calendar {

class Context : public Loop::Timeout {
public:
	void activate() override {
		this->time += 1s;

		// resume all waiting coroutines
		this->waitlist.resumeAll();
	}

	// waiting coroutines
	Waitlist<> waitlist;
};

bool inited = false;
Context context;


// waiting coroutines
//Waitlist<> waitlist;

/*
asio::deadline_timer *timer;

void setTimeout(boost::posix_time::ptime utc) {
	Calendar::timer->expires_at(utc);
	Calendar::timer->async_wait([] (boost::system::error_code error) {
		if (!error) {
			// resume all waiting coroutines
			Calendar::waitlist.resumeAll();

			// set next timeout in one second
			auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
			setTimeout(utc + boost::posix_time::seconds(1));
		}
	});
}
*/

void init() {
	//Calendar::timer = new asio::deadline_timer(Loop::context);
	//auto utc = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
	//setTimeout(utc);
	// check if already initialized
	if (Calendar::inited)
		return;
	Calendar::inited = true;

	Calendar::context.time = Loop::now();
	Loop::timeouts.add(Calendar::context);
}
/*
void handle() {
}
*/
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

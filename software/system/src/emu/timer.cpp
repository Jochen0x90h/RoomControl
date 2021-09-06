#include "../timer.hpp"
#include "loop.hpp"
#include <sysConfig.hpp>
#include <iostream>


namespace timer {

std::chrono::steady_clock::time_point startTime;

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;

asio::steady_timer *timer = nullptr;


constexpr int TIMER_WAITING_COUNT = 4;
struct TimerOld {
	asio::steady_timer *timer = nullptr;
	std::function<void ()> onTimeout;
};
TimerOld timersOld[TIMER_WAITING_COUNT];


std::chrono::steady_clock::time_point toChronoTime(SystemTime time) {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - timer::startTime);
	
	// number of system ticks since timer::init()
	auto ticks = us.count() * 128 / 125000;
	
	// difference of ticks for the given time
	int32_t d = time.value - uint32_t(ticks);

	// add difference to current time
	return now + std::chrono::microseconds((long long)d * 125000 / 128);
}

void handle() {
	auto timeout = timer::next;
	timer::next.value += 0x80000;

	// resume all coroutines that where timeout occurred
	timer::waitlist.resumeAll([timeout](SystemTime time) {
		if (time == timeout)
			return true;
		
		// check if this time is the next to elapse
		if (time < timer::next)
			timer::next = time;
		return false;
	});

	// next timeout
	timer::timer->expires_at(toChronoTime(timer::next));
	timer::timer->async_wait([] (boost::system::error_code error) {
		if (!error)
			handle();
	});
}

void init() {
	timer::startTime = std::chrono::steady_clock::now();
	timer::next.value = 0x80000;
	timer::timer = new asio::steady_timer(loop::context);
	timer::timer->expires_at(toChronoTime(timer::next));
	timer::timer->async_wait([] (boost::system::error_code error) {
		if (!error)
			handle();
	});

	for (TimerOld &timer : timersOld) {
		timer.timer = new asio::steady_timer(loop::context);
	}
}

SystemTime now() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - timer::startTime);
	return {uint32_t(us.count() * 128 / 125000)};
}

Awaitable<SystemTime> wait(SystemTime time) {
	// check if this time is the next to elapse
	if (time < timer::next) {
		timer::next = time;
		timer::timer->expires_at(toChronoTime(time));
		timer::timer->async_wait([] (boost::system::error_code error) {
			if (!error)
				handle();
		});
	}

	return {timer::waitlist, time};
}

void setHandler(int index, std::function<void ()> const &onTimeout) {
	assert(uint(index) < TIMER_WAITING_COUNT);
	TimerOld &timer = timer::timersOld[index];

	timer.onTimeout = onTimeout;
}

void start(int index, SystemTime time) {
	assert(uint(index) < TIMER_WAITING_COUNT);
	TimerOld &timer = timer::timersOld[index];

	timer.timer->expires_at(toChronoTime(time));
	timer.timer->async_wait([&timer] (boost::system::error_code error) {
		if (!error && timer.onTimeout) {
			timer.onTimeout();
		}
	});
}

void stop(int index) {
	assert(uint(index) < TIMER_WAITING_COUNT);
	TimerOld &t = timer::timersOld[index];

	t.timer->cancel();
}

} // namespace timer

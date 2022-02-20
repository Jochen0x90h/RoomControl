#include "../Timer.hpp"
#include "Loop.hpp"
#include <boardConfig.hpp>
#include <iostream>


namespace Timer {

asio::steady_timer *timer = nullptr;

std::chrono::steady_clock::time_point startTime;

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


std::chrono::steady_clock::time_point toChronoTime(SystemTime time) {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - Timer::startTime);
	
	// number of system ticks since timer::init()
	auto ticks = us.count() * 128 / 125000;
	
	// difference of ticks for the given time
	int32_t d = time.value - uint32_t(ticks);

	// add difference to current time
	return now + std::chrono::microseconds((long long)d * 125000 / 128);
}

void handle() {
	auto timeout = Timer::next;
	Timer::next.value += 0x80000;

	// resume all coroutines that where timeout occurred
	Timer::waitlist.resumeAll([timeout](SystemTime time) {
		if (time == timeout)
			return true;
		
		// check if this time is the next to elapse
		if (time < Timer::next)
			Timer::next = time;
		return false;
	});

	// next timeout
	Timer::timer->expires_at(toChronoTime(Timer::next));
	Timer::timer->async_wait([] (boost::system::error_code error) {
		if (!error)
			handle();
	});
}

void init() {
	// check if already initialized
	if (Timer::timer != nullptr)
		return;
	
	Timer::timer = new asio::steady_timer(Loop::context);
	Timer::startTime = std::chrono::steady_clock::now();
	Timer::next.value = 0x80000;
	Timer::timer->expires_at(toChronoTime(Timer::next));
	Timer::timer->async_wait([] (boost::system::error_code error) {
		if (!error)
			handle();
	});
}

SystemTime now() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - Timer::startTime);
	return {uint32_t(us.count() * 128 / 125000)};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < Timer::next) {
		Timer::next = time;
		Timer::timer->expires_at(toChronoTime(time));
		Timer::timer->async_wait([] (boost::system::error_code error) {
			if (!error)
				handle();
		});
	}

	return {Timer::waitlist, time};
}

} // namespace Timer

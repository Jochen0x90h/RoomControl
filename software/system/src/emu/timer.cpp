#include "../timer.hpp"
#include "loop.hpp"
#include <sysConfig.hpp>


namespace timer {

struct Timer {
	asio::steady_timer *timer = nullptr;
	std::function<void ()> onTimeout;
};

Timer timers[TIMER_COUNT];


std::chrono::steady_clock::time_point toChronoTime(SystemTime time) {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	SystemDuration d = time - SystemTime{uint32_t(us.count() * 128 / 125000)};
	return now + std::chrono::microseconds((long long)d.value * 125000 / 128);
}

void init() {
	for (Timer &timer : timers) {
		timer.timer = new asio::steady_timer(loop::context);
	}
}

SystemTime now() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	return {uint32_t(us.count() * 128 / 125000)};
}

void setHandler(int index, std::function<void ()> const &onTimeout) {
	assert(uint(index) < TIMER_COUNT);
	Timer &timer = timer::timers[index];

	timer.onTimeout = onTimeout;
}

void start(int index, SystemTime time) {
	assert(uint(index) < TIMER_COUNT);
	Timer &timer = timer::timers[index];

	timer.timer->expires_at(toChronoTime(time));
	timer.timer->async_wait([&timer] (boost::system::error_code error) {
		if (!error && timer.onTimeout) {
			timer.onTimeout();
		}
	});
}

void stop(int index) {
	assert(uint(index) < TIMER_COUNT);
	Timer &timer = timer::timers[index];

	timer.timer->cancel();
}

} // namespace timer

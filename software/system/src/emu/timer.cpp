#include "../timer.hpp"
#include "loop.hpp"
#include <config.hpp>


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
}

SystemTime getTime() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	return {uint32_t(us.count() * 128 / 125000)};
}

uint8_t allocate() {
	for (uint8_t i = 0; i < TIMER_COUNT; ++i) {
		Timer &timer = timer::timers[i];
		if (timer.timer == nullptr) {
			timer.timer = new asio::steady_timer(loop::context);
			return i + 1;
		}
	}
	
	// error: timer array is full
	assert(false);
	return 0;
}

void setHandler(uint8_t id, std::function<void ()> const &onTimeout) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.timer != nullptr);
	timer.onTimeout = onTimeout;
}

void start(uint8_t id, SystemTime time) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.timer != nullptr);
	timer.timer->expires_at(toChronoTime(time));
	timer.timer->async_wait([&timer] (boost::system::error_code error) {
		if (!error && timer.onTimeout) {
			timer.onTimeout();
		}
	});
}

void stop(uint8_t id) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.timer != nullptr);
	timer.timer->cancel();
}

} // namespace timer

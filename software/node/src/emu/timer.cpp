#include "../timer.hpp"
#include "global.hpp"
#include <config.hpp>


namespace timer {

asio::steady_timer *timers[TIMER_COUNT];
std::function<void ()> onTimeouts[4];

std::chrono::steady_clock::time_point toChronoTime(SystemTime time) {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	SystemDuration d = time - SystemTime{uint32_t(us.count() * 128 / 125000)};
	return now + std::chrono::microseconds((long long)d.value * 125000 / 128);
}

void init() {
	for (asio::steady_timer *&timer : timers) {
		timer = new asio::steady_timer(global::context);
	}
}

void handle() {
}

SystemTime getTime() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	return {uint32_t(us.count() * 128 / 125000)};
}

void setHandler(int index, std::function<void ()> onTimeout) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT)) {
		timer::onTimeouts[index] = onTimeout;
	}
}

void start(int index, SystemTime time) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT)) {
		timer::timers[index]->expires_at(toChronoTime(time));
		timer::timers[index]->async_wait([index] (boost::system::error_code error) {
			if (!error) {
				timer::onTimeouts[index]();
			}
		});
	}
}

void stop(int index) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT))
		timer::timers[index]->cancel();
}

} // namespace timer

#include "../Timer.hpp"
#include "Loop.hpp"
#include <time.h>


namespace Timer {

class Context : public Loop::Timeout {
public:
	void activate() override {
		auto time = this->time;
		this->time += SystemDuration::max();

		// resume all coroutines that where timeout occurred
		this->waitlist.resumeAll([this, time](SystemTime timeout) {
			if (timeout == time)
				return true;

			// check if this time is the next to elapse
			if (timeout < this->time)
				this->time = timeout;
			return false;
		});
	}

	// waiting coroutines
	Waitlist<SystemTime> waitlist;
};

bool inited = false;
Context context;

void init() {
	// check if already initialized
	if (Timer::inited)
		return;
	Timer::inited = true;

	Timer::context.time = now() + SystemDuration::max();
	Loop::addTimeout(Timer::context);
}

SystemTime now() {
	timespec time;
	int r = clock_gettime(CLOCK_MONOTONIC, &time);
	return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if Timer::init() was called
    assert(Timer::inited);

    // check if this time is the next to elapse
	if (time < Timer::context.time)
		Timer::context.time = time;

	return {Timer::context.waitlist, time};
}

} // namespace Timer

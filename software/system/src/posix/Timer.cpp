#include "../Timer.hpp"
#include "Loop.hpp"


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
	Loop::timeouts.add(Timer::context);
}

SystemTime now() {
	return Loop::now();
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

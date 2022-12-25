#include "Loop.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <WinSock2.h>
#undef interface
#undef INTERFACE
#undef IN
#undef OUT
#define poll WSAPoll
#else
#include <ctime>
#include <poll.h>
#endif


namespace loop {

// SocketHandler

SocketHandler::~SocketHandler() {
}
SocketHandlerList socketHandlers;


// TimeHandler

TimeHandler::~TimeHandler() {
}
TimeHandlerList timeHandlers;



class Timer : public TimeHandler {
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
Timer timer;


/**
 * Platform dependent function: Initialize timer
 * @param wait wait for an event or timeout. Set to false when using a rendering loop, e.g. when using GLFW
 */
static void initTimer() {
	// check if not yet initialized
	assert(!loop::timer.isInList());

	loop::timer.time = loop::now() + SystemDuration::max();
	loop::timeHandlers.add(loop::timer);
}

SystemTime now() {
#ifdef _WIN32
	return {timeGetTime()};
#else
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
#endif
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if initTimer() was called
	assert(loop::timer.isInList());

	// check if this time is the next to elapse
	if (time < loop::timer.time)
		loop::timer.time = time;

	return {loop::timer.waitlist, time};
}


/**
 * Platform dependent function: Handle events
 * @param wait wait for an event or timeout. Set to false when using a rendering loop, e.g. when using GLFW
 */
static void handleEvents(bool wait = true) {
	// activate timeouts
	SystemTime time;
	bool activated;
	do {
		time = loop::now();
		activated = false;
		auto it = loop::timeHandlers.begin();
		while (it != loop::timeHandlers.end()) {
			// increment iterator beforehand because a timer can remove() itself
			auto current = it;
			++it;

			// check if timer needs to be activated
			if (current->time <= time) {
				current->activate();
				activated = true;
			}
		}
	} while (activated);

	// get next timeout
	auto next = time + SystemDuration::max();
	for (auto &handler : loop::timeHandlers) {
		if (handler.time < next)
			next = handler.time;
	}

	// fill poll infos
	struct pollfd infos[16];
	int count = 0;
	for (auto &handler : loop::socketHandlers) {
		infos[count++] = {handler.socket, handler.events, 0};
	}
	assert(count <= array::count(infos));

	// poll
	auto timeout = (next - time).value;
	//Terminal::out << "timeout " << dec(timeout) << '\n';
	int r = poll(infos, count, (timeout > 0 && wait) ? timeout : 0);

	// activate file descriptors
	if (r > 0) {
		int i = 0;
		auto it = loop::socketHandlers.begin();
		while (it != loop::socketHandlers.end()) {
			// increment iterator beforehand because a socket handler can remove() itself
			auto current = it;
			++it;

			// check if file descriptor needs to be activated
			auto events = infos[i].revents;
			if (events != 0)
				current->activate(events);

			// "garbage collect" file descriptors that are not interested in events anymore, also after close() was called
			if (current->events == 0)
				current->remove();

			++i;
		}
	}
}

} // namespace loop

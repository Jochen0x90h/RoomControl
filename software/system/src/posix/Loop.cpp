#include "Loop.hpp"
#include "../Timer.hpp"
#include <util.hpp>
#include <assert.hpp>
#include <poll.h>


namespace Timer {

SystemTime now() {
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
}

}

namespace Loop {

Handler::~Handler() {
}

Timeout::~Timeout() {
}

bool inited = false;

HandlerList handlers;
TimeoutList timeouts;

void addHandler(Handler &handler) {
	Loop::handlers.add(handler);
}

void addTimeout(Timeout &timeout) {
	Loop::timeouts.add(timeout);
}

void init() {
	Loop::inited = true;
}

void run() {
	assert(Loop::inited);
	while (true) {
		// timeouts
		SystemTime now;
		bool activated;
		do {
			now = Timer::now();
			activated = false;
			for (auto &timeout: Loop::timeouts) {
				if (timeout.time <= now) {
					timeout.activate();
					activated = true;
				}
			}
		} while (activated);

		// get next timeout
		auto next = now + SystemDuration::max();
		for (auto &timeout : Loop::timeouts) {
			if (timeout.time < next)
				next = timeout.time;
		}

		// fill poll infos
		struct pollfd infos[16];
		int count = 0;
		for (auto &handler : Loop::handlers) {
			infos[count++] = {handler.fd, handler.events, 0};
		}

		// poll
		auto timeout = max((next - now).value, 0);
		//Terminal::out << "timeout " << dec(timeout) << '\n';
		int r = poll(infos, count, timeout);

		// check for events
		if (r > 0) {
			int i = 0;
			auto it = Loop::handlers.begin();
			while (it != Loop::handlers.end()) {
				auto events = infos[i].revents;
				if (events != 0)
					it->activate(events);

				auto last = it;
				++it;
				++i;

				if (last->events == 0)
					last->remove();
			}
		}
	}
}

} // namespace Loop

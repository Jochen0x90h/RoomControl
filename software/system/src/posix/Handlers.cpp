#include "Handlers.hpp"
#ifdef _WIN32
#define poll WSAPoll
#endif


namespace Loop {

SocketHandler::~SocketHandler() {
}
SocketHandlerList socketHandlers;

TimeHandler::~TimeHandler() {
}
TimeHandlerList timeHandlers;

void runOnce(bool wait) {
	// activate timeouts
	SystemTime time;
	bool activated;
	do {
		time = now();
		activated = false;
		auto it = Loop::timeHandlers.begin();
		while (it != Loop::timeHandlers.end()) {
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
	for (auto &handler : Loop::timeHandlers) {
		if (handler.time < next)
			next = handler.time;
	}

	// fill poll infos
	struct pollfd infos[16];
	int count = 0;
	for (auto &handler : Loop::socketHandlers) {
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
		auto it = Loop::socketHandlers.begin();
		while (it != Loop::socketHandlers.end()) {
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

} // namespace Loop

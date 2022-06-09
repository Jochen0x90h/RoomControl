#include "Loop.hpp"
#include <poll.h>


namespace Loop {

FileDescriptor::~FileDescriptor() {
}
FileDescriptorList fileDescriptors;

Timeout::~Timeout() {
}
TimeoutList timeouts;

void runOnce(bool wait) {
	// timeouts
	SystemTime time;
	bool activated;
	do {
		time = now();
		activated = false;
		for (auto &timeout: Loop::timeouts) {
			if (timeout.time <= time) {
				timeout.activate();
				activated = true;
			}
		}
	} while (activated);

	// get next timeout
	auto next = time + SystemDuration::max();
	for (auto &timeout : Loop::timeouts) {
		if (timeout.time < next)
			next = timeout.time;
	}

	// fill poll infos
	struct pollfd infos[16];
	int count = 0;
	for (auto &fileDescriptor : Loop::fileDescriptors) {
		infos[count++] = {fileDescriptor.fd, fileDescriptor.events, 0};
	}

	// poll
	auto timeout = (next - time).value;
	//Terminal::out << "timeout " << dec(timeout) << '\n';
	int r = poll(infos, count, (timeout > 0 && wait) ? timeout : 0);

	// check for fileDescriptors
	if (r > 0) {
		int i = 0;
		auto it = Loop::fileDescriptors.begin();
		while (it != Loop::fileDescriptors.end()) {
			auto fileDescriptors = infos[i].revents;
			if (fileDescriptors != 0)
				it->activate(fileDescriptors);

			auto last = it;
			++it;
			++i;

			if (last->events == 0)
				last->remove();
		}
	}
}

} // namespace Loop

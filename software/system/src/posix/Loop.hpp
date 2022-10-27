#pragma once

#include "../Loop.hpp"
#include "SystemTime.hpp"
#include <LinkedList.hpp>
#include <ctime>


namespace Loop {

// current time
inline SystemTime now() {
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
}


// list of file descriptors to observe readable/writable events (used in Network.cpp)
class FileDescriptor : public LinkedListNode {
public:
	virtual ~FileDescriptor();
	virtual void activate(uint16_t events) = 0;

	int fd = -1;
	short int events;
};
using FileDescriptorList = LinkedList<FileDescriptor>;
extern FileDescriptorList fileDescriptors;


// timeouts for Timer and Calendar
class Timeout : public LinkedListNode {
public:
	virtual ~Timeout();
	virtual void activate() = 0;

	SystemTime time;
};
using TimeoutList = LinkedList<Timeout>;
extern TimeoutList timeouts;


/**
 * Run the event loop only once
 * @param wait wait for an event or timeout. Set to false when using a rendering loop, e.g. using GLFW
 */
void runOnce(bool wait = true);

} // namespace Loop

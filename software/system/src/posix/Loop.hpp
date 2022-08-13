#pragma once

#include "../Loop.hpp"
#include "SystemTime.hpp"
#include <LinkedListNode.hpp>
#include <ctime>


namespace Loop {

// current time
inline SystemTime now() {
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
}

// list of file descriptors to observe readable/writable events (used in Network.cpp)
class FileDescriptor : public LinkedListNode<FileDescriptor> {
public:
	virtual ~FileDescriptor();
	virtual void activate(uint16_t events) = 0;

	int fd;
	short int events;
};
using FileDescriptorList = LinkedListNode<FileDescriptor>;
extern FileDescriptorList fileDescriptors;

// timeouts for Timer and Calendar
class Timeout : public LinkedListNode<Timeout> {
public:
	virtual ~Timeout();
	virtual void activate() = 0;

	SystemTime time;
};
using TimeoutList = LinkedListNode<Timeout>;
extern TimeoutList timeouts;

/**
 * Run the event loop only once
 */
void runOnce(bool wait = true);

} // namespace Loop

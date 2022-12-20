#pragma once

#include "../Loop.hpp"
#include "SystemTime.hpp"
#include <LinkedList.hpp>
#include <ctime>
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#undef interface
#undef INTERFACE
#undef IN
#undef OUT
#else
#include <poll.h>
using SOCKET = int;
#endif


namespace Loop {

// current time
inline SystemTime now() {
#ifdef _WIN32
	return {timeGetTime()};
#else
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return {uint32_t(time.tv_sec * 1000 + time.tv_nsec / 1000000)};
#endif
}


// list of file descriptors to observe readable/writable events (used in Network.cpp)
class SocketHandler : public LinkedListNode {
public:
	virtual ~SocketHandler();
	virtual void activate(uint16_t events) = 0;

	SOCKET socket = -1;
	short int events;
};
using SocketHandlerList = LinkedList<SocketHandler>;
extern SocketHandlerList socketHandlers;


// timeouts for Timer and Calendar
class TimeHandler : public LinkedListNode {
public:
	virtual ~TimeHandler();
	virtual void activate() = 0;

	SystemTime time;
};
using TimeHandlerList = LinkedList<TimeHandler>;
extern TimeHandlerList timeHandlers;


/**
 * Run the event loop only once
 * @param wait wait for an event or timeout. Set to false when using a rendering loop, e.g. using GLFW
 */
void runOnce(bool wait = true);

} // namespace Loop

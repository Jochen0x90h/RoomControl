#pragma once

#include "../Loop.hpp"
#include "SystemTime.hpp"
#include <LinkedListNode.hpp>
#include <cstdint>


namespace Loop {

class Handler : public LinkedListNode<Handler> {
public:
	virtual ~Handler();
	virtual void activate(uint16_t events) = 0;

	int fd;
	short int events;
};
using HandlerList = LinkedListNode<Handler>;


class Timeout : public LinkedListNode<Timeout> {
public:
	virtual ~Timeout();
	virtual void activate() = 0;

	SystemTime time;
};
using TimeoutList = LinkedListNode<Timeout>;


/**
 * Add a handler to the event loop
 */
void addHandler(Handler &handler);

/**
 * Add a timeout to the event loop
 */
void addTimeout(Timeout &timeout);

} // namespace Loop

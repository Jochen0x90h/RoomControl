#pragma once

#include "../Loop.hpp"
#include <LinkedList.hpp>


namespace loop {

using Handler = void (*)(); 

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);



/**
 * Event handler that handles activity of the peripherals
 */
class Handler2 : public LinkedListNode {
public:
	virtual void handle() = 0;
};
using HandlerList = LinkedList<Handler2>;
extern HandlerList handlers;

} // namespace loop

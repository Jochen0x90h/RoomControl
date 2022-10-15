#pragma once

#include "../Loop.hpp"
#include <LinkedList.hpp>


namespace Loop {

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


/**
 * Set a handler to the event loop
 * @param handler handler to add
 * @return previous handler, needs to be called next
 */
//Handler2 *setHandler(Handler2 *handler);


} // namespace Loop

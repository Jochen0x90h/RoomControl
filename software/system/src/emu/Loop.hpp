#pragma once

#include "../Loop.hpp"
#include "Gui.hpp"
#include <LinkedList.hpp>


namespace Loop {

using Handler = void (*)(Gui &);

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);

/*
class Handler2 {
public:
	virtual void handle(Gui &);
};

/ **
 * Set a handler to the event loop
 * @param handler handler to add
 * @return previous handler, needs to be called next
 * /
Handler2 *setHandler(Handler2 *handler);
*/

/**
 * Event handler that handles activity of the emulated peripherals on the gui
 */
class Handler2 : public LinkedListNode {
public:
	virtual void handle(Gui &gui) = 0;
};
using HandlerList = LinkedList<Handler2>;
extern HandlerList handlers;

} // namespace Loop

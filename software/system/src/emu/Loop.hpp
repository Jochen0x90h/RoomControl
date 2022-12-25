#pragma once

#include "../Loop.hpp"
#include "Gui.hpp"
#include <LinkedList.hpp>


namespace loop {

using Handler = void (*)(Gui &);

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);


/**
 * Event handler that handles activity of the emulated peripherals on the gui
 */
class Handler2 : public LinkedListNode {
public:
	virtual void handle(Gui &gui) = 0;
};
using HandlerList = LinkedList<Handler2>;
extern HandlerList handlers;

} // namespace loop

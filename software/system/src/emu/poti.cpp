#include "../poti.hpp"
#include "loop.hpp"

namespace poti {

// waiting coroutines
Waitlist<Parameters> waitlist;


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	// call next handler in chain
	poti::nextHandler(gui);
	
	// draw poti on gui using random id
	auto poti = gui.poti(0xadead869);

	if (poti) {
		// resume all waiting coroutines
		poti::waitlist.resumeAll([&poti](Parameters p) {
			p.delta = poti->delta;
			p.activated = poti->activated;
			return true;
		});
	}
}

void init() {
	// add to event loop handler chain
	poti::nextHandler = loop::addHandler(handle);
}

Awaitable<Parameters> change(int& delta, bool& activated) {
	return {poti::waitlist, delta, activated};
}

} // namespace poti

#include "../poti.hpp"
#include "loop.hpp"
#include "input.hpp"
#include <appConfig.hpp>


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

	// check if the user changed something
	if (poti.delta) {
		int delta = *poti.delta;
		
		// resume all waiting coroutines
		poti::waitlist.resumeAll([delta](Parameters p) {
			p.delta = delta;
			return true;
		});
	}

	if (poti.button)
		input::set(INPUT_POTI_BUTTON, *poti.button);
}

void init() {
	// add to event loop handler chain
	poti::nextHandler = loop::addHandler(handle);
}

[[nodiscard]] Awaitable<Parameters> change(int index, int8_t& delta) {
	// currently only one poti supported
	assert(poti::nextHandler != nullptr && index == 0);
	return {poti::waitlist, delta};
}

} // namespace poti

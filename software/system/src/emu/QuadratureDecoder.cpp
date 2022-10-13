#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"
#include "Input.hpp"
#include <boardConfig.hpp>


namespace QuadratureDecoder {

// waiting coroutines
Waitlist<Parameters> waitlist;


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// call next handler in chain
	QuadratureDecoder::nextHandler(gui);
	
	// draw poti on gui using random id
	auto poti = gui.poti(0xadead869);

	// check if the user changed something
	if (poti.delta) {
		int delta = *poti.delta;
		
		// resume all waiting coroutines
		QuadratureDecoder::waitlist.resumeAll([delta](Parameters p) {
			p.delta = delta;
			return true;
		});
	}

#ifdef INPUT_EMU_POTI_BUTTON
	if (poti.button)
		Input::set(INPUT_EMU_POTI_BUTTON, *poti.button);
#endif
}

void init() {
	// add to event loop handler chain
	QuadratureDecoder::nextHandler = Loop::addHandler(handle);
}

[[nodiscard]] Awaitable<Parameters> change(int index, int8_t& delta) {
	// currently only one poti supported
	assert(QuadratureDecoder::nextHandler != nullptr && index == 0);
	return {QuadratureDecoder::waitlist, delta};
}

} // namespace QuadratureDecoder

#include "QuadratureDecoderImpl.hpp"
#include "Input.hpp"
#include <boardConfig.hpp>


QuadratureDecoderImpl::QuadratureDecoderImpl() {
	// add to list of handlers
	loop::handlers.add(*this);
}

Awaitable<QuadratureDecoder::Parameters> QuadratureDecoderImpl::change(int8_t& delta) {
	return {this->waitlist, delta};
}

void QuadratureDecoderImpl::handle(Gui &gui) {
	// draw poti on gui using random id
	auto poti = gui.poti(0xadead869);

	// check if the user changed something
	if (poti.delta) {
		int delta = *poti.delta;

		// resume all waiting coroutines
		this->waitlist.resumeAll([delta](Parameters p) {
			p.delta = delta;
			return true;
		});
	}

#ifdef INPUT_EMU_POTI_BUTTON
	if (poti.button)
		Input::set(INPUT_EMU_POTI_BUTTON, *poti.button);
#endif
}

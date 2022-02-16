#include "../poti.hpp"
#include "loop.hpp"
#include "timer.hpp"
#include "defs.hpp"
#include <assert.hpp>
#include <boardConfig.hpp>

/*
	Dependencies:
	
	Resources:
		NRF_QDEC
*/
namespace poti {

// waiting coroutines
Waitlist<Parameters> waitlist;

int acc = 0;

// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_QDEC->EVENTS_REPORTRDY) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_QDEC->EVENTS_REPORTRDY = 0;
		clearInterrupt(QDEC_IRQn);

		poti::acc += NRF_QDEC->ACCREAD;
		int poti = poti::acc & ~3;
		if (poti != 0) {
			poti::acc -= poti;
			int delta = poti >> 2;

			// resume all waiting coroutines
			poti::waitlist.resumeAll([delta](Parameters parameters) {
				parameters.delta = delta;
				return true;
			});
		}
	}

	// call next handler in chain
	poti::nextHandler();
}

void init() {
	// check if already initialized
	if (poti::nextHandler != nullptr)
		return;
	
	// add to event loop handler chain
	poti::nextHandler = loop::addHandler(handle);

	configureInput(POTI_A_PIN, Pull::UP);
	configureInput(POTI_B_PIN, Pull::UP);

	// quadrature decoder https://infocenter.nordicsemi.com/topic/ps_nrf52840/qdec.html?cp=4_0_0_5_17
	NRF_QDEC->SHORTS = N(QDEC_SHORTS_REPORTRDY_RDCLRACC, Enabled); // clear accumulator register on report
	NRF_QDEC->INTENSET = N(QDEC_INTENSET_REPORTRDY, Set);
	NRF_QDEC->SAMPLEPER = N(QDEC_SAMPLEPER_SAMPLEPER, 1024us);
	NRF_QDEC->REPORTPER = N(QDEC_REPORTPER_REPORTPER, 10Smpl);
	NRF_QDEC->PSEL.A = POTI_A_PIN;
	NRF_QDEC->PSEL.B = POTI_B_PIN;
	NRF_QDEC->DBFEN = N(QDEC_DBFEN_DBFEN, Enabled); // enable debounce filter
	NRF_QDEC->ENABLE = N(QDEC_ENABLE_ENABLE, Enabled);
	NRF_QDEC->TASKS_START = TRIGGER;
}

Awaitable<Parameters> change(int index, int8_t& delta) {
	// only one poti supported
	return {poti::waitlist, delta};
}

} // namespace poti

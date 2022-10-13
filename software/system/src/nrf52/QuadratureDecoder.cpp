#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include <assert.hpp>
#include <boardConfig.hpp>

/*
	Dependencies:
	
	Config:
	
	Resources:
		NRF_QDEC
*/
namespace QuadratureDecoder {

// waiting coroutines
Waitlist<Parameters> waitlist;

int acc = 0;

// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_QDEC->EVENTS_REPORTRDY) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_QDEC->EVENTS_REPORTRDY = 0;
		clearInterrupt(QDEC_IRQn);

		QuadratureDecoder::acc += NRF_QDEC->ACCREAD;
		int poti = QuadratureDecoder::acc & ~3;
		if (poti != 0) {
			QuadratureDecoder::acc -= poti;
			int delta = poti >> 2;

			// resume all waiting coroutines
			QuadratureDecoder::waitlist.resumeAll([delta](Parameters parameters) {
				parameters.delta = delta;
				return true;
			});
		}
	}

	// call next handler in chain
	QuadratureDecoder::nextHandler();
}

void init() {
	// check if already initialized
	if (QuadratureDecoder::nextHandler != nullptr)
		return;
	
	// add to event loop handler chain
	QuadratureDecoder::nextHandler = Loop::addHandler(handle);

	configureInput(POTI_A_PIN, gpio::Pull::UP);
	configureInput(POTI_B_PIN, gpio::Pull::UP);

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
	return {QuadratureDecoder::waitlist, delta};
}

} // namespace QuadratureDecoder

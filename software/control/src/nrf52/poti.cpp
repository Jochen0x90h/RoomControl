#include "../poti.hpp"
#include <timer.hpp>
#include <nrf52/global.hpp>


namespace poti {

std::function<void (int, bool)> onChanged = [](int, bool) {};
int acc = 0;
bool buttonState = false;
uint32_t counter;

void init() {
	configureInputWithPullUp(POTI_A_PIN);
	configureInputWithPullUp(POTI_B_PIN);
	configureInputWithPullUp(BUTTON_PIN);

	// quadrature decoder https://infocenter.nordicsemi.com/topic/ps_nrf52840/qdec.html?cp=4_0_0_5_17
	NRF_QDEC->SHORTS = N(QDEC_SHORTS_REPORTRDY_RDCLRACC, Enabled); // clear accumulator register on report
	NRF_QDEC->INTENSET = N(QDEC_INTENSET_REPORTRDY, Set);
	NRF_QDEC->SAMPLEPER = N(QDEC_SAMPLEPER_SAMPLEPER, 1024us);
	NRF_QDEC->REPORTPER = N(QDEC_REPORTPER_REPORTPER, 10Smpl);
	NRF_QDEC->PSEL.A = POTI_A_PIN;
	NRF_QDEC->PSEL.B = POTI_B_PIN;
	NRF_QDEC->DBFEN = N(QDEC_DBFEN_DBFEN, Enabled); // enable debounce filter

	// gpio event
	NRF_GPIOTE->INTENSET = N(GPIOTE_INTENSET_IN0, Enabled) | N(GPIOTE_INTENSET_IN1, Enabled);
}

void handle() {
	if (NRF_QDEC->EVENTS_REPORTRDY) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_QDEC->EVENTS_REPORTRDY = 0;
		clearInterrupt(QDEC_IRQn);

		poti::acc += NRF_QDEC->ACCREAD;
		int poti = poti::acc & ~3;
		if (poti != 0) {
			poti::acc -= poti;
			poti::onChanged(poti >> 2, false);
		}
	}
	if (NRF_GPIOTE->EVENTS_IN[0]) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_GPIOTE->EVENTS_IN[0] = 0;
		clearInterrupt(GPIOTE_IRQn);
		
		// debounce filter
		bool b = !getInput(BUTTON_PIN);
		uint32_t c = timer::getTime().value;		
		bool activated = !poti::buttonState;
		if (activated) {
			if (c - poti::counter > 100)
				poti::onChanged(0, true);		
		}
		poti::buttonState = b;
		poti::counter = c;
	}
}

void setHandler(std::function<void (int, bool)> onChanged) {
	poti::onChanged = onChanged;

	// quadrature decoder
	NRF_QDEC->ENABLE = N(QDEC_ENABLE_ENABLE, Enabled);
	NRF_QDEC->TASKS_START = Trigger;

	// button
	NRF_GPIOTE->CONFIG[0] = N(GPIOTE_CONFIG_MODE, Event) | N(GPIOTE_CONFIG_POLARITY, Toggle)
		| (BUTTON_PIN << GPIOTE_CONFIG_PSEL_Pos);
}

} // namespace poti

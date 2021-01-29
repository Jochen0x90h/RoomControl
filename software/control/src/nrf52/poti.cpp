#include "poti.hpp"
#include <nrf52/util.hpp>


namespace poti {

std::function<void (int)> onPotiChanged;
std::function<void ()> onButtonActivated;

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
}

void handle() {
	if (NRF_QDEC->EVENTS_REPORTRDY) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_QDEC->EVENTS_REPORTRDY = 0;
		clearInterrupt(QDEC_IRQn);

		onPotiChanged(NRF_QDEC->ACCREAD);
	}
}

void setPotiHandler(std::function<void (int)> onChanged) {
	onPotiChanged = onChanged;
	NRF_QDEC->ENABLE = N(QDEC_ENABLE_ENABLE, Enabled); // enable quadrature decoder
	NRF_QDEC->TASKS_START = Trigger;
}

void setButtonHandler(std::function<void ()> onActivated) {
	
}

} // namespace poti

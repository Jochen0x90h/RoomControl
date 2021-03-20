#include "../radio.hpp"
#include "global.hpp"
#include <assert.hpp>
//#include <debug.hpp>


namespace radio {

std::function<void ()> onSent;
std::function<void (uint8_t const *)> onReceived[RADIO_RECEIVE_HANDLER_COUNT];

uint8_t receiveBuffer[1 + RADIO_MAX_PAYLOAD_LENGTH] __attribute__((aligned(4)));


void init() {
	// set modulation mode
	NRF_RADIO->MODE = N(RADIO_MODE_MODE, Ieee802154_250Kbit);
	
	// configure crc
	NRF_RADIO->CRCCNF =
		N(RADIO_CRCCNF_SKIPADDR, Ieee802154) // CRC starts after first byte of length field
		| N(RADIO_CRCCNF_LEN, Two); // CRC-16
	NRF_RADIO->CRCPOLY = 0x11021;
	NRF_RADIO->CRCINIT = 0;

	// configure packet (PREAMBLE, ADDRESS, S0, LENGTH, S1, PAYLOAD, CRC)
	NRF_RADIO->PCNF0 =
		N(RADIO_PCNF0_PLEN, 32bitZero) // PREAMBLE is 32 bit zero
		| V(RADIO_PCNF0_S0LEN, 0) // no S0
		| N(RADIO_PCNF0_CRCINC, Include) // LENGTH includes CRC
		| V(RADIO_PCNF0_LFLEN, 8) // LENGTH is 8 bit
		| V(RADIO_PCNF0_S1LEN, 0); // no S1
	NRF_RADIO->PCNF1 =
		V(RADIO_PCNF1_MAXLEN, RADIO_MAX_PAYLOAD_LENGTH) // maximum length of PAYLOAD
		| N(RADIO_PCNF1_ENDIAN, Little); // little endian

	// set interrupt flags
	NRF_RADIO->INTENSET = N(RADIO_INTENSET_RXREADY, Enabled)
		| N(RADIO_INTENSET_TXREADY, Enabled)
		| N(RADIO_INTENSET_END, Enabled);

// todo
NRF_RADIO->FREQUENCY = V(RADIO_FREQUENCY_FREQUENCY, 2405 + 5 * (15 - 11) - 2400);

	// enable receiver
	NRF_RADIO->TASKS_RXEN = Trigger; // -> RXREADY
}

void handle() {
	if (!isInterruptPending(RADIO_IRQn))
		return;

	if (NRF_RADIO->EVENTS_RXREADY) {
		// clear pending interrupt flag at peripheral
		NRF_RADIO->EVENTS_RXREADY = 0;
		
		// set receive buffer
		NRF_RADIO->PACKETPTR = intptr_t(radio::receiveBuffer);
		
		// start receive
		NRF_RADIO->TASKS_START = Trigger; // -> END
		
	}
	if (NRF_RADIO->EVENTS_END) {
		// clear pending interrupt flag at peripheral
		NRF_RADIO->EVENTS_END = 0;
	
		// call receive handlers
		for (auto const &h : radio::onReceived) {
			if (h)
				h(receiveBuffer);
		}
		
		// start receive
		NRF_RADIO->TASKS_START = Trigger;
	}
	
	// clear pending interrupt flag at NVIC
	clearInterrupt(RADIO_IRQn);
}

void setChannel(int channel) {
	NRF_RADIO->FREQUENCY = V(RADIO_FREQUENCY_FREQUENCY, 2405 + 5 * (channel - 11) - 2400);
}

bool send(uint8_t const* data, std::function<void ()> const &onSent) {
	radio::onSent = onSent;

	return true;
}

int8_t addReceiveHandler(std::function<void (uint8_t const *)> const &onReceived) {
	assert(onReceived);
	for (int i = 0; i < RADIO_RECEIVE_HANDLER_COUNT; ++i) {
		if (!radio::onReceived[i]) {
			radio::onReceived[i] = onReceived;
			return i;
		}
	}

	// error: handler array is full
	assert(false);
	return -1;
}

} // namespace radio

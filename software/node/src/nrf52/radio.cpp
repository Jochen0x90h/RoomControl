#include "radio.hpp"
#include "util.hpp"


namespace radio {

void init() {
	// set modulation mode
	NRF_RADIO->MODE = N(RADIO_MODE_MODE, Ieee802154_250Kbit);
	
	// configure crc
	NRF_RADIO->CRCCNF =
		N(RADIO_CRCCNF_SKIPADDR, Ieee802154) // CRC starts after first byte of length field
		| N(RADIO_CRCCNF_LEN, Two); // CRC-16
	NRF_RADIO->CRCPOLY = 0x11021;
	NRF_RADIO->CRCINIT = 0;

	// configure packet
	NRF_RADIO->PCNF0 =
		N(RADIO_PCNF0_PLEN, 32bitZero) // preamble is 32 bit zero
		| N(RADIO_PCNF0_CRCINC, Include) // length includes CRC
		| V(RADIO_PCNF0_LFLEN, 8); // length is 8 bit
	NRF_RADIO->PCNF1 = N(RADIO_PCNF1_ENDIAN, Little); // little endian
}

void handle() {

}

void setRadioChannel(int channel) {
	NRF_RADIO->FREQUENCY = V(RADIO_FREQUENCY_FREQUENCY, 2405 + 5 * (channel - 11) - 2400);
}

void sendRadio(uint8_t const* data, int length, std::function<void ()> onSent) {
}

void receiveRadio(uint8_t* data, int length, std::function<void ()> onReceived) {
}

} // namespace radio

#include "../display.hpp"
#include "spi.hpp" // internal interface
#include "global.hpp"


namespace display {

void init() {
	setOutput(DISPLAY_CS_PIN, true);
	configureOutput(DISPLAY_CS_PIN);	
}

bool send(uint8_t const* data, int commandLength, int dataLength, std::function<void ()> const &onSent) {
	// check if transfer queue is full
	if (spi::transferQueue.full())
		return false;
	
	spi::Transfer &transfer = spi::transferQueue.add();
	transfer.csPin = DISPLAY_CS_PIN;
	transfer.writeData = intptr_t(data);
	transfer.writeLength = commandLength + dataLength;
	transfer.readData = 1; // used to detect display tasks
	transfer.readLength = commandLength; // use readDataLength to store length of command part
	transfer.onTransferred = onSent;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		spi::startTransfer();

	return true;
}

} // namespace spi

#include "../display.hpp"
#include <nrf52/spi.hpp> // internal interface
#include <nrf52/global.hpp>


namespace display {

void init() {
	setOutput(DISPLAY_CS_PIN, true);
	configureOutput(DISPLAY_CS_PIN);	
}

bool send(uint8_t const* data, int commandLength, int dataLength, std::function<void ()> const &onSent) {
	// check if task list is full
	if (spi::taskCount >= SPI_TASK_COUNT)
		return false;
	
	spi::Task &task = spi::allocateTask();
	task.csPin = DISPLAY_CS_PIN;
	task.writeData = intptr_t(data);
	task.writeLength = commandLength + dataLength;
	task.readData = 1; // used to detect display tasks
	task.readLength = commandLength; // use readDataLength to store length of command part
	task.onTransferred = onSent;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		spi::transfer();

	return true;
}

} // namespace spi

#include "../spi.hpp"
#include "spi.hpp"
#include <nrf52/global.hpp>


namespace spi {

Task tasks[SPI_TASK_COUNT];
int taskHead = 0;
int taskTail = 0;
int taskCount = 0;

Task &allocateTask() {
	Task &task = spi::tasks[spi::taskHead];
	spi::taskHead = (spi::taskHead == SPI_TASK_COUNT - 1) ? 0 : spi::taskHead + 1;
	++spi::taskCount;
	return task;
}

void transfer() {
	Task const &task = spi::tasks[spi::taskTail];
	
	// set CS pin
	NRF_SPIM3->PSEL.CSN = task.csPin;

	// set write data
	NRF_SPIM3->TXD.PTR = task.writeData;
	NRF_SPIM3->TXD.MAXCNT = task.writeLength;
	
	// check if this is a write-only task for display
	if (task.readData == 1) {
		// configure MISO pin as D/C# pin and set D/C# count
		NRF_SPIM3->PSEL.MISO = Disconnected;
		NRF_SPIM3->PSELDCX = SPI_MISO_PIN;
		//configureOutput(SPI_MISO_PIN); // done automatically
		NRF_SPIM3->DCXCNT = task.readLength;
	
		// no read data
		NRF_SPIM3->RXD.PTR = 0;
		NRF_SPIM3->RXD.MAXCNT = 0;
	} else {
		// set read data
		NRF_SPIM3->RXD.PTR = task.readData;
		NRF_SPIM3->RXD.MAXCNT = task.readLength;
	}
		
	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = Trigger;
}


void init() {
	configureOutput(SPI_SCK_PIN);
	setOutput(SPI_MOSI_PIN, true);
	configureOutput(SPI_MOSI_PIN);
	setOutput(SPI_MISO_PIN, true);
	configureInputWithPullUp(SPI_MISO_PIN);
	
	// set cs pins to outputs and high to disable devices
	for (int csPin : SPI_CS_PINS) {
		setOutput(csPin, true);
		configureOutput(csPin);
	}

	NRF_SPIM3->INTENSET = N(SPIM_INTENSET_END, Set);
	NRF_SPIM3->PSEL.SCK = SPI_SCK_PIN;
	NRF_SPIM3->PSEL.MOSI = SPI_MOSI_PIN;
	NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
	NRF_SPIM3->FREQUENCY = N(SPIM_FREQUENCY_FREQUENCY, M1);
	NRF_SPIM3->CONFIG = N(SPIM_CONFIG_CPOL, ActiveHigh)
		| N(SPIM_CONFIG_CPHA, Leading)
		| N(SPIM_CONFIG_ORDER, MsbFirst);
}

void handle() {
	if (NRF_SPIM3->EVENTS_END) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_SPIM3->EVENTS_END = 0;
		clearInterrupt(SPIM3_IRQn);			

		// disable SPI
		NRF_SPIM3->ENABLE = 0;

		// get current task
		Task const &task = spi::tasks[spi::taskTail];
		if (task.readData == 1) {
			// restore MISO pin
			NRF_SPIM3->PSELDCX = Disconnected;
			NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
			//configureInputWithPullUp(SPI_MISO_PIN); // done automatically
		}
		spi::taskTail = (spi::taskTail == SPI_TASK_COUNT - 1) ? 0 : spi::taskTail + 1;
		--spi::taskCount;
		
		// transfer pending tasks
		if (spi::taskCount > 0)
			transfer();

		task.onTransferred();			
	}
}

bool transfer(int csPin, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred)
{
	// check if task list is full
	if (spi::taskCount >= SPI_TASK_COUNT)
		return false;

	Task &task = allocateTask();
	task.csPin = csPin;
	task.writeData = intptr_t(writeData);
	task.writeLength = writeLength;
	task.readData = intptr_t(readData);
	task.readLength = readLength;
	task.onTransferred = onTransferred;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		transfer();

	return true;
}

} // namespace spi

#include "spi.hpp"
#include "loop.hpp"
#include "global.hpp"


/*
	resources:
	SPIM3
*/
namespace spi {

// queue of pending transfers
Queue<Transfer, SPI_TRANSFER_QUEUE_LENGTH> transferQueue;

// event loop handler chain
loop::Handler nextHandler;
void handle() {
	if (NRF_SPIM3->EVENTS_END) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_SPIM3->EVENTS_END = 0;
		clearInterrupt(SPIM3_IRQn);			

		// disable SPI
		NRF_SPIM3->ENABLE = 0;

		// get current transfer
		Transfer const &transfer = spi::transferQueue.get();
		if (transfer.readData == 1) {
			// restore MISO pin
			NRF_SPIM3->PSELDCX = Disconnected;
			NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
			//configureInputWithPullUp(SPI_MISO_PIN); // done automatically by hardware
		}
		spi::transferQueue.remove();
		
		// handle pending transfers
		if (!spi::transferQueue.empty())
			startTransfer();

		// callback to user code
		transfer.onTransferred();			
	}
	
	// call next handler in chain
	spi::nextHandler();
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

	// add to event loop handler chain
	spi::nextHandler = loop::addHandler(handle);
}

void startTransfer() {
	Transfer const &transfer = spi::transferQueue.get();
	
	// set CS pin
	NRF_SPIM3->PSEL.CSN = transfer.csPin;

	// set write data
	NRF_SPIM3->TXD.PTR = transfer.writeData;
	NRF_SPIM3->TXD.MAXCNT = transfer.writeLength;
	
	// check if this is a write-only transfer for display
	if (transfer.readData == 1) {
		// configure MISO pin as D/C# pin and set D/C# count
		NRF_SPIM3->PSEL.MISO = Disconnected;
		NRF_SPIM3->PSELDCX = SPI_MISO_PIN;
		//configureOutput(SPI_MISO_PIN); // done automatically by hardware
		NRF_SPIM3->DCXCNT = transfer.readLength;
	
		// no read data
		NRF_SPIM3->RXD.PTR = 0;
		NRF_SPIM3->RXD.MAXCNT = 0;
	} else {
		// set read data
		NRF_SPIM3->RXD.PTR = transfer.readData;
		NRF_SPIM3->RXD.MAXCNT = transfer.readLength;
	}
		
	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = Trigger;
}

bool transfer(int index, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> const &onTransferred)
{
	assert(uint(index) < array::size(SPI_CS_PINS));

	// check if transfer queue is full
	if (spi::transferQueue.full())
		return false;

	Transfer &transfer = spi::transferQueue.add();
	transfer.csPin = SPI_CS_PINS[index];
	transfer.writeData = intptr_t(writeData);
	transfer.writeLength = writeLength;
	transfer.readData = intptr_t(readData);
	transfer.readLength = readLength;
	transfer.onTransferred = onTransferred;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		startTransfer();

	return true;
}

} // namespace spi

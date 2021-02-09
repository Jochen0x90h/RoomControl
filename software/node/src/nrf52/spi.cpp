#include "../spi.hpp"
#include <nrf52/global.hpp>


namespace spi {

struct SpiTask {
	int csPin;
	
	intptr_t writeData;
	int writeLength;
	intptr_t readData;
	int readLength;
	
	std::function<void ()> onTransferred;
};

SpiTask spiTasks[SPI_TASK_COUNT];
int spiTaskHead = 0;
int spiTaskTail = 0;
int spiTaskCount = 0;

static SpiTask &allocateSpiTask() {
	SpiTask &task = spiTasks[spiTaskHead];
	spiTaskHead = (spiTaskHead == SPI_TASK_COUNT - 1) ? 0 : spiTaskHead + 1;
	++spiTaskCount;
	return task;
}

static void transferSpi() {
	SpiTask const &task = spiTasks[spiTaskTail];
	
	// set CS pin
	NRF_SPIM3->PSEL.CSN = task.csPin;

	// set write data
	NRF_SPIM3->TXD.PTR = task.writeData;
	NRF_SPIM3->TXD.MAXCNT = task.writeLength;
	
#ifdef HAVE_SPI_DISPLAY
	if (task.csPin == DISPLAY_CS_PIN) {
		// configure MISO pin as D/C# pin and set D/C# count
		NRF_SPIM3->PSEL.MISO = Disconnected;
		NRF_SPIM3->PSELDCX = SPI_MISO_PIN;
		configureOutput(SPI_MISO_PIN);
		NRF_SPIM3->DCXCNT = task.readLength;
	
		// no read data
		NRF_SPIM3->RXD.PTR = 0;
		NRF_SPIM3->RXD.MAXCNT = 0;
	} else
#endif	
	{
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
#ifdef HAVE_SPI_DISPLAY
	setOutput(DISPLAY_CS_PIN, true);
	configureOutput(DISPLAY_CS_PIN);	
#endif

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
		SpiTask const &task = spiTasks[spiTaskTail];
#ifdef HAVE_SPI_DISPLAY
		if (task.csPin == DISPLAY_CS_PIN) {
			// restore MISO pin
			NRF_SPIM3->PSELDCX = Disconnected;
			NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
			configureInputWithPullUp(SPI_MISO_PIN);
		}
#endif
		spiTaskTail = (spiTaskTail == SPI_TASK_COUNT - 1) ? 0 : spiTaskTail + 1;
		--spiTaskCount;
		
		// transfer pending tasks
		if (spiTaskCount > 0)
			transferSpi();

		task.onTransferred();			
	}
}

bool transfer(int csPin, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred)
{
	// check if task list is full
	if (spiTaskCount >= SPI_TASK_COUNT)
		return false;

	SpiTask &task = allocateSpiTask();
	task.csPin = csPin;
	task.writeData = intptr_t(writeData);
	task.writeLength = writeLength;
	task.readData = intptr_t(readData);
	task.readLength = readLength;
	task.onTransferred = onTransferred;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		transferSpi();

	return true;
}

#ifdef HAVE_SPI_DISPLAY
bool writeDisplay(uint8_t const* data, int commandLength, int dataLength, std::function<void ()> onWritten) {
	// check if task list is full
	if (spiTaskCount >= SPI_TASK_COUNT)
		return false;
	
	SpiTask &task = allocateSpiTask();
	task.csPin = DISPLAY_CS_PIN;
	task.writeData = intptr_t(data);
	task.writeLength = commandLength + dataLength;
	task.readData = 0;
	task.readLength = commandLength; // use readDataLength to store length of command part
	task.onTransferred = onWritten;

	// transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		transferSpi();

	return true;
}
#endif

} // namespace spi

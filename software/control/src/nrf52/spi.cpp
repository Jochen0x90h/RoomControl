#include "spi.hpp"
#include <nrf52/util.hpp>


namespace spi {

struct SpiTask {
	int csPin;
	
	intptr_t writeData;
	int writeLength;
	intptr_t readData;
	int readLength;
	
	std::function<void ()> onTransferred;
};

SpiTask spiTasks[4];
int spiTasksHead = 0;
int spiTasksTail = 0;

static SpiTask &allocateSpiTask() {
	SpiTask &task = spiTasks[spiTasksHead];
	spiTasksHead = (spiTasksHead + 1) & 3;	
	return task;
}

static void transferSpi() {
	SpiTask const &task = spiTasks[spiTasksTail];
	
	// set CS pin
	NRF_SPIM3->PSEL.CSN = task.csPin;

	// set write data
	NRF_SPIM3->TXD.PTR = task.writeData;
	NRF_SPIM3->TXD.MAXCNT = task.writeLength;
	
	if (task.csPin != DISPLAY_CS_PIN) {
		// set read data
		NRF_SPIM3->RXD.PTR = task.readData;
		NRF_SPIM3->RXD.MAXCNT = task.readLength;

	} else {
		// configure MISO pin as D/C# pin and set D/C# count
		NRF_SPIM3->PSEL.MISO = Disconnected;
		NRF_SPIM3->PSELDCX = SPI_MISO_PIN;
		configureOutput(SPI_MISO_PIN);
		NRF_SPIM3->DCXCNT = task.readLength;
	
		// no read data
		NRF_SPIM3->RXD.PTR = 0;
		NRF_SPIM3->RXD.MAXCNT = 0;
	}
		
	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = Trigger;
}

int displayTaskCount = 0;
int airSensorTaskCount = 0;
int feRamTaskCount = 0;


void init() {
	configureOutput(SPI_SCK_PIN);
	setOutput(SPI_MOSI_PIN, true);
	configureOutput(SPI_MOSI_PIN);
	setOutput(SPI_MISO_PIN, true);
	configureInputWithPullUp(SPI_MISO_PIN);
	
	setOutput(DISPLAY_CS_PIN, true);
	configureOutput(DISPLAY_CS_PIN);	
	setOutput(AIR_SENSOR_CS_PIN, true);
	configureOutput(AIR_SENSOR_CS_PIN);
	setOutput(FE_RAM_CS_PIN, true);
	configureOutput(FE_RAM_CS_PIN);

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
		SpiTask const &task = spiTasks[spiTasksTail];
		spiTasksTail = (spiTasksTail + 1) & 3;	

		if (task.csPin == DISPLAY_CS_PIN) {
			// restore MISO pin
			NRF_SPIM3->PSELDCX = Disconnected;
			NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
			configureInputWithPullUp(SPI_MISO_PIN);
			--displayTaskCount;
		} else if (task.csPin == AIR_SENSOR_CS_PIN) {
			--airSensorTaskCount;
		} else {
			--feRamTaskCount;
		}
		
		// transfer pending tasks if SPI is idle
		if (spiTasksHead != spiTasksTail && !NRF_SPIM3->ENABLE)
			transferSpi();

		task.onTransferred();			
	}
}

bool writeDisplay(uint8_t const* data, int commandLength, int dataLength, std::function<void ()> onWritten) {
	if (displayTaskCount > 2)
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

bool transferAirSensor(uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred)
{
	if (airSensorTaskCount > 1)
		return false;

	SpiTask &task = allocateSpiTask();
	task.csPin = AIR_SENSOR_CS_PIN;
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

bool transferFeRam(uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred)
{
	if (feRamTaskCount > 1)
		return false;

	SpiTask &task = allocateSpiTask();
	task.csPin = FE_RAM_CS_PIN;
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

} // namespace spi

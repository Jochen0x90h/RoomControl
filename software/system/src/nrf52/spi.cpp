#include "../spi.hpp"
#include "loop.hpp"
#include "defs.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Dependencies:
	
	Resources:
		SPIM3
		GPIO
			SPI_CS_PINS
			DISPLAY_CS_PIN
*/
namespace spi {

constexpr int CONTEXT_COUNT = array::count(SPIS);

struct Context {
	Waitlist<Parameters> waitlist;
};

Context contexts[CONTEXT_COUNT];

// index of context that is currently in progress
int transferContext;

static void startTransfer(int index, Parameters const &p) {
	// set CS pin
	NRF_SPIM3->PSEL.CSN = SPIS[index].csPin;

	// check if MISO and DC are on the same pin
	if (SPI_MISO_PIN == SPI_DC_PIN) {
		if (SPIS[index].type == SpiConfig::MASTER) {
			// read/write master: does not support DC 
			NRF_SPIM3->PSELDCX = DISCONNECTED;
			NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
		} else {
			// write only master: we can use MISO pin for DC signal
			NRF_SPIM3->PSEL.MISO = DISCONNECTED;
			NRF_SPIM3->PSELDCX = SPI_DC_PIN;
			//configureOutput(SPI_DC_PIN); // done automatically by hardware		
			NRF_SPIM3->DCXCNT = p.command ? p.writeLength : 0;
		}
	} else if (SPI_DC_PIN != DISCONNECTED) {
		// set command/data length
		NRF_SPIM3->DCXCNT = p.command ? p.writeLength : 0;
	}

	// set write data
	NRF_SPIM3->TXD.MAXCNT = p.writeLength;
	NRF_SPIM3->TXD.PTR = intptr_t(p.writeData);

	// set read data
	NRF_SPIM3->RXD.MAXCNT = p.readLength;
	NRF_SPIM3->RXD.PTR = intptr_t(p.readData);

	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = TRIGGER;
	spi::transferContext = index;
}

// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_SPIM3->EVENTS_END) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_SPIM3->EVENTS_END = 0;
		clearInterrupt(SPIM3_IRQn);
		int index = spi::transferContext;

		// resume first waiting coroutine
		auto &context = spi::contexts[index];
		context.waitlist.resumeFirst([](Parameters p) {
			return true;
		});

		// disable SPI (after resume to prevent queue-jumping of new transfers)
		NRF_SPIM3->ENABLE = 0;

		// check all contexts for more transfers
		do {
			index = index < spi::CONTEXT_COUNT - 1 ? index + 1 : 0;			
			auto &context = spi::contexts[index];
			
			// check if a coroutine is waiting on this context
			if (context.waitlist.resumeFirst([index](Parameters const &p) {
				startTransfer(index, p);

				// don't resume yet
				return false;
			})) {
				break;
			}
		} while (index != spi::transferContext);
	}
	
	// call next handler in chain
	spi::nextHandler();
}

void init() {
	// check if already initialized
	if (spi::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	spi::nextHandler = loop::addHandler(handle);

	// configure idle levels
	configureOutput(SPI_SCK_PIN);
	setOutput(SPI_MOSI_PIN, true);
	configureOutput(SPI_MOSI_PIN);
	configureInput(SPI_MISO_PIN, Pull::UP);
	if (SPI_MISO_PIN != SPI_DC_PIN && SPI_DC_PIN != DISCONNECTED)
		configureOutput(SPI_DC_PIN);
	
	// set cs pins to output and high to disable devices
	for (auto &spi : SPIS) {
		setOutput(spi.csPin, true);
		configureOutput(spi.csPin);
	}

	// configure spi pins
	NRF_SPIM3->PSEL.SCK = SPI_SCK_PIN;
	NRF_SPIM3->PSEL.MOSI = SPI_MOSI_PIN;
	
	// don't configure MISO and DC yet if they share the same pin
	if (SPI_MISO_PIN != SPI_DC_PIN) {
		NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;
		NRF_SPIM3->PSELDCX = SPI_DC_PIN;
	}
	
	NRF_SPIM3->INTENSET = N(SPIM_INTENSET_END, Set);
	NRF_SPIM3->FREQUENCY = N(SPIM_FREQUENCY_FREQUENCY, M1);
	NRF_SPIM3->CONFIG = N(SPIM_CONFIG_CPOL, ActiveHigh)
		| N(SPIM_CONFIG_CPHA, Leading)
		| N(SPIM_CONFIG_ORDER, MsbFirst);
}

Awaitable<Parameters> transfer(int index, int writeLength, uint8_t const *writeData, int readLength, uint8_t *readData,
	bool command)
{
	assert(spi::nextHandler != nullptr && uint(index) < SPI_CONTEXT_COUNT); // init() not called or index out of range
	auto &context = spi::contexts[index];

	Awaitable<Parameters> r = {context.waitlist, writeLength, writeData, readLength, readData, command};

	// start transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		startTransfer(index, r.element.value);//{writeLength, writeData, readLength, readData});
	
	return r;
	//return {context.waitlist, writeLength, writeData, readLength, readData};
}

} // namespace spi

#include "../spi.hpp"
#include "../display.hpp"
#include "loop.hpp"
#include "global.hpp"
#include "util.hpp"


/*
	Also implements display.hpp
	
	Resources:
	SPIM3
	GPIO
		SPI_CS_PINS
		DISPLAY_CS_PIN
*/
namespace spi {

constexpr int CONTEXT_COUNT = array::size(SPI_CS_PINS);

struct Context {
	Waitlist<Parameters> waitlist;
};

Context contexts[CONTEXT_COUNT];

int transferContext;

static void startTransfer(int index, Parameters const &p) {
	// set CS pin
	NRF_SPIM3->PSEL.CSN = SPI_CS_PINS[index];

	// configure MISO pin
	NRF_SPIM3->PSELDCX = Disconnected;
	NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN;

	// set write data
	NRF_SPIM3->TXD.MAXCNT = p.writeLength;
	NRF_SPIM3->TXD.PTR = intptr_t(p.writeData);

	// set read data
	NRF_SPIM3->RXD.MAXCNT = p.readLength;
	NRF_SPIM3->RXD.PTR = intptr_t(p.readData);

	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = Trigger;
	spi::transferContext = index;
}

} // namespace spi

namespace display {

Waitlist<Parameters> waitlist;

void startTransfer(int index, Parameters const &p) {
	// set CS pin
	NRF_SPIM3->PSEL.CSN = DISPLAY_CS_PIN;

	// configure MISO pin as D/C# pin and set D/C# count
	NRF_SPIM3->PSEL.MISO = Disconnected;
	NRF_SPIM3->PSELDCX = SPI_MISO_PIN;
	//configureOutput(SPI_MISO_PIN); // done automatically by hardware
	NRF_SPIM3->DCXCNT = p.commandLength;
	
	// set write data
	NRF_SPIM3->TXD.MAXCNT = p.commandLength + p.dataLength;
	NRF_SPIM3->TXD.PTR = intptr_t(p.data);

	// no read data
	NRF_SPIM3->RXD.MAXCNT = 0;

	// enable and start
	NRF_SPIM3->ENABLE = N(SPIM_ENABLE_ENABLE, Enabled);
	NRF_SPIM3->TASKS_START = Trigger;
	
	spi::transferContext = index;
}

} // namespace display

namespace spi {

// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_SPIM3->EVENTS_END) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_SPIM3->EVENTS_END = 0;
		clearInterrupt(SPIM3_IRQn);
		int index = spi::transferContext;

		// resume first waiting coroutine
		if (index < spi::CONTEXT_COUNT) {
			auto &context = spi::contexts[index];
			context.waitlist.resumeFirst([](Parameters p) {
				return true;
			});
		} else {
			display::waitlist.resumeFirst([](display::Parameters p) {
				return true;
			});
		}

		// disable SPI (after resume to prevent queue-jumping of new transfers)
		NRF_SPIM3->ENABLE = 0;

		// check all contexts for more transfers
		do {
			index = index < spi::CONTEXT_COUNT ? index + 1 : 0;
			
			if (index < spi::CONTEXT_COUNT) {
				auto &context = spi::contexts[index];
				
				// check if a coroutine is waiting on this context
				if (context.waitlist.resumeFirst([index](Parameters const &p) {
					startTransfer(index, p);

					// don't resume yet
					return false;
				})) {
					break;
				}
			} else {
				// display: check if a coroutine is waiting on this context
				if (display::waitlist.resumeFirst([](display::Parameters const &p) {
					display::startTransfer(spi::CONTEXT_COUNT, p);
					
					// don't resume yet
					return false;
				})) {
					break;
				}
			}
		} while (index != spi::transferContext);
	}
	
	// call next handler in chain
	spi::nextHandler();
}

void init() {
	if (spi::nextHandler != nullptr)
		return;

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
	//NRF_SPIM3->PSEL.MISO = SPI_MISO_PIN; // gets configured in startTransfer()
	NRF_SPIM3->FREQUENCY = N(SPIM_FREQUENCY_FREQUENCY, M1);
	NRF_SPIM3->CONFIG = N(SPIM_CONFIG_CPOL, ActiveHigh)
		| N(SPIM_CONFIG_CPHA, Leading)
		| N(SPIM_CONFIG_ORDER, MsbFirst);

	// add to event loop handler chain
	spi::nextHandler = loop::addHandler(handle);
}

Awaitable<Parameters> transfer(int index, int writeLength, uint8_t const *writeData, int readLength, uint8_t *readData) {
	assert(uint(index) < spi::CONTEXT_COUNT);
	auto &context = spi::contexts[index];

	// start transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		startTransfer(index, {writeLength, writeData, readLength, readData});

	return {context.waitlist, writeLength, writeData, readLength, readData};
}

} // namespace spi

// display is connected via spi
namespace display {

void init() {
	spi::init();
	setOutput(DISPLAY_CS_PIN, true);
	configureOutput(DISPLAY_CS_PIN);
}

Awaitable<Parameters> send(int commandLength, int dataLength, uint8_t const *data) {
	// start transfer immediately if SPI is idle
	if (!NRF_SPIM3->ENABLE)
		startTransfer(spi::CONTEXT_COUNT, {commandLength, dataLength, data});

	return {display::waitlist, commandLength, dataLength, data};
}

} // namespace display

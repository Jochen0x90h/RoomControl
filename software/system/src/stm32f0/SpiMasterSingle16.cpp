#include "SpiMasterSingle16.hpp"
#include "defs.hpp"
#include <util.hpp>


/*
	Reference manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
	Data sheet: https://www.st.com/resource/en/datasheet/stm32f042f6.pdf

	Dependencies:

	Config:
		SPI_CONTEXTS: Configuration of SPI channels (can share the same SPI peripheral)

	Resources:
		SPI1: SPI master (reference manual section 28)
		GPIO
			CS-pin
*/

using namespace gpio;

namespace {

// for alternate functions, see STM32F042x4 STM32F042x6 datasheet
inline PinFunction sckFunction(int pin) {
	assert(pin == PA(5) || pin == PB(3));
	return {pin, 0};
}

inline PinFunction mosiFunction(int pin) {
	assert(pin == PA(7) || pin == PB(5));
	return {pin, 0};
}

inline PinFunction misoFunction(int pin) {
	assert(pin == PA(6) || pin == PB(4));
	return {pin, 0};
}


// CR1 register
// ------------

// SPI mode
constexpr int SPI_CR1_OFF = 0;
constexpr int SPI_CR1_FULL_DUPLEX_MASTER = SPI_CR1_MSTR;

// SPI clock phase and polarity
constexpr int SPI_CR1_CPHA_0 = 0;
constexpr int SPI_CR1_CPHA_1 = SPI_CR1_CPHA;
constexpr int SPI_CR1_CPOL_0 = 0;
constexpr int SPI_CR1_CPOL_1 = SPI_CR1_CPOL;

// SPI bit order
constexpr int SPI_CR1_LSB_FIRST = SPI_CR1_LSBFIRST;
constexpr int SPI_CR1_MSB_FIRST = 0;

// SPI prescaler
constexpr int SPI_CR1_DIV2 = 0;
constexpr int SPI_CR1_DIV4 = (SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV8 = (SPI_CR1_BR_1);
constexpr int SPI_CR1_DIV16 = (SPI_CR1_BR_1 | SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV32 = (SPI_CR1_BR_2);
constexpr int SPI_CR1_DIV64 = (SPI_CR1_BR_2 | SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV128 = (SPI_CR1_BR_2 | SPI_CR1_BR_1);
constexpr int SPI_CR1_DIV256 = (SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0);

// CR2 register
// ------------

// SPI data size (8, 16, 32)
constexpr int SPI_CR2_DATA_SIZE(int s) { return (s - 1) << SPI_CR2_DS_Pos; }


// configuration

constexpr int SPI_CR1 = SPI_CR1_SPE
	| SPI_CR1_FULL_DUPLEX_MASTER
	| SPI_CR1_CPHA_1 | SPI_CR1_CPOL_0 // shift on rising edge, sample on falling edge
	| SPI_CR1_MSB_FIRST
	| SPI_CR1_DIV8;

constexpr int SPI_CR2 = SPI_CR2_DATA_SIZE(16)
	| SPI_CR2_RXNEIE // RXNE interrupt enable
	| SPI_CR2_SSOE; // single master mode

}


// SpiMasterSingle16

SpiMasterSingle16::SpiMasterSingle16(int sckPin, int mosiPin, int misoPin, int csPin) : csPin(csPin) {
	// configure SPI pins (driven low when SPI is disabled)
	configureAlternateOutput(sckFunction(sckPin));
	configureAlternateOutput(mosiFunction(mosiPin));
	configureAlternateOutput(misoFunction(misoPin));

	// configure CS pin: output, high on idle
	gpio::setOutput(csPin, true);
	gpio::configureOutput(csPin);

	// initialize TIM14
	RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_TIM14EN;

	// add to list of handlers
	Loop::handlers.add(*this);
}

void SpiMasterSingle16::handle() {
	if (SPI1->SR & SPI_SR_RXNE) {
		// set CS pin high
		gpio::setOutput(this->csPin, true);

		// read data and clear RXNE flag
		uint16_t data = SPI1->DR;

		// clear pending interrupt flags at NVIC
		clearInterrupt(SPI1_IRQn);

		// disable timer and SPI
		SPI1->CR1 = 0;
		RCC->APB2ENR = RCC->APB2ENR & ~RCC_APB2ENR_SPI1EN;

		// check for more transfers
		this->waitlist.visitSecond([this](const SpiMaster::Parameters &p) {
			startTransfer(p);
		});

		// resume first waiting coroutine
		this->waitlist.resumeFirst([data](const SpiMaster::Parameters &p) {
			*reinterpret_cast<uint16_t*>(p.readData) = data;
			return true;
		});
	}
}

void SpiMasterSingle16::startTransfer(const SpiMaster::Parameters &p) {
	// initialize SPI
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_SPI1EN;
	SPI1->CR2 = SPI_CR2;
	SPI1->CR1 = SPI_CR1;

	// set CS pin low and store it
	gpio::setOutput(this->csPin, false);

	// write data
	SPI1->DR = *reinterpret_cast<uint16_t const*>(p.writeData);

	// now wait for SPI_SR_RXNE flag
}

Awaitable<SpiMaster::Parameters> SpiMasterSingle16::transfer(int writeCount, void const *writeData, int readCount,
	void *readData)
{
	// start transfer immediately if SPI is idle
	if (SPI1->CR1 == 0) {
		Parameters parameters{this, writeCount, writeData, readCount, readData};
		this->startTransfer(parameters);
	}

	return {waitlist, this, writeCount, writeData, readCount, readData};
}

void SpiMasterSingle16::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
}

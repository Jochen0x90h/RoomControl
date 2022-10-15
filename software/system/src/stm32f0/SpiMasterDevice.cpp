#include "SpiMasterDevice.hpp"
#include "defs.hpp"
#include <util.hpp>


/*
	Dependencies:

	Config:
		SPI_CONTEXTS: Configuration of SPI channels (can share the same SPI peripheral)

	Resources:
		SPI1
		TIM14
		GPIO
			CS-pins
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
	| SPI_CR2_SSOE;

}


// SpiMasterDevice

SpiMasterDevice::SpiMasterDevice(int index, int sckPin, int mosiPin, int misoPin) {
	// configure SPI pins (driven low when SPI is disabled)
	configureAlternateOutput(sckFunction(sckPin));
	configureAlternateOutput(mosiFunction(mosiPin));
	configureAlternateOutput(misoFunction(misoPin));

	// initialize TIM14
	RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_TIM14EN;
	TIM14->DIER = TIM_DIER_CC1IE; // interrupt enable for CC1

	// add to list of handlers
	Loop::handlers.add(*this);
}

void SpiMasterDevice::handle() {
	if (TIM14->SR & TIM_SR_CC1IF) {
	//if (SPI1->SR & SPI_SR_TXE) {
		// set CS pin high
		gpio::setOutput(this->csPin, true);

		// clear pending interrupt flags at peripheral and NVIC
		TIM14->SR = ~TIM_SR_CC1IF;
		clearInterrupt(TIM14_IRQn);

		// read
		//int x = SPI1->DR;

		// disable timer and SPI
		TIM14->CR1 = 0;
		SPI1->CR1 = 0;
		RCC->APB2ENR = RCC->APB2ENR & ~RCC_APB2ENR_SPI1EN;

		// check for more transfers
		this->waitlist.visitSecond([this](const SpiMaster::Parameters &p) {
			startTransfer(p);
		});

		// resume first waiting coroutine
		this->waitlist.resumeFirst([](const SpiMaster::Parameters &p) {
			return true;
		});
	}
}

void SpiMasterDevice::startTransfer(const SpiMaster::Parameters &p) {
	auto &channel = *reinterpret_cast<const SpiMasterDevice::Channel *>(p.config);

	// initialize SPI
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_SPI1EN;
	SPI1->CR2 = SPI_CR2;
	SPI1->CR1 = SPI_CR1;

	// set CS pin low and store it
	gpio::setOutput(channel.csPin, false);
	this->csPin = channel.csPin;

	// prepare timer
	TIM14->PSC = (2 << ((SPI_CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)) * 16 - 1;
	TIM14->CCR1 = 1;
	TIM14->EGR = TIM_EGR_UG; // clear

	// write data
	// todo: use DMA, now only the first word is transferred
	SPI1->DR = *(uint16_t*)p.writeData;

	// start timer
	TIM14->CR1 = TIM_CR1_CEN;
}


// SpiMasterDevice::Channel

SpiMasterDevice::Channel::Channel(SpiMasterDevice &device, int csPin)
	: device(device), csPin(csPin)
{
	// configure CS pin: output, high on idle
	gpio::setOutput(csPin, true);
	gpio::configureOutput(csPin);
}

Awaitable<SpiMaster::Parameters> SpiMasterDevice::Channel::transfer(int writeCount, void const *writeData, int readCount,
	void *readData)
{
	// start transfer immediately if SPI is idle
	if ((TIM14->CR1 & TIM_CR1_CEN) == 0) {
		Parameters parameters{this, writeCount, writeData, readCount, readData};
		this->device.startTransfer(parameters);
	}

	return {device.waitlist, this, writeCount, writeData, readCount, readData};
}

void SpiMasterDevice::Channel::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	// check if a transfer is running
	bool running = (TIM14->CR1 & TIM_CR1_CEN) != 0;

	// wait for end of running transfer
	if (running) {
		while (!(TIM14->SR & TIM_SR_CC1IF))
			__NOP();
	}

	Parameters parameters{this, writeCount, writeData, readCount, readData};
	this->device.startTransfer(parameters);

	// wait for end of transfer
	while (!(TIM14->SR & TIM_SR_CC1IF))
		__NOP();

	// clear pending interrupt flags at peripheral and NVIC unless a transfer was running which gets handled in handle()
	if (!running) {
		TIM14->SR = ~TIM_SR_CC1IF;
		clearInterrupt(TIM14_IRQn);
	}
}

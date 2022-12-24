#include "SpiMasterImpl.hpp"
#include "defs.hpp"
#include "gpio.hpp"
#include <util.hpp>


/*
	Reference manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
	Data sheet: https://www.st.com/resource/en/datasheet/stm32f042f6.pdf

	Dependencies:

	Config:
		SPI_CONTEXTS: Configuration of SPI channels (can share the same SPI peripheral)

	Resources:
		SPI1: SPI master (reference manual section 28)
		DMA1 (reference manual section 10)
			Channel2: Read (reference manual table 29)
			Channel3: Write
		GPIO
			CS-pins
*/

using namespace gpio;

namespace {

// for alternate functions, see Table 14 and 15 in datasheet
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

constexpr int SPI_CR2 = SPI_CR2_DATA_SIZE(8)
	| SPI_CR2_FRXTH
	| SPI_CR2_SSOE; // single master mode

}


// SpiMasterImpl

SpiMasterImpl::SpiMasterImpl(int index, int sckPin, int mosiPin, int misoPin) {
	// configure SPI pins (driven low when SPI is disabled)
	configureAlternateOutput(sckFunction(sckPin));
	configureAlternateOutput(mosiFunction(mosiPin));
	configureAlternateOutput(misoFunction(misoPin));

	// initialize DMA
	RCC->AHBENR = RCC->AHBENR | RCC_AHBENR_DMAEN;
	DMA1_Channel2->CPAR = (uint32_t)&SPI1->DR;
	DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
	RCC->AHBENR = RCC->AHBENR & ~RCC_AHBENR_DMAEN;

	// add to list of handlers
	Loop::handlers.add(*this);
}

void SpiMasterImpl::handle() {
	// check if read DMA has completed
	if ((DMA1->ISR & DMA_ISR_TCIF2) != 0) {
		// clear interrupt flags at DMA and NVIC
		DMA1->IFCR = DMA_IFCR_CTCIF2;
		clearInterrupt(DMA1_Ch2_3_DMA2_Ch1_2_IRQn);

		if (update()) {
			// end of transfer

			// disable clocks
			RCC->APB2ENR = RCC->APB2ENR & ~RCC_APB2ENR_SPI1EN;
			RCC->AHBENR = RCC->AHBENR & ~RCC_AHBENR_DMAEN;

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
}

void SpiMasterImpl::startTransfer(const SpiMaster::Parameters &p) {
	auto &channel = *reinterpret_cast<const Channel *>(p.config);

	// enable clocks
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_SPI1EN;
	RCC->AHBENR = RCC->AHBENR | RCC_AHBENR_DMAEN;

	// set transfer state
	this->csPin = channel.csPin;
	this->readCount = p.readCount;
	this->readAddress = (uint32_t)p.readData;
	this->writeCount = p.writeCount;
	this->writeAddress = (uint32_t)p.writeData;

	// update transfer
	update();

	// now wait for DMA_ISR_TCIF2 flag
}

bool SpiMasterImpl::update() {
	if (this->readCount > 0) {
		SPI1->CR1 = 0;
		DMA1_Channel2->CCR = 0;
		DMA1_Channel3->CCR = 0;
		SPI1->CR2 = SPI_CR2 | SPI_CR2_RXDMAEN; // first enable receive DMA according to data sheet
		if (this->writeCount > 0) {
			// read and write
			int count = min(this->readCount, this->writeCount);
			DMA1_Channel2->CNDTR = count;
			DMA1_Channel2->CMAR = this->readAddress;
			DMA1_Channel2->CCR = DMA_CCR_EN // enable read channel
				| DMA_CCR_TCIE // transfer complete interrupt enable
				| DMA_CCR_MINC; // auto-increment memory
			DMA1_Channel3->CNDTR = count;
			DMA1_Channel3->CMAR = this->writeAddress;
			DMA1_Channel3->CCR = DMA_CCR_EN // enable write channel
				| DMA_CCR_DIR // read from memory
				| DMA_CCR_MINC; // auto-increment memory

			this->readAddress += count;
			this->writeAddress += count;
			this->readCount -= count;
			this->writeCount -= count;
		} else {
			// read only
			DMA1_Channel2->CNDTR = this->readCount;
			DMA1_Channel2->CMAR = this->readAddress;
			DMA1_Channel2->CCR = DMA_CCR_EN // enable read channel
				| DMA_CCR_TCIE // transfer complete interrupt enable
				| DMA_CCR_MINC; // auto-increment memory
			DMA1_Channel3->CNDTR = this->readCount;
			DMA1_Channel3->CMAR = (uint32_t)&this->writeDummy; // write from dummy
			DMA1_Channel3->CCR = DMA_CCR_EN // enable write channel
				| DMA_CCR_DIR; // read from memory

			this->readCount = 0;
		}
	} else if (this->writeCount > 0) {
		// write only
		SPI1->CR1 = 0;
		DMA1_Channel2->CCR = 0;
		DMA1_Channel3->CCR = 0;
		SPI1->CR2 = SPI_CR2 | SPI_CR2_RXDMAEN; // first enable receive DMA according to data sheet

		DMA1_Channel2->CNDTR = this->writeCount;
		DMA1_Channel2->CMAR = (uint32_t)&this->readDummy; // read into dummy
		DMA1_Channel2->CCR = DMA_CCR_EN // enable read channel
			| DMA_CCR_TCIE; // transfer complete interrupt enable
		DMA1_Channel3->CNDTR = this->writeCount;
		DMA1_Channel3->CMAR = this->writeAddress;
		DMA1_Channel3->CCR = DMA_CCR_EN // enable write channel
			| DMA_CCR_DIR // read from memory
			| DMA_CCR_MINC; // auto-increment memory

		this->writeCount = 0;
	} else {
		// end of transfer

		// set CS pin high
		gpio::setOutput(this->csPin, true);

		// disable SPI and DMA
		SPI1->CR1 = 0;
		DMA1_Channel2->CCR = 0;
		DMA1_Channel3->CCR = 0;

		return true;
	}

	// set CS pin low
	gpio::setOutput(this->csPin, false);

	// start SPI
	SPI1->CR2 = SPI_CR2 | SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
	SPI1->CR1 = SPI_CR1;

	return false;
}


// SpiMasterImpl::Channel

SpiMasterImpl::Channel::Channel(SpiMasterImpl &master, int csPin)
	: master(master), csPin(csPin)
{
	// configure CS pin: output, high on idle
	gpio::setOutput(csPin, true);
	gpio::configureOutput(csPin);
}

Awaitable<SpiMaster::Parameters> SpiMasterImpl::Channel::transfer(int writeCount, void const *writeData, int readCount,
	void *readData)
{
	// start transfer immediately if SPI is idle
	if (DMA1_Channel2->CCR == 0) {
		Parameters parameters{this, writeCount, writeData, readCount, readData};
		this->master.startTransfer(parameters);
	}

	return {master.waitlist, this, writeCount, writeData, readCount, readData};
}

void SpiMasterImpl::Channel::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	// check if a transfer is running
	bool running = DMA1_Channel2->CCR != 0;

	// wait for end of running transfer
	if (running) {
		do {
			while ((DMA1->ISR & DMA_ISR_TCIF2) == 0)
				__NOP();
		} while (!this->master.update());
	}

	// clear pending interrupt flag and disable SPI and DMA
	DMA1->IFCR = DMA_IFCR_CTCIF2;

	Parameters parameters{this, writeCount, writeData, readCount, readData};
	this->master.startTransfer(parameters);

	// wait for end of transfer
	do {
		while ((DMA1->ISR & DMA_ISR_TCIF2) == 0)
			__NOP();
	} while (!this->master.update());

	// clear pending interrupt flags at DMA and NVIC unless a transfer was running which gets handled in handle()
	if (!running) {
		DMA1->IFCR = DMA_IFCR_CTCIF2;
		clearInterrupt(DMA1_Ch2_3_DMA2_Ch1_2_IRQn);

		// disable clocks
		RCC->APB2ENR = RCC->APB2ENR & ~RCC_APB2ENR_SPI1EN;
		RCC->AHBENR = RCC->AHBENR & ~RCC_AHBENR_DMAEN;
	}
}

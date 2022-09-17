#include "../Spi.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


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
namespace Spi {

// CR1
// ---

// SPI mode
constexpr int SPI_CR1_OFF                = 0;
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
constexpr int SPI_CR1_DIV2   = 0;
constexpr int SPI_CR1_DIV4   = (SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV8   = (SPI_CR1_BR_1);
constexpr int SPI_CR1_DIV16  = (SPI_CR1_BR_1 | SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV32  = (SPI_CR1_BR_2);
constexpr int SPI_CR1_DIV64  = (SPI_CR1_BR_2 | SPI_CR1_BR_0);
constexpr int SPI_CR1_DIV128 = (SPI_CR1_BR_2 | SPI_CR1_BR_1);
constexpr int SPI_CR1_DIV256 = (SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0);

// CR2
// ---

// SPI data width
constexpr int SPI_CR2_DATA_SIZE(int s) {return (s - 1) << SPI_CR2_DS_Pos;}


// configuration

constexpr int SPI_CR1 = SPI_CR1_SPE
	| SPI_CR1_FULL_DUPLEX_MASTER
	| SPI_CR1_CPHA_1 | SPI_CR1_CPOL_0 // shift on rising edge, sample on falling edge
	| SPI_CR1_MSB_FIRST
	| SPI_CR1_DIV8;

constexpr int SPI_CR2 = SPI_CR2_DATA_SIZE(16)
	| SPI_CR2_SSOE;



constexpr int CONTEXT_COUNT = array::count(SPI_CONTEXTS);

struct Context {
	Waitlist<Parameters> waitlist;
};

Context contexts[CONTEXT_COUNT];

// index of context that is currently in progress
int transferContext;

static void startTransfer(int index, Parameters const &p) {
	// initialize SPI
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_SPI1EN;
	SPI1->CR2 = SPI_CR2;
	SPI1->CR1 = SPI_CR1;

	// set cs pin low
	gpio::setOutput(SPI_CONTEXTS[index].csPin, false);
	
	// store current index
	Spi::transferContext = index;
	
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

// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (TIM14->SR & TIM_SR_CC1IF) {
	//if (SPI1->SR & SPI_SR_TXE) {
		int index = Spi::transferContext;

		// read
		//int x = SPI1->DR;
		
		// set CS pin high
		gpio::setOutput(SPI_CONTEXTS[index].csPin, true);
		
		// disable SPI
		SPI1->CR1 = 0;
		RCC->APB2ENR = RCC->APB2ENR & ~RCC_APB2ENR_SPI1EN;

		// resume first waiting coroutine
		auto &context = Spi::contexts[index];
		context.waitlist.resumeFirst([](Parameters p) {
			return true;
		});

		// stop timer (after resume to prevent queue-jumping of new transfers)
		TIM14->CR1 = 0;

		// clear pending interrupt flags at peripheral and NVIC
		TIM14->SR = ~TIM_SR_CC1IF;
		clearInterrupt(TIM14_IRQn);

		// check all contexts for more transfers
		do {
			index = index < Spi::CONTEXT_COUNT - 1 ? index + 1 : 0;			
			auto &context = Spi::contexts[index];
			
			// check if a coroutine is waiting on this context
			if (context.waitlist.visitFirst([index](Parameters const &p) {
				startTransfer(index, p);
			})) {
				break;
			}
		} while (index != Spi::transferContext);
	}	
	
	// call next handler in chain
	Spi::nextHandler();
}

void init() {
	// check if already initialized
	if (Spi::nextHandler != nullptr)
		return;

	// configure CS pins
	for (auto &config : SPI_CONTEXTS) {
		gpio::setOutput(config.csPin, true);
		gpio::configureOutput(config.csPin);
	}

	// configure SPI pins (driven low when SPI is disabled)
	configureAlternateOutput(gpio::SPI_SCK<SPI_SCK_PIN>());
	configureAlternateOutput(gpio::SPI_MOSI<SPI_MOSI_PIN>());
	configureAlternateOutput(gpio::SPI_MISO<SPI_MISO_PIN>());

	// initialize TIM14
	RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_TIM14EN;
	TIM14->DIER = TIM_DIER_CC1IE; // interrupt enable for CC1

	// add to event loop handler chain
	Spi::nextHandler = Loop::addHandler(handle);
}

Awaitable<Parameters> transfer(int index, int writeLength, void const *writeData, int readLength, void *readData) {
	assert(Spi::nextHandler != nullptr && uint(index) < SPI_CONTEXT_COUNT); // init() not called or index out of range
	auto &context = Spi::contexts[index];

	// start transfer immediately if SPI is idle
	if ((TIM14->CR1 & TIM_CR1_CEN) == 0) {
		Parameters parameters{writeLength, writeData, readLength, readData};
		startTransfer(index, parameters);
	}
	
	return {context.waitlist, writeLength, writeData, readLength, readData};
}

} // namespace Spi

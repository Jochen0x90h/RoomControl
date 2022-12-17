#include "BusNodeImpl.hpp"
#include "defs.hpp"
#include "gpio.hpp"
#include "boardConfig.hpp" // CLOCK
#include "Debug.hpp"


/*
	Reference manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
	Data sheet: https://www.st.com/resource/en/datasheet/stm32f042f6.pdf https://www.st.com/resource/en/datasheet/stm32f051r8.pdf

	Resources:
		USART1: Bus (reference manual section 27)
		TIM16: Timeout (reference manual section 20)
		IRQ17: Transfer complete (https://community.arm.com/support-forums/f/architectures-and-processors-forum/4070/using-interrupts-not-implemented-as-software-interrupts)
*/

using namespace gpio;

constexpr int COMPLETE_IRQ = 17;
constexpr int REQUEST_INTERVAL = 50 * 20; // 50ms
constexpr int REQUEST_BYTE = 0xff;

namespace {

// for alternate functions, see Table 14 and 15 in data sheet
inline PinFunction rxFunction(int pin) {
	assert(pin == PA(10) || pin == PB(7));
	return {pin, pin == PA(10) ? 1 : 0};
}

inline PinFunction txFunction(int pin) {
	assert(pin == PA(9) || pin == PB(6));
	return {pin, pin == PA(9) ? 1 : 0};
}

// instance pointer used in interrupt handlers
BusNodeImpl *busNode;

}

BusNodeImpl::BusNodeImpl(int rxPin, int txPin) : rxPin(rxPin) {
	const auto uart = USART1;
	const auto timer = TIM16;

	// set instance pointer
	busNode = this;

	// configure UART pins
	configureAlternateOutput(rxFunction(rxPin));
	configureAlternateOutput(txFunction(txPin));

	// initialize UART
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_USART1EN; // clock enable
	uart->BRR = (CLOCK + 9600) / 19200; // baud rate
	//uart->CR2 = USART_CR2_LINEN // LIN enable
	//	| USART_CR2_LBDIE; // LIN break interrupt enable
	uart->CR1 = USART_CR1_UE // enable
		| USART_CR1_RE // rx enable
		| USART_CR1_TE // tx enable
		| USART_CR1_RXNEIE; // rx interrupt enable
	enableInterrupt(USART1_IRQn);

	// initialize timer
	RCC->APB2ENR = RCC->APB2ENR | RCC_APB2ENR_TIM16EN; // clock enable
	timer->PSC = (CLOCK + 9600) / 19200 + 1; // prescaler for bit time (f = CLOCK / (PSC + 1) -> PSC = CLOCK / f - 1)
	timer->DIER = TIM_DIER_CC1IE; // interrupt enable for CC1
	timer->CCR1 = REQUEST_INTERVAL; // set timeout, but do not start timer yet
	enableInterrupt(TIM16_IRQn);

	// add to list of handlers
	Loop::handlers.add(*this);
}

Awaitable<BusNode::ReceiveParameters> BusNodeImpl::receive(int &length, uint8_t *data) {
	// start receiving immediately if no rx is pending and bus is idle
	if (this->rxState == RxState::IDLE) {
		ReceiveParameters p{&length, data};
		startReceive(p);
	}

	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusNode::SendParameters> BusNodeImpl::send(int length, uint8_t const *data) {
	if (length <= 0)
		return {};

	// start sending immediately if no tx is pending and bus is idle
	if (this->txState == TxState::IDLE) {
		//Debug::toggleRedLed();
		SendParameters p{length, data};
		startSend(p);
	}

	return {this->sendWaitlist, length, data};
}

void BusNodeImpl::handle() {
	if (isInterruptPending(COMPLETE_IRQ)) {
		// check if tx or rx operation has finished
		if (this->txState == TxState::FINISHED) {
			// send operation complete
			this->txState = TxState::IDLE;

			// check if a next send operation is pending
			this->sendWaitlist.visitSecond([this](const SendParameters &p) {
				startSend(p);
			});

			// resume first waiting coroutine
			this->sendWaitlist.resumeFirst([](const SendParameters &p) {
				return true;
			});
		} else if (this->rxState == RxState::FINISHED) {
			// receive operation complete
			int rxLength = this->rxData - this->rxBegin;
			this->rxState = RxState::IDLE;

			// check if a next receive operation is pending
			this->receiveWaitlist.visitSecond([this](const ReceiveParameters &p) {
				startReceive(p);
			});

			// resume first waiting coroutine
			this->receiveWaitlist.resumeFirst([rxLength](const ReceiveParameters &p) {
				*p.length = rxLength;
				return true;
			});
		}

		// clear pending interrupt flag at NVIC
		clearInterrupt(COMPLETE_IRQ);
	}
}

void BusNodeImpl::startReceive(const ReceiveParameters &p) {
	this->rxState = RxState::PENDING;
	this->rxBegin = p.data;
	this->rxEnd = p.data + *p.length;

	// wait for master to start a transfer
}

void BusNodeImpl::startSend(const SendParameters &p) {
	this->txState = TxState::PENDING;
	this->txBegin = p.data;
	this->txEnd = p.data + p.length;

	// request a transfer when bus is idle
	if (this->state == State::IDLE)
		startRequest();
}

void BusNodeImpl::startRequest() {
	const auto uart = USART1;
	const auto timer = TIM16;

	// send a single start bit to inform the master that we want to send data
	uart->TDR = REQUEST_BYTE;

	// repeat in case the master does not react
	timer->CCR1 = REQUEST_INTERVAL; // set timeout
	timer->CR1 = TIM_CR1_CEN; // start imer
}


// interrupt handlers

void USART1_IRQHandler() {
	busNode->usartIrqHandler();
}

void BusNodeImpl::usartIrqHandler() {
	const auto uart = USART1;
	const auto timer = TIM16;

	// stop and reset timer
	timer->CR1 = 0; // stop timer
	timer->EGR = TIM_EGR_UG; // reset timer

	// clear timer interrupt in case it triggered just before stop
	timer->SR = ~TIM_SR_CC1IF;
	clearInterrupt(TIM16_IRQn);

	// get interrupt status flags
	auto isr = uart->ISR;

	if (isr & USART_ISR_LBDF) {
		// break detected
		uart->ICR = USART_ICR_LBDCF;

		// start receive timeout
		timer->CCR1 = 20; // 2 characters = 20 bits
		timer->CR1 = TIM_CR1_CEN; // start timer

		// now wait for sync
		this->state = State::SYNC;
	}
	if (isr & USART_ISR_RXNE) {
		// received a byte
		uint8_t b = uart->RDR;

		switch (this->state) {
		case State::IDLE:
			// restart timer when tx is pending
			if (this->txState >= TxState::PENDING) {
				timer->CR1 = TIM_CR1_CEN; // restart timer
			}
			break;
		case State::SYNC:
			// check if sync byte was received correctly
			if (b != 0x55) {
				// go to error state and skip incoming bytes until a timeout occurs
				this->state = State::ERROR;
			} else {
				// sync ok: now start to transfer data
				if (this->rxState >= RxState::PENDING) {
					this->rxState = RxState::ACTIVE;
					this->rxData = this->rxBegin;
				}

				// send first byte if tx is pending
				if (this->txState >= TxState::PENDING) {
					this->txState = TxState::ACTIVE;
					auto data = this->txBegin;
					this->txData = data + 1;
					b = *data;
					uart->TDR = b;
					this->txByte = b;
				} else {
					// tx is idle: stop transmit
					//uart->TASKS_STOPTX = TRIGGER;
				}

				// now in transfer state
				this->state = State::TRANSFER;
			}
			timer->CR1 = TIM_CR1_CEN; // restart timer
			break;
		case State::TRANSFER:
			// received next byte

			// when tx is active, check if received byte is equal to the last sent byte
			if (this->txState == TxState::ACTIVE) {
				// check if byte was transferred ok and the current state of the bus is high
				if (b == this->txByte && gpio::readInput(this->rxPin)) {
					// byte was transferred ok
					auto data = this->txData;
					if (data < this->txEnd) {
						// send next byte
						this->txData = data + 1;
						b = *data;
						uart->TDR = b;
						this->txByte = b;
					} else {
						// end: stop transmit and set tx state to success
						//uart->TASKS_STOPTX = TRIGGER;
						this->txState = TxState::END;
					}
				} else {
					// error: stop transmit as probably another node won the bus arbitration
					//uart->TASKS_STOPTX = TRIGGER;
					this->txState = TxState::PENDING;
				}
			} else if (this->txState == TxState::END) {
				// error: extra byte received after successful transmission
				this->txState = TxState::PENDING;
			}

			// check if receive is pending
			if (this->rxState == RxState::ACTIVE) {
				auto data = this->rxData;
				if (data < this->rxEnd) {
					// add next byte to receive buffer
					this->rxData = data + 1;
					*data = b;
				}
			}

			timer->CR1 = TIM_CR1_CEN; // restart timer
			break;
		case State::ERROR:
			timer->CR1 = TIM_CR1_CEN; // restart timer
			break;
		}
	}
}

void TIM16_IRQHandler() {
	busNode->timerIrqHandler();
}

void BusNodeImpl::timerIrqHandler() {
	const auto timer = TIM16;

	// clear interrupt flag
	timer->SR = ~TIM_SR_CC1IF;

	// stop and reset timer
	timer->CR1 = 0; // stop timer
	timer->EGR = TIM_EGR_UG; // reset timer

	switch (this->state) {
	case State::TRANSFER:
		// inform event loop if tx or rx operation has finished
		if (this->txState == TxState::END) {
			this->txState = TxState::FINISHED;
			triggerInterrupt(COMPLETE_IRQ);
		} else if (this->rxState == RxState::ACTIVE) {
			this->rxState = RxState::FINISHED;
			triggerInterrupt(COMPLETE_IRQ);
		}

		// return to idle state
		this->state = State::IDLE;
		break;
	default:
		// timeout in IDLE, SYNC or ERROR state
		Debug::toggleGreenLed();

		// return to idle state
		this->state = State::IDLE;

		// request a transfer when tx is pending
		if (this->txState >= TxState::PENDING)
			startRequest();
		break;
	}
}

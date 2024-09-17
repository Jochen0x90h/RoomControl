#include "BusMasterImpl.hpp"
#include "gpio.hpp"
#include <util.hpp>
#include "Debug.hpp"


// debug
/*
#define initSignal() gpio::configureOutput(20)
#define setSignal(value) gpio::setOutput(20, value)
#define toggleSignal() gpio::toggleOutput(20)
*/
#define initSignal()
#define setSignal(value)
#define toggleSignal()


/*
	How to calculate the value for the BAUDRATE register: desired baudrate * 2^32 / 16000000 (https://devzone.nordicsemi.com/f/nordic-q-a/391/uart-baudrate-register-values)
	Dependencies:
	
	Config:
	
	Resources:
		NRF_UART0
		NRF_TIMER1: Break and rx timeout
		NRF_EGU1
			TRIGGERED[0]: Received a packet
			TRIGGERED[1]: Send operation has finished
			TRIGGERED[2]: Request to send by node
*/
constexpr int TIMER_CLOCK = 1000000; // 1MHz
constexpr int BAUD_RATE = 19200;

namespace {
BusMasterImpl *busMaster;
}

BusMasterImpl::BusMasterImpl(int rxPin, int txPin) : rxPin(rxPin), txPin(txPin) {
	const auto uart = NRF_UART0;
	const auto timer = NRF_TIMER1;
	const auto egu = NRF_EGU1;

	// set instance pointer
	busMaster = this;

	initSignal();

	// configure rx pin (pull-up) and tx pin (idle state is high)
	gpio::configureInput(rxPin);//, gpio::Pull::UP);
	gpio::setOutput(txPin, true);
	gpio::configureOutput(txPin);

	// init uart
	uart->PSEL.RXD = rxPin;
	uart->PSEL.TXD = txPin;
	uart->CONFIG = N(UART_CONFIG_STOP, One) | N(UART_CONFIG_PARITY, Excluded);
	uart->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud19200);
	uart->INTENSET = N(UART_INTENSET_RXDRDY, Set);// | N(UART_INTENSET_TXDRDY, Set);
	uart->ENABLE = N(UART_ENABLE_ENABLE, Enabled);
	enableInterrupt(UARTE0_UART0_IRQn);

	// start receiving requests from nodes
	uart->TASKS_STARTRX = TRIGGER; // -> EVENTS_RXRDY

	// init timer
	timer->MODE = N(TIMER_MODE_MODE, Timer);
	timer->BITMODE = N(TIMER_BITMODE_BITMODE, 32Bit);
	timer->PRESCALER = 4; // 1MHz
	timer->INTENSET = N(TIMER_INTENSET_COMPARE0, Set);
	enableInterrupt(TIMER1_IRQn);

	// init event generator
	egu->INTENSET =
		N(EGU_INTENSET_TRIGGERED0, Set)
		| N(EGU_INTENSET_TRIGGERED1, Set)
		| N(EGU_INTENSET_TRIGGERED2, Set);

	// add to list of handlers
	loop::handlers.add(*this);
}

Awaitable<BusMaster::ReceiveParameters> BusMasterImpl::receive(int &length, uint8_t *data) {
	// start receiving immediately if no rx is pending and bus is idle
	if (this->rxState == RxState::IDLE) {
		ReceiveParameters p{&length, data};
		startReceive(p);
	}

	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusMaster::SendParameters> BusMasterImpl::send(int length, const uint8_t *data) {
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

void BusMasterImpl::handle() {
	const auto egu = NRF_EGU1;

	if (isInterruptPending(SWI1_EGU1_IRQn)) {

		// check if tx or rx operation has finished
		if (egu->EVENTS_TRIGGERED[1]) {
			// send operation complete
			egu->EVENTS_TRIGGERED[1] = 0;
			this->txState = TxState::IDLE;

			// check if a next send operation is pending
			this->sendWaitlist.visitSecond([this](const SendParameters &p) {
				startSend(p);
			});

			// resume first waiting coroutine
			this->sendWaitlist.resumeFirst([](const SendParameters &p) {
				return true;
			});
		} else if (egu->EVENTS_TRIGGERED[0]) {
			// receive operation complete
			egu->EVENTS_TRIGGERED[0] = 0;
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

		if (egu->EVENTS_TRIGGERED[2]) {
			// a node requested to be read: Start break if we are in idle state (no new tx was started above)
			egu->EVENTS_TRIGGERED[2] = 0;
			if (this->state == State::IDLE)
				startBreak();
		}

		// clear pending interrupt flag at NVIC
		clearInterrupt(SWI1_EGU1_IRQn);
	}
}

void BusMasterImpl::startReceive(const ReceiveParameters &p) {
	this->rxState = RxState::PENDING;
	this->rxBegin = p.data;
	this->rxEnd = p.data + *p.length;

	// don't start yet, wait for send or request signal from node
}

void BusMasterImpl::startSend(const SendParameters &p) {
	this->txState = TxState::PENDING;
	this->txBegin = p.data;
	this->txEnd = p.data + p.length;
	startBreak();
}

void BusMasterImpl::startBreak() {
	const auto uart = NRF_UART0;
	const auto timer = NRF_TIMER1;

	// disconnect uart from tx and set high baud rate to force stop of rx/tx during break
	uart->TASKS_STOPRX = TRIGGER;
	uart->TASKS_STOPTX = TRIGGER;
	uart->ENABLE = 0;
	uart->INTENCLR = 0xffffffff;
	uart->PSEL.TXD = gpio::DISCONNECTED;
	uart->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud1M);
	uart->ENABLE = N(UART_ENABLE_ENABLE, Enabled);

	// generate break: 13 bit times, 677us
	gpio::setOutput(this->txPin, false);
	timer->TASKS_CLEAR = TRIGGER;
	timer->CC[0] = 677;
	timer->TASKS_START = TRIGGER;

	// set state
	this->state = State::BREAK;

	//Debug::toggleRedLed();
}


// interrupt handlers

void UARTE0_UART0_IRQHandler() {
	busMaster->uartIrqHandler();
}

void BusMasterImpl::uartIrqHandler() {
	const auto uart = NRF_UART0;
	const auto timer = NRF_TIMER1;
	const auto egu = NRF_EGU1;

	if (uart->EVENTS_RXDRDY) {
		uart->EVENTS_RXDRDY = 0;
		setSignal(false);

		// restart rx timeout
		timer->TASKS_CLEAR = TRIGGER;

		auto b = uart->RXD;
		switch (this->state) {
		case State::IDLE:
			// received a request signal from a node
			egu->TASKS_TRIGGER[2] = TRIGGER;
			break;
		case State::SYNC:
			// check if sync byte was received correctly
			//Debug::toggleRedLed();
			if (b != 0x55) {
				// error: sync byte not received correctly

				// repeat
				startBreak();

				//todo indicate bus error after 3-5 retries
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
					uart->TXD = b;
					this->txByte = b;
				} else {
					// tx is idle: stop transmit
					uart->TASKS_STOPTX = TRIGGER;
				}

				// now in transfer state
				this->state = State::TRANSFER;
			}
			break;
		case State::TRANSFER: {
			// received next byte
			//Debug::toggleGreenLed();

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
						uart->TXD = b;
						this->txByte = b;
					} else {
						// end: stop transmit and set tx state to success
						uart->TASKS_STOPTX = TRIGGER;
						this->txState = TxState::END;
					}
				} else {
					// error: stop transmit as probably a node won the bus arbitration
					uart->TASKS_STOPTX = TRIGGER;
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
			break;
		}
		default:
			// ignore when in other state
			break;
		}
	}

	// debug
	/*if (uart->EVENTS_TXDRDY) {
		uart->EVENTS_TXDRDY = 0;
		setSignal(true);
	}*/
}

void TIMER1_IRQHandler() {
	busMaster->timerIrqHandler();
}

void BusMasterImpl::timerIrqHandler() {
	const auto uart = NRF_UART0;
	const auto timer = NRF_TIMER1;
	const auto egu = NRF_EGU1;

	// clear interrupt flag
	timer->EVENTS_COMPARE[0] = 0;

	if (state == State::BREAK) {
		gpio::setOutput(this->txPin, true);

		// restart timer for pause (half bit)
		timer->CC[0] = TIMER_CLOCK / (BAUD_RATE * 2);
		timer->TASKS_CLEAR = TRIGGER;
		this->state = State::PAUSE;
	} else if (state == State::PAUSE) {
		// end of break signal
		//Debug::toggleBlueLed();
		gpio::setOutput(this->txPin, true);

		// reconfigure uart for transmission
		uart->ENABLE = 0;
		uart->EVENTS_RXDRDY = 0;
		uart->EVENTS_TXDRDY = 0;
		uart->PSEL.TXD = this->txPin;
		uart->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud19200);
		uart->INTENSET = N(UART_INTENSET_RXDRDY, Set);// | N(UART_INTENSET_TXDRDY, Set);
		uart->ENABLE = N(UART_ENABLE_ENABLE, Enabled);

		// start transfer with sync byte
		uart->TASKS_STARTRX = TRIGGER; // -> EVENTS_RXRDY
		uart->TASKS_STARTTX = TRIGGER; // -> EVENTS_TXRDY
		uart->TXD = 0x55;

		// restart timer for rx timeout (two characters = 20 bit)
		timer->CC[0] = (20 * TIMER_CLOCK) / BAUD_RATE;
		timer->TASKS_CLEAR = TRIGGER;

		// expect receiving the sync byte
		this->state = State::SYNC;
	} else {
		// receive timeout
		//Debug::toggleRedLed();

		// stop timer
		timer->TASKS_STOP = TRIGGER;

		// inform event loop if tx or rx operation has finished
		if (this->txState == TxState::END) {
			//Debug::toggleRedLed();
			this->txState = TxState::FINISHED;
			egu->TASKS_TRIGGER[1] = TRIGGER;
		} else if (this->rxState == RxState::ACTIVE) {
			this->rxState = RxState::FINISHED;
			egu->TASKS_TRIGGER[0] = TRIGGER;
		}

		// return to idle state
		this->state = State::IDLE;

		// now the next transfer will start either on send by the master or on request by a node
	}
}

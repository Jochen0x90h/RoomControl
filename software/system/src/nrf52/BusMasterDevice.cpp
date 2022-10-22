#include "BusMasterDevice.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include "gpio.hpp"
#include <util.hpp>


// debug
/*
#define initSignal() configureOutput(3)
#define setSignal(value) setOutput(3, value)
#define toggleSignal() toggleOutput(3)
*/
#define initSignal()
#define setSignal(value)
#define toggleSignal()

/*
	Dependencies:
	
	Config:
	
	Resources:
		NRF_UART0
		NRF_TIMER1
*/
namespace {
BusMasterDevice *busMaster[1];
}

BusMasterDevice::BusMasterDevice(int rxPin, int txPin) : txPin(txPin) {
	this->uart = NRF_UART0;
	this->timer =  NRF_TIMER1;
	busMaster[0] = this;
	initSignal();

	// init uart
	gpio::setOutput(txPin, true);
	gpio::configureOutput(txPin);
	configureInput(rxPin, gpio::Pull::UP);
	//NRF_UART0->PSEL.TXD = BUS_TX_PIN;
	NRF_UART0->PSEL.RXD = rxPin;
	NRF_UART0->CONFIG = N(UART_CONFIG_STOP, One) | N(UART_CONFIG_PARITY, Excluded);
	//NRF_UART0->INTENSET = N(UART_INTENSET_TXDRDY, Set) | N(UART_INTENSET_RXDRDY, Set);
	enableInterrupt(UARTE0_UART0_IRQn);

	// init timer
	NRF_TIMER1->MODE = N(TIMER_MODE_MODE, Timer);
	NRF_TIMER1->BITMODE = N(TIMER_BITMODE_BITMODE, 32Bit);
	NRF_TIMER1->PRESCALER = 4; // 1MHz
	NRF_TIMER1->CC[0] = 1250;
	NRF_TIMER1->INTENSET = N(TIMER_INTENSET_COMPARE0, Set);
	enableInterrupt(TIMER1_IRQn);

	// add to list of handlers
	Loop::handlers.add(*this);
}

Awaitable<BusMaster::ReceiveParameters> BusMasterDevice::receive(int &length, uint8_t *data) {
	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusMaster::SendParameters> BusMasterDevice::send(int length, uint8_t const *data) {
	return {this->sendWaitlist, length, data};
}

void BusMasterDevice::handle() {
	if (rxReady) {
		rxReady = false;
		this->onTransferred(this->rxIndex);
		setSignal(true);
	}
	if (requestReady) {
		requestReady = false;
		if (this->onRequest)
			this->onRequest(this->requestData[0]);//, BusMaster::requestIndex);
		this->requestIndex = 0;
	}
}

void BusMasterDevice::uartIrqHandler() {
	if (this->uart->EVENTS_TXDRDY) {
		this->uart->EVENTS_TXDRDY = 0;

		// check if end of send buffer
		int txIndex = this->txIndex;
		if (txIndex < this->txLength) {
			// send next byte
			this->uart->TXD = this->txData[txIndex];
			this->txIndex = txIndex + 1;
		} else {
			// stop transmit
			this->uart->TASKS_STOPTX = TRIGGER;
		}
	}
	if (this->uart->EVENTS_RXDRDY) {
		this->uart->EVENTS_RXDRDY = 0;
		toggleSignal();
		//setSignal(true);

		if (this->state == SYNC) {
			// check if sync byte was received correctly
			uint8_t sync = this->uart->RXD;
			if (sync != 0x55) {
				// error: sync byte not received correctly

				// indicate that rx buffer is ready (zero length to indicate error)
				this->rxReady = true;

				// stop timer
				this->timer->TASKS_STOP = TRIGGER;

				// now in request mode
				this->state = REQUEST;
			} else {
				// now receive data
				this->state = RX;
			}
		} else if (this->state == RX) {
			// check if rx buffer full
			int rxIndex = this->rxIndex;
			this->rxData[rxIndex] = this->uart->RXD;
			this->rxIndex = rxIndex + 1;
			if (this->rxIndex == this->rxLength) {
				// indicate that rx buffer is ready
				this->rxReady = true;

				// stop timer
				this->timer->TASKS_STOP = TRIGGER;

				// now in request mode
				this->state = REQUEST;
			}

			// restart rx timeout (unless stopped)
			this->timer->TASKS_CLEAR = TRIGGER;
		} else {
			// check if request buffer full
			int requestIndex = this->requestIndex;
			if (requestIndex < array::count(this->requestData)) {
				this->requestData[requestIndex] = this->uart->RXD;
				this->requestIndex = requestIndex + 1;
				if (requestIndex == array::count(this->requestData)) {
					// indicate that requests are ready
					this->requestReady = true;
				}
			}
		}

		//toggleSignal();
	}
}

void BusMasterDevice::timerIrqHandler() {
	if (this->timer->EVENTS_COMPARE[0]) {
		this->timer->EVENTS_COMPARE[0] = 0;

		if (state == BREAK) {
			gpio::setOutput(this->txPin, true);

			// reconfigure uart for transmission
			this->uart->ENABLE = 0;
			this->uart->EVENTS_RXDRDY = 0;
			this->uart->EVENTS_TXDRDY = 0;
			this->uart->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud19200);
			this->uart->PSEL.TXD = this->txPin;
			this->uart->INTENSET = N(UART_INTENSET_RXDRDY, Set) | N(UART_INTENSET_TXDRDY, Set);
			this->uart->ENABLE = N(UART_ENABLE_ENABLE, Enabled);

			// start transfer with sync byte
			this->uart->TASKS_STARTRX = TRIGGER; // -> EVENTS_RXRDY
			this->uart->TASKS_STARTTX = TRIGGER; // -> EVENTS_TXRDY
			this->uart->TXD = 0x55;

			// restart timer for rx timeout
			this->timer->TASKS_CLEAR = TRIGGER;
			this->timer->CC[0] = 1250;

			// expect receive of sync byte
			this->state = SYNC;
			setSignal(false);
		} else {
			//toggleSignal();

			// stop timer
			this->timer->TASKS_STOP = TRIGGER;

			// indicate that rx buffer is ready
			this->rxReady = true;

			// now in request mode
			this->state = REQUEST;

			//		toggleSignal();
			setSignal(true);
		}
	}
}

extern "C" {
void UARTE0_UART0_IRQHandler(void);
void TIMER1_IRQHandler(void);
}

void UARTE0_UART0_IRQHandler(void) {
	busMaster[0]->uartIrqHandler();
}

void TIMER1_IRQHandler(void) {
	busMaster[0]->timerIrqHandler();
}

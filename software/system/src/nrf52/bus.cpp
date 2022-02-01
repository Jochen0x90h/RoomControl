#include "../bus.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <util.hpp>
#include <functional>


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
	Resources:
	NRF_UART0,
	NRF_TIMER1
*/ 
namespace bus {

Waitlist<Parameters> transferWaitlist;
Waitlist<> requestWaitlist;

enum State {
	BREAK,
	SYNC,
	RX,
	REQUEST
};
State volatile state;

// transmit buffer
uint8_t const *volatile txData;
int volatile txLength;
int volatile txIndex;

// receive buffer
uint8_t *volatile rxData;
int volatile rxLength;
int volatile rxIndex;
bool rxReady;
std::function<void (int)> onTransferred;

// request buffer
uint8_t requestData[1];
int volatile requestIndex;
bool requestReady;
std::function<void (uint8_t)> onRequest;

// event loop handler chain
loop::Handler nextHandler;
void handle() {
	if (rxReady) {
		rxReady = false;
		bus::onTransferred(bus::rxIndex);
		setSignal(true);
	}
	if (requestReady) {
		requestReady = false;
		if (bus::onRequest)
			bus::onRequest(bus::requestData[0]);//, bus::requestIndex);
		bus::requestIndex = 0;
	}
	
	// call next handler in chain
	bus::nextHandler();	
}

void init() {
	initSignal();
	
	// init uart
	setOutput(BUS_TX_PIN, true);
	configureOutput(BUS_TX_PIN);
	configureInputWithPullUp(BUS_RX_PIN);
	//NRF_UART0->PSEL.TXD = BUS_TX_PIN;
	NRF_UART0->PSEL.RXD = BUS_RX_PIN;
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

	// add to event loop handler chain
	bus::nextHandler = loop::addHandler(handle);
}

extern "C" {
	void UARTE0_UART0_IRQHandler(void);
	void TIMER1_IRQHandler(void);
}

void UARTE0_UART0_IRQHandler(void) {
	if (NRF_UART0->EVENTS_TXDRDY) {
		NRF_UART0->EVENTS_TXDRDY = 0;

		// check if end of send buffer
		int txIndex = bus::txIndex;
		if (txIndex < bus::txLength) {
			// send next byte
			NRF_UART0->TXD = bus::txData[txIndex];
			bus::txIndex = txIndex + 1;
		} else {
			// stop transmit
			NRF_UART0->TASKS_STOPTX = Trigger;
		}			
	}
	if (NRF_UART0->EVENTS_RXDRDY) {
		NRF_UART0->EVENTS_RXDRDY = 0;
		toggleSignal();
		//setSignal(true);

		if (bus::state == SYNC) {
			// check if sync byte was received correctly
			uint8_t sync = NRF_UART0->RXD;
			if (sync != 0x55) {
				// error: sync byte not received correctly

				// indicate that rx buffer is ready (zero length to indicate error)
				bus::rxReady = true;

				// stop timer
				NRF_TIMER1->TASKS_STOP = Trigger;

				// now in request mode
				bus::state = REQUEST;
			} else {
				// now receive data
				bus::state = RX;
			}
		} else if (bus::state == RX) {
			// check if rx buffer full
			int rxIndex = bus::rxIndex;
			bus::rxData[rxIndex] = NRF_UART0->RXD;
			bus::rxIndex = rxIndex + 1;
			if (bus::rxIndex == bus::rxLength) {
				// indicate that rx buffer is ready
				bus::rxReady = true;

				// stop timer
				NRF_TIMER1->TASKS_STOP = Trigger;

				// now in request mode
				bus::state = REQUEST;
			}
			
			// restart rx timeout (unless stopped)
			NRF_TIMER1->TASKS_CLEAR = Trigger;
		} else {
			// check if request buffer full
			int requestIndex = bus::requestIndex;
			if (requestIndex < array::count(bus::requestData)) {
				bus::requestData[requestIndex] = NRF_UART0->RXD;
				bus::requestIndex = requestIndex + 1;
				if (requestIndex == array::count(bus::requestData)) {
					// indicate that requests are ready
					bus::requestReady = true;
				}
			}				
		}

		//toggleSignal();
	}
}

void TIMER1_IRQHandler(void) {
	if (NRF_TIMER1->EVENTS_COMPARE[0]) {
		NRF_TIMER1->EVENTS_COMPARE[0] = 0;

		if (state == BREAK) {
			setOutput(BUS_TX_PIN, true);
			
			// reconfigure uart for transmission
			NRF_UART0->ENABLE = 0;
			NRF_UART0->EVENTS_RXDRDY = 0;
			NRF_UART0->EVENTS_TXDRDY = 0;
			NRF_UART0->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud19200);
			NRF_UART0->PSEL.TXD = BUS_TX_PIN;
			NRF_UART0->INTENSET = N(UART_INTENSET_RXDRDY, Set) | N(UART_INTENSET_TXDRDY, Set);
			NRF_UART0->ENABLE = N(UART_ENABLE_ENABLE, Enabled);
			
			// start transfer with sync byte
			NRF_UART0->TASKS_STARTRX = Trigger; // -> EVENTS_RXRDY		
			NRF_UART0->TASKS_STARTTX = Trigger; // -> EVENTS_TXRDY
			NRF_UART0->TXD = 0x55;

			// restart timer for rx timeout
			NRF_TIMER1->TASKS_CLEAR = Trigger;
			NRF_TIMER1->CC[0] = 1250;
	
			// expect receive of sync byte
			bus::state = SYNC;
			setSignal(false);
		} else {
			//toggleSignal();
					
			// stop timer
			NRF_TIMER1->TASKS_STOP = Trigger;
	
			// indicate that rx buffer is ready
			bus::rxReady = true;
	
			// now in request mode
			bus::state = REQUEST;
	
	//		toggleSignal();
			setSignal(true);
		}
	}
}
/*
void transfer(uint8_t const *txData, int txLength, uint8_t *rxData, int rxLength,
	std::function<void (int)> const &onTransferred)
{
	bus::onTransferred = onTransferred;

	// reset timer
	NRF_TIMER1->TASKS_STOP = Trigger;
	NRF_TIMER1->TASKS_CLEAR = Trigger;

	// disconnect uart from tx and set high baud rate to force stop of rx/tx during break
	NRF_UART0->TASKS_STOPRX = Trigger;
	NRF_UART0->TASKS_STOPTX = Trigger;
	NRF_UART0->ENABLE = 0;
	NRF_UART0->INTENCLR = 0xffffffff;
	NRF_UART0->PSEL.TXD = Disconnected;
	NRF_UART0->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud1M);
	NRF_UART0->ENABLE = N(UART_ENABLE_ENABLE, Enabled);

	// transmit buffer
	bus::txData = txData;
	bus::txLength = txLength;
	bus::txIndex = 0;

	// receive buffer
	bus::rxData = rxData;
	bus::rxLength = rxLength;
	bus::rxIndex = 0;
	bus::rxReady = false;

	// generate break: 13 bit times, 677us
	setOutput(BUS_TX_PIN, false);
	NRF_TIMER1->CC[0] = 677;
	NRF_TIMER1->TASKS_START = Trigger;
	
	bus::state = BREAK;
}

void setRequestHandler(std::function<void (uint8_t)> const &onRequest) {
	bus::onRequest = onRequest;
}
*/

Awaitable<Parameters> transfer(int writeLength, uint8_t const *writeData, int &readLength, uint8_t *readData) {
	
	// start transfer immediately if bus is idle
	// todo
	
	return {bus::transferWaitlist, writeLength, writeData, readLength, readData};
}

Awaitable<> request() {
	return {bus::requestWaitlist};
}

} // namespace bus

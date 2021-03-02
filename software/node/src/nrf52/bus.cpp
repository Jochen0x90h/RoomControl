#include "../bus.hpp"
#include "global.hpp"
#include <util.hpp>


constexpr int SIGNAL_PIN = 3;


namespace bus {

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
std::function<void (int)> onRx = [](int) {};

// request buffer
uint8_t requestData[1];
int volatile requestIndex;
bool requestReady;
std::function<void (uint8_t)> onRequest = [](uint8_t) {};


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
		toggleOutput(SIGNAL_PIN);
		//setOutput(SIGNAL_PIN, true);

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
			if (requestIndex < array::size(bus::requestData)) {
				bus::requestData[requestIndex] = NRF_UART0->RXD;
				bus::requestIndex = requestIndex + 1;			
				if (requestIndex == array::size(bus::requestData)) {
					// indicate that requests are ready
					bus::requestReady = true;
				}
			}				
		}

		//toggleOutput(SIGNAL_PIN);
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
			setOutput(SIGNAL_PIN, false);			
		} else {
			//toggleOutput(SIGNAL_PIN);
					
			// stop timer
			NRF_TIMER1->TASKS_STOP = Trigger;
	
			// indicate that rx buffer is ready
			bus::rxReady = true;
	
			// now in request mode
			bus::state = REQUEST;
	
	//		toggleOutput(SIGNAL_PIN);
			setOutput(SIGNAL_PIN, true);
		}
	}
}

void init() {
	configureOutput(SIGNAL_PIN);
	
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
}

void handle() {
	if (rxReady) {
		rxReady = false;
		bus::onRx(bus::rxIndex);
		setOutput(SIGNAL_PIN, true);
	}
	if (requestReady) {
		requestReady = false;
		bus::onRequest(bus::requestData[0]);//, bus::requestIndex);
		bus::requestIndex = 0;
	}
}

void transfer(uint8_t const *txData, int txLength, uint8_t *rxData, int rxLength,
	std::function<void (int)> const &onRx)
{
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
	bus::onRx = onRx;

	// generate break: 13 bit times, 677us
	setOutput(BUS_TX_PIN, false);
	NRF_TIMER1->CC[0] = 677;
	NRF_TIMER1->TASKS_START = Trigger;
	
	bus::state = BREAK;
}

void setRequestHandler(std::function<void (uint8_t)> const &onRequest) {
	bus::onRequest = onRequest;
}

} // namespace radio

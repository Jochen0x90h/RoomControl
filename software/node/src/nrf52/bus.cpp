#include "bus.hpp"
#include "util.hpp"


namespace bus {

std::function<void ()> onBusTransferred;
std::function<void ()> onBusWakeup = nop;

void init() {
	
	setOutput(BUS_TX_PIN, true);
	configureOutput(BUS_TX_PIN);
	configureInputWithPullUp(BUS_RX_PIN);
	
	// UART0 and UARTE0 are the same device
	NRF_UART0->PSEL.TXD = BUS_TX_PIN;
	NRF_UART0->PSEL.RXD = BUS_RX_PIN;
	NRF_UART0->CONFIG = N(UART_CONFIG_STOP, One) | N(UART_CONFIG_PARITY, Excluded);

}

void handle() {

	if (NRF_UART0->EVENTS_TXDRDY) {
		NRF_UART0->INTENCLR = N(UART_INTENCLR_TXDRDY, Clear);

		// clear pending interrupt flags at peripheral and NVIC
		NRF_UART0->EVENTS_TXDRDY = 0;
		clearInterrupt(UARTE0_UART0_IRQn);
		
		// disable uart
		NRF_UART0->ENABLE = 0;
		
		// now transfer the data using dma
		NRF_UARTE0->BAUDRATE = N(UARTE_BAUDRATE_BAUDRATE, Baud19200);
		NRF_UARTE0->INTENSET = N(UARTE_INTENSET_ENDTX, Set);
		NRF_UARTE0->ENABLE = N(UARTE_ENABLE_ENABLE, Enabled);
		NRF_UARTE0->TASKS_STARTTX = Trigger;		
	}
	if (NRF_UARTE0->EVENTS_ENDTX) {
		NRF_UARTE0->INTENCLR = N(UARTE_INTENCLR_ENDTX, Clear);
		
		// clear pending interrupt flags at peripheral and NVIC
		NRF_UARTE0->EVENTS_ENDTX = 0;
		clearInterrupt(UARTE0_UART0_IRQn);

		// disable uart
		NRF_UART0->ENABLE = 0;	

		onBusTransferred();
	}
}

void transferBus(uint8_t const *sendData, int sendLength, uint8_t *receiveData, int receiveLength,
	std::function<void ()> onTransferred)
{
	onBusTransferred = onTransferred;
	
	// set data
	NRF_UARTE0->TXD.PTR = intptr_t(sendData);
	NRF_UARTE0->TXD.MAXCNT = sendLength;

	// generate break: 13 bit times, 677us
	NRF_UART0->BAUDRATE = 0x00300000;
	NRF_UART0->INTENSET = N(UART_INTENSET_TXDRDY, Set);
	NRF_UART0->ENABLE = N(UART_ENABLE_ENABLE, Enabled); // without dma
	NRF_UART0->TASKS_STARTTX = Trigger;
	NRF_UART0->TXD = 0x80;
}

void setWakeupHandler(std::function<void ()> onWakeup) {
	onBusWakeup = onWakeup;
}

} // namespace radio

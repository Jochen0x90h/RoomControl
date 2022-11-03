#pragma once

#include "../BusMaster.hpp"
#include "Loop.hpp"
#include "system/nrf.h"
#include <functional>


class BusMasterImpl : public BusMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	BusMasterImpl(int rxPin, int txPin);

	Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle() override;

	void uartIrqHandler();
	void timerIrqHandler();

protected:

	int txPin;
	NRF_UART_Type *uart;
	NRF_TIMER_Type *timer;

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


	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};

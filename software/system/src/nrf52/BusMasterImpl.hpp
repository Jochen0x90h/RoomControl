#pragma once

#include "../BusMaster.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"
#include <functional>


extern "C" {
void UARTE0_UART0_IRQHandler();
void TIMER1_IRQHandler();
}

class BusMasterImpl : public BusMaster, public loop::Handler2 {
	friend void UARTE0_UART0_IRQHandler();
	friend void TIMER1_IRQHandler();
public:
	/**
	 * Constructor
	 * @param rxPin receive pin from LIN driver
	 * @param txPin transmit pin to LIN driver
	 */
	BusMasterImpl(int rxPin, int txPin);

	Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle() override;

protected:
	void startReceive(const ReceiveParameters &p);
	void startSend(const SendParameters &p);
	void startBreak();

	void uartIrqHandler();
	void timerIrqHandler();

	int rxPin;
	int txPin;

	// state
	enum class State : uint8_t {
		// idle, waiting for receive/send by application or a request from a node
		IDLE,

		// sending break signal
		BREAK,
		PAUSE,

		// sending sync byte
		SYNC,

		// receive until timeout or send until last character
		TRANSFER,

		// finished, waiting for handling in event loop
		FINISHED,
	};
	volatile State state = State::IDLE;

	// rx
	enum class RxState : uint8_t {
		IDLE,
		FINISHED,
		PENDING,
		ACTIVE
	};
	volatile RxState rxState = RxState::IDLE;
	uint8_t *rxBegin;
	uint8_t *rxEnd;
	uint8_t *rxData;

	// tx
	enum class TxState : uint8_t {
		IDLE,
		FINISHED,
		PENDING,
		ACTIVE,
		END
	};
	volatile TxState txState = TxState::IDLE;
	const uint8_t *txBegin;
	const uint8_t *txEnd;
	const uint8_t *txData;
	uint8_t txByte;

	// lists for coroutines waiting for receive or send to complete
	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};

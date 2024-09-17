#pragma once

#include "../BusNode.hpp"
#include "Loop.hpp"


extern "C" {
void USART1_IRQHandler();
void TIM16_IRQHandler();
}

class BusNodeImpl : public BusNode, public loop::Handler2 {
	friend void USART1_IRQHandler();
	friend void TIM16_IRQHandler();
public:
	/**
	 * Constructor
	 * @param rxPin receive pin from LIN driver
	 * @param txPin transmit pin to LIN driver
	 */
	BusNodeImpl(int rxPin, int txPin);

	[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle() override;

protected:
	void startReceive(const ReceiveParameters &p);
	void startSend(const SendParameters &p);
	void startRequest();

	void usartIrqHandler();
	void timerIrqHandler();

	int rxPin;

	// state
	enum class State : uint8_t {
		// idle, waiting for receive/send by master and sending a request from time to time if send is pending
		IDLE,

		// receiving sync byte
		SYNC,

		// receive until timeout or send until last character
		TRANSFER,

		// error when receiving sync byte
		ERROR
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

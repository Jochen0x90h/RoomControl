#include "../Radio.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"
#include "Random.hpp"
#include "../Debug.hpp"
#include <ieee.hpp>
#include <assert.hpp>
#include <Queue.hpp>


// disable CSMA/CA (Carrier Sense Multiple Access/Collision Avoidance) for jamming tests
//#define NO_CSMA_CA

/*
	Dependencies:
		Random

	Config:
		RADIO_CONTEXT_COUNT: Number of contexts (virtual radios)
		RADIO_RECEIVE_QUEUE_LENGTH: length of queue for received messages
		RADIO_MAX_PAYLOAD_LENGTH: maximum length of payload

	Resources:
		NRF_RADIO
		NRF_TIMER0
			CC[0]: ack turnaround timeout, minimum time between received packet and sending ack
			CC[1]: ack wait timeout, maximum time to wait for receiving ack after sending a packet
			CC[2]: time of last packet received or sent
			CC[3]: backoff timeout
		NRF_PPI
			CH[27]: RADIO->EVENTS_END -> TIMER0->TASKS_CAPTURE[2]
		NRF_EGU0
			TRIGGERED[0]: energy detection
			TRIGGERED[1]: received a packet
			TRIGGERED[2]: send operation has finished

	Glossary:
		CCA: Clear Channel Assessment (-> ED and/or carrier detection)
		ED: Energy Detection on a radio channel
		RFD: Reduced-function defice (can only talk to -> FFDs)
		FFD: Full-function device
		MLME: MAC Layer Management Entity, management service of the MAC layer
		MCPS: MAC Common Part Layer, data transport service of the MAC layer
		PIB:  PAN Information Base

	References:
		https://www.prismmodelchecker.org/casestudies/zigbee.php

	Bugs:
 		No timeout for send() with SendFlags::AWAIT_DATA_REQUEST, i.e. waits forever
*/
namespace Radio {

// symbol duration: 16us
constexpr int symbolDuration = 16;

// turnaround time from receive to send and vice versa (e.g. send ack after receive)
constexpr int ackTurnaroundDuration = 12 * symbolDuration;

// maximum time to wait for an ack
constexpr int ackWaitDuration = 54 * symbolDuration;


// radio state
// -----------

// radio is active (start() was called)
bool active;

// radio is in Disabled state
inline bool isDisabled() {
	return NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled;
}

// radio is in RxIdle state
inline bool isRxIdle() {
	return NRF_RADIO->STATE == RADIO_STATE_STATE_RxIdle;
}

// radio is in RxIdle or Rx state
inline bool isReceiveState() {
	return (NRF_RADIO->STATE | 1) == 3;
}

// action to take on END event
enum class EndAction {
	NOP,
	RECEIVE,
	SEND_ACK,
	//SEND_REQUESTED,
	ON_SENT
};
EndAction endAction;


// energy detection
// ----------------

std::function<void (uint8_t)> onEdReady;


// context
// -------

uint64_t longAddress;

struct Context {
	ContextFlags volatile flags;
	uint16_t volatile pan;
	uint16_t volatile shortAddress;

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;


	bool filter(uint8_t const *data) const {
		uint8_t const *mac = data + 1;
		ContextFlags flags = this->flags;

		// ALL: all packets pass
		if ((flags & ContextFlags::PASS_ALL) != 0)
			return true;

		// get frame control field
		auto frameControl = ieee::FrameControl(mac[0] | (mac[1] << 8));
		auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;

		// reject frames with no sequence number
		if ((frameControl & ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION) != 0)
			return false;

		// beacon packets
		if ((flags & ContextFlags::PASS_TYPE_BEACON) != 0 && frameType == ieee::FrameControl::TYPE_BEACON)
			return true;

		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_FLAG) != 0) {
			// has destination address: check pan
			uint16_t pan = mac[3] | (mac[4] << 8);
			if (pan != 0xffff && pan != this->pan)
				return false;

			if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
				// short destination addressing mode
				if ((flags & ContextFlags::PASS_DEST_SHORT) != 0
					|| ((flags & ContextFlags::PASS_TYPE_DATA_DEST_SHORT) != 0 && frameType == ieee::FrameControl::TYPE_DATA))
				{
					uint16_t shortAddress = mac[5] | (mac[6] << 8);
					if (shortAddress == 0xffff || shortAddress == this->shortAddress)
						return true;
				}
			} else {
				// long destination addressing mode
				if ((flags & ContextFlags::PASS_DEST_LONG) != 0) {
					uint32_t longAddressLo = mac[5] | (mac[6] << 8) | (mac[7] << 16) | (mac[8] << 24);
					uint32_t longAddressHi = mac[9] | (mac[10] << 8) | (mac[11] << 16) | (mac[12] << 24);
					if (longAddressLo == uint32_t(Radio::longAddress) && longAddressHi == uint32_t(Radio::longAddress >> 32))
						return true;
				}
			}
		}

		return false;
	}

	// check if a packet for given pan id and destination address is pending (called on receive of data request)
	bool isPending(uint16_t panId, int addressLength, uint8_t const *destinationAddress) {
		return this->sendWaitlist.contains([panId, addressLength, destinationAddress](SendParameters &p) {
			// check pan id
			if (panId != (p.packet[4] | (p.packet[5] << 8)))
				return false;

			// frame control
			auto frameControl = ieee::FrameControl(p.packet[1] | (p.packet[2] << 8))
				& (ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION | ieee::FrameControl::DESTINATION_ADDRESSING_MASK);

			// check destination addressing
			if (addressLength == 2) {
				if (frameControl != ieee::FrameControl::DESTINATION_ADDRESSING_SHORT)
					return false;
			} else {
				if (frameControl != ieee::FrameControl::DESTINATION_ADDRESSING_LONG)
					return false;
			}

			// check destination address
			if (array::equals(addressLength, destinationAddress, p.packet + 6)) {
				// clear AWAIT_DATA_REQUEST flag in byte behind packet
				int length = p.packet[0] - 2;
				p.packet[1 + length] &= uint8_t(~SendFlags::AWAIT_DATA_REQUEST);
				return true;
			}
			return false;
		});
	}

};

Context contexts[RADIO_CONTEXT_COUNT];


// receiver
// --------

// receiver state (base band decoder)
bool volatile receiverEnabled = false;

// receive queue
struct Receive {
	// receive packet
	Packet packet;

	// for each context a flag that indicates if the packet gets passed to the context
	uint8_t passFlags;
};
volatile Queue<Receive, RADIO_RECEIVE_QUEUE_LENGTH> receiveQueue;


// start receiving a packet
inline void startReceive() {
	Radio::endAction = EndAction::RECEIVE;

	// overwrite last queue element if the queue is full
	if (Radio::receiveQueue.isFull()) {
		Radio::receiveQueue.removeBack();
	}

	Receive &receive = Radio::receiveQueue.getNextBack();
	NRF_RADIO->PACKETPTR = intptr_t(receive.packet);
	NRF_RADIO->TASKS_START = TRIGGER; // -> END
}


// sender
// ------

// state
enum SendState : uint8_t {
	IDLE,

	// wait until send operation is done
	AWAIT_SENT,

	// wait for send and then ACK
	AWAIT_SENT_ACK,

	// wait for an ACK from the destination
	AWAIT_ACK,
};
int sendIndex;
SendState sendState = SendState::IDLE;
Packet sendPacket;
uint8_t *sendResult;

// superframe
constexpr int baseSlotDuration = 60 * symbolDuration;
constexpr int baseSuperframeDuration = 16 * baseSlotDuration;

// inter frame spacing
constexpr int maxSifsLength = 18;
constexpr int minSifsDuration = 12 * symbolDuration;
constexpr int minLifsDuration = 40 * symbolDuration;
uint32_t ifsDuration;

// backoff
constexpr int minBackoffExponent = 3;
constexpr int maxBackoffExponent = 5;
constexpr int maxBackoffCount = 3;
constexpr int unitBackoffDuration = 20 * symbolDuration;
int backoffExponent;
int backoffCount;

// ack
constexpr int maxAckRetryCount = 3;
int ackRetryCount;

static void prepareForSend(int index, ContextFlags flags, uint8_t const *packet, uint8_t &result);
static void startBackoff();
static void backoff();
static void setSendResult(uint8_t result);

// select a packet for the next send operation
static void selectForSend() {
	int index = Radio::sendIndex;
	do {
		// select next context
		index = index < RADIO_CONTEXT_COUNT - 1 ? index + 1 : 0;
		auto &context = Radio::contexts[index];
		auto flags = context.flags;

		// check if a send operation can be started for this context
		if (context.sendWaitlist.contains([index, flags](SendParameters &p) {
			if (p.result != 255)
				return false;
			int length = p.packet[0] - 2;
			auto sendFlags = SendFlags(p.packet[1 + length]);
			if ((sendFlags & SendFlags::AWAIT_DATA_REQUEST) == 0) {
				prepareForSend(index, flags, p.packet, p.result);
				return true;
			}
			return false;
		}))
		{
			break;
		}
	} while (index != Radio::sendIndex);
}

static void prepareForSend(int index, ContextFlags flags, uint8_t const *packet, uint8_t &result) {
	// set context plugIndex that initiated the send operation
	Radio::sendIndex = index;

	// check if the packet requests an ACK
	bool requestAck = (ieee::FrameControl(packet[1]) & ieee::FrameControl::ACKNOWLEDGE_REQUEST) != 0;

	// check if the context is configured for handling ACK
	bool handleAck = (flags & ContextFlags::HANDLE_ACK) != 0;

	// either wait until packet is sent or until ack was received
	Radio::sendState = (requestAck && handleAck) ? SendState::AWAIT_SENT_ACK : SendState::AWAIT_SENT;

	// copy send packet
	int length = packet[0] - 2;
	array::copy(1 + length, Radio::sendPacket, packet);

	// set pointer to send result
	Radio::sendResult = &result;

	// reset ack retry count
	Radio::ackRetryCount = 1;

	// start backoff for csma-ca
#ifndef NO_CSMA_CA
	startBackoff();
#else
	NRF_RADIO->TASKS_STOP = TRIGGER;
	NRF_RADIO->TASKS_TXEN = TRIGGER; // -> TXREADY
#endif
}

// start backoff for csma-ca
static void startBackoff() {
	// initialize backoff parameters
	Radio::backoffExponent = Radio::minBackoffExponent;
	Radio::backoffCount = 0;

	// first backoff for csma-ca
	backoff();
}

// repeat backoff for csma-ca
static void backoff() {
	// fail when maximum backoff count is reached
	if (Radio::backoffCount >= Radio::maxBackoffCount) {
		setSendResult(0);

		// check if more to send
		selectForSend();

		return;
	}

	// throw the dice to determine backoff period
	int backoff = (Random::u8() & ~(0xffffffff << Radio::backoffExponent)) + 1;
	int backoffDuration = backoff * unitBackoffDuration;

	// update backoff parameters
	Radio::backoffExponent = min(Radio::backoffExponent + 1, Radio::maxBackoffExponent);
	++Radio::backoffCount;

	// set timer for backoff duration
	NRF_TIMER0->TASKS_CAPTURE[3] = TRIGGER;
	NRF_TIMER0->CC[3] = NRF_TIMER0->CC[3] + backoffDuration;
	NRF_TIMER0->EVENTS_COMPARE[3] = 0;
	NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE3, Set); // -> COMPARE[3]
}

// start clear channel assessment
static void startClearChannelAssessment() {
	// shortcut: stop receiving and enable sender if channel is clear
	NRF_RADIO->SHORTS = N(RADIO_SHORTS_CCAIDLE_STOP, Enabled)
		| N(RADIO_SHORTS_CCAIDLE_TXEN, Enabled);

	// start clear channel assessment
	NRF_RADIO->TASKS_CCASTART = TRIGGER; // -> CCABUSY or CCAIDLE -> TXEN -> TXREADY
}


// start to send a packet
static void startSend() {
	// handle sent packet on END event
	Radio::endAction = EndAction::ON_SENT;

	// start send operation using a copy of the packet
	NRF_RADIO->PACKETPTR = intptr_t(Radio::sendPacket);
	NRF_RADIO->TASKS_START = TRIGGER; // -> END
}

// set result of send operation
static void setSendResult(uint8_t result) {
	// sender is idle again
	Radio::sendState = SendState::IDLE;

	// set result unless send operation was cancelled
	if (Radio::sendResult != nullptr) {
		*Radio::sendResult = result;
		Radio::sendResult = nullptr;

		// signal to handle() that a send operation has finished
		NRF_EGU0->TASKS_TRIGGER[2] = TRIGGER;
	}
}

// ack packet: length (including crc), frame control, mac counter (ackPacket[3])
uint8_t ackPacket[] = {5, 0x02, 0x00, 0};

// start to send an ack packet
static void startSendAck() {
	// don't do anything on END event
	Radio::endAction = EndAction::NOP;

	NRF_RADIO->PACKETPTR = intptr_t(Radio::ackPacket);
	NRF_RADIO->TASKS_START = TRIGGER; // -> END
}


static inline void lock() {
	disableInterrupt(RADIO_IRQn);
	disableInterrupt(TIMER0_IRQn);
}

static inline void unlock() {
	enableInterrupt(RADIO_IRQn);
	enableInterrupt(TIMER0_IRQn);
}

/*
ReceiveParameters::ReceiveParameters(ReceiveParameters &&p) noexcept
	: WaitlistElement((lock(), std::move(p))), packet(p.packet)
{
	unlock();
}

void ReceiveParameters::add(WaitlistHead &head) noexcept {
	lock();
	WaitlistElement::add(head);
	unlock();
}

void ReceiveParameters::remove() noexcept {
	lock();
	WaitlistElement::remove();
	unlock();
}


SendParameters::SendParameters(SendParameters &&p) noexcept
	: WaitlistElement((lock(), std::move(p))), packet(p.packet), result(p.result)
{
	unlock();
}

void SendParameters::add(WaitlistHead &head) noexcept {
	lock();
	WaitlistElement::add(head);
	unlock();
}

void SendParameters::remove() noexcept {
	lock();
	WaitlistElement::remove();

	// cancel if this is the current send operation
	if (Radio::sendResult == &this->result) {
		// disable timer interrupt 3 to stop backoff
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE3, Clear);

		// stop clear channel assessment
		NRF_RADIO->TASKS_CCASTOP = TRIGGER; // -> CCASTOPPED

		// clear result pointer
		Radio::sendResult = nullptr;

		// check if no transmit operation is ongoing
		if (isReceiveState()) {
			// return to idle state
			Radio::sendState = SendState::IDLE;

			// check if more to send
			selectForSend();
		}
	}

	unlock();
}
*/

void ReceiveParameters::append(WaitlistNode &list) noexcept {
	lock();
	list.add(*this);
	unlock();
}

void ReceiveParameters::cancel() noexcept {
	lock();
	remove();
	unlock();
}

void SendParameters::append(WaitlistNode &list) noexcept {
	lock();
	list.add(*this);
	unlock();
}

void SendParameters::cancel() noexcept {
	lock();
	remove();

	// cancel if this is the current send operation
	if (Radio::sendResult == &this->result) {
		// disable timer interrupt 3 to stop backoff
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE3, Clear);

		// stop clear channel assessment
		NRF_RADIO->TASKS_CCASTOP = TRIGGER; // -> CCASTOPPED

		// clear result pointer
		Radio::sendResult = nullptr;

		// check if no transmit operation is ongoing
		if (isReceiveState()) {
			// return to idle state
			Radio::sendState = SendState::IDLE;

			// check if more to send
			selectForSend();
		}
	}

	unlock();
}



// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (isInterruptPending(SWI0_EGU0_IRQn)) {
		// check energy detection
		if (NRF_EGU0->EVENTS_TRIGGERED[0]) {
			NRF_EGU0->EVENTS_TRIGGERED[0] = 0;
			Radio::onEdReady(NRF_RADIO->EDSAMPLE);
		}

		// check if a send operation has finished
		if (NRF_EGU0->EVENTS_TRIGGERED[2]) {
			NRF_EGU0->EVENTS_TRIGGERED[2] = 0;

			// resume coroutines for finished send operations
			for (auto &c : Radio::contexts) {
				c.sendWaitlist.resumeAll([](SendParameters &p) {
					// check if the send operation has finished
					return p.result != 255;
				});
			}
		}

		// check if a packet was received
		if (NRF_EGU0->EVENTS_TRIGGERED[1]) {
			NRF_EGU0->EVENTS_TRIGGERED[1] = 0;
			do {
				Receive &receive = Radio::receiveQueue.getFront();

				// resume coroutines
				uint8_t passFlag = 1;
				for (auto &c : Radio::contexts) {
					if ((receive.passFlags & passFlag) != 0) {
						// resume first waiting coroutine
						c.receiveWaitlist.resumeFirst([&receive](ReceiveParameters &p) {
							// length without crc but with extra data
							int length = receive.packet[0] - 2 + Radio::RECEIVE_EXTRA_LENGTH;
							array::copy(1 + length, p.packet, receive.packet);
							return true;
						});
					}
					passFlag <<= 1;
				}

				// remove element from queue and restart receive if the buffer was full
				{
					lock();
					//bool full = Radio::receiveQueue.isFull();

					// remove entry from receive queue
					Radio::receiveQueue.removeFront();

					// continue receiving if the queue has now space again
					//if (full) {
					//	startReceive(); // -> END
					//}
					unlock();
				}
			} while (!Radio::receiveQueue.isEmpty());
		}

		// clear pending interrupt flag at NVIC
		clearInterrupt(SWI0_EGU0_IRQn);
	}

	// call next handler in chain
	Radio::nextHandler();
}

void init() {
	// check if already initialized
	if (Radio::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Radio::nextHandler = loop::addHandler(handle);

	// init random number generator
	Random::init();

	// init timer
	NRF_TIMER0->MODE = N(TIMER_MODE_MODE, Timer);
	NRF_TIMER0->BITMODE = N(TIMER_BITMODE_BITMODE, 32Bit);
	NRF_TIMER0->PRESCALER = 4; // 1MHz
	enableInterrupt(TIMER0_IRQn);

	// init radio

	// set modulation mode
	NRF_RADIO->MODE = N(RADIO_MODE_MODE, Ieee802154_250Kbit);

	// configure crc
	NRF_RADIO->CRCCNF =
		N(RADIO_CRCCNF_SKIPADDR, Ieee802154) // CRC starts after first byte of length field
		| N(RADIO_CRCCNF_LEN, Two); // CRC-16
	NRF_RADIO->CRCPOLY = 0x11021;
	NRF_RADIO->CRCINIT = 0;

	// configure packet (PREAMBLE, ADDRESS, S0, LENGTH, S1, PAYLOAD, CRC)
	NRF_RADIO->PCNF0 =
		N(RADIO_PCNF0_PLEN, 32bitZero) // PREAMBLE is 32 bit zero
		| V(RADIO_PCNF0_S0LEN, 0) // no S0
		| N(RADIO_PCNF0_CRCINC, Include) // LENGTH includes CRC
		| V(RADIO_PCNF0_LFLEN, 8) // LENGTH is 8 bit
		| V(RADIO_PCNF0_S1LEN, 0); // no S1
	NRF_RADIO->PCNF1 =
		V(RADIO_PCNF1_MAXLEN, RADIO_MAX_PAYLOAD_LENGTH) // maximum length of PAYLOAD
		| N(RADIO_PCNF1_ENDIAN, Little); // little endian

	// configure clear channel assessment (CCA)
	NRF_RADIO->CCACTRL = N(RADIO_CCACTRL_CCAMODE, CarrierAndEdMode);

	// set interrupt flags and enable interrupts
	NRF_RADIO->INTENSET =
		N(RADIO_INTENSET_RXREADY, Set)
		| N(RADIO_INTENSET_EDEND, Set)
		| N(RADIO_INTENSET_CRCOK, Set)
		| N(RADIO_INTENSET_CRCERROR, Set)
		| N(RADIO_INTENSET_CCABUSY, Set)
		| N(RADIO_INTENSET_TXREADY, Set)
		| N(RADIO_INTENSET_END, Set)
		| N(RADIO_INTENSET_DISABLED, Set);
	enableInterrupt(RADIO_IRQn);

	// capture time when packet was received or sent (RADIO:END -> TIMER0:CAPTURE[2])
	NRF_PPI->CHENSET = 1 << 27;

	// init event generator
	NRF_EGU0->INTENSET =
		N(EGU_INTENSET_TRIGGERED0, Set)
		| N(EGU_INTENSET_TRIGGERED1, Set)
		| N(EGU_INTENSET_TRIGGERED2, Set);
}

extern "C" {
	void RADIO_IRQHandler(void);
	void TIMER0_IRQHandler(void);
}

void RADIO_IRQHandler(void) {
	// check if ready to receive (RxIdle state)
	if (NRF_RADIO->EVENTS_RXREADY) {
		NRF_RADIO->EVENTS_RXREADY = 0;

		// start receive
		if (Radio::receiverEnabled/* && !Radio::receiveQueue.isFull()*/)
			startReceive(); // -> END

		// check if more to send (this is executed after a send operation completes)
		if (Radio::sendState == SendState::IDLE) {
			selectForSend();
		}
	}

	// check if end of energy detection
	if (NRF_RADIO->EVENTS_EDEND) {
		NRF_RADIO->EVENTS_EDEND = 0;

		// signal end of energy detection to handle()
		NRF_EGU0->TASKS_TRIGGER[0] = TRIGGER;
	}

	// check if clear channel assessment failed
	if (NRF_RADIO->EVENTS_CCABUSY) {
		NRF_RADIO->EVENTS_CCABUSY = 0;

		// backoff and try again
		backoff();
	}

	// check if sender ready (TxIdle state)
	if (NRF_RADIO->EVENTS_TXREADY) {
		NRF_RADIO->EVENTS_TXREADY = 0;
		// shortcut: disable sender on END event
		NRF_RADIO->SHORTS = N(RADIO_SHORTS_END_DISABLE, Enabled);

		// check if clear channel assessment was running
#ifndef NO_CSMA_CA
		if (Radio::endAction == EndAction::SEND_ACK) {
			// send ACK overrides a normal send operation
			if (Radio::sendState != SendState::IDLE) {
				// backoff and try again
				backoff();
			}
		} else {
			// start send operation assuming that CCA was successful (CCAIDLE)
			startSend();
		}
#else
		startSend();
#endif
	}

	// check if packet was received with no crc error
	if (NRF_RADIO->EVENTS_CRCOK) {
		NRF_RADIO->EVENTS_CRCOK = 0;

		// get received packet
		Receive &receive = Radio::receiveQueue.getNextBack();
		uint8_t length = receive.packet[0];
		Radio::ifsDuration = length <= maxSifsLength ? minSifsDuration : minLifsDuration;

		// frame control
		auto frameControl = ieee::FrameControl(receive.packet[1] | (receive.packet[2] << 8));

		// check if a previously sent packet awaits an ACK
		if (Radio::sendState == SendState::AWAIT_ACK) {
			// check if this is the ACK packet that we are waiting for (mac counter must match)
			auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
			if (frameType == ieee::FrameControl::TYPE_ACK && receive.packet[3] == Radio::sendPacket[3]) {
				// disable ACK wait and backoff interrupts
				NRF_TIMER0->INTENCLR =
					N(TIMER_INTENCLR_COMPARE1, Clear)
					| N(TIMER_INTENCLR_COMPARE3, Clear);

				// set state of sent packet to success, send state becomes idle
				setSendResult(Radio::backoffCount);

				// check if more to send
				selectForSend();
			}
		}

		// check if a context is interested in this packet and wants to handle ack
		receive.passFlags = 0;
		uint8_t passFlag = 1;
		bool ack = false;
		Radio::ackPacket[1] = uint8_t(ieee::FrameControl::TYPE_ACK);
		for (auto &c : Radio::contexts) {
			if (c.flags != ContextFlags::NONE && c.filter(receive.packet)) {
				receive.passFlags |= passFlag;

				// check if this context handles ack and the packet requests an ack
				if ((c.flags & Radio::ContextFlags::HANDLE_ACK) != 0
					&& (frameControl & ieee::FrameControl::ACKNOWLEDGE_REQUEST) != 0)
				{
					// we need to send an ack
					ack = true;

					// check if this is a data request packet
					if ((frameControl & (ieee::FrameControl::TYPE_MASK
							| ieee::FrameControl::SECURITY
							| ieee::FrameControl::PAN_ID_COMPRESSION
							| ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION
							| ieee::FrameControl::DESTINATION_ADDRESSING_FLAG
							| ieee::FrameControl::SOURCE_ADDRESSING_FLAG))
						== (ieee::FrameControl::TYPE_COMMAND
							| ieee::FrameControl::PAN_ID_COMPRESSION
							| ieee::FrameControl::DESTINATION_ADDRESSING_FLAG
							| ieee::FrameControl::SOURCE_ADDRESSING_FLAG))
					{
						// skip length, frameControl and macCounter
						int i = 1 + 2 + 1;

						// pan id
						uint16_t panId = receive.packet[i] | (receive.packet[i + 1] << 8);
						i += 2;

						// skip destination address
						if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) != 0)
							i += 8;
						else
							i += 2;

						// source address
						uint8_t const *sourceAddress = receive.packet + i;
						int len = (frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0 ? 2 : 8;

						// check if it is a data request command
						if (receive.packet[i + len] == uint8_t(ieee::Command::DATA_REQUEST)) {
							// check if there is a packet pending for this source and set flag in ACK packet
							if (c.isPending(panId, len, sourceAddress))
								Radio::ackPacket[1] = uint8_t(ieee::FrameControl::TYPE_ACK | ieee::FrameControl::FRAME_PENDING);

							// don't pass data request packet to context
							receive.passFlags &= ~passFlag;
						}
					}
				}
			}
			passFlag <<= 1;
		}

		// check if we need to send an ACK
		if (ack) {
			// note: clear channel assessment for normal send operation may be in progress at this point. This means
			// that if the sender gets enabled (TXREADY), ACK has to override the normal send operation

			// copy frame sequence number to ACK packet
			Radio::ackPacket[3] = receive.packet[3];

			// send ACK on END event
			Radio::endAction = EndAction::SEND_ACK;
		}

		// check if a context is interested in the packet
		if (receive.passFlags != 0) {
			// capture timestamp of received packet (CC[1] is currently unused)
			if (Radio::RECEIVE_EXTRA_LENGTH >= 5) {
				NRF_TIMER0->TASKS_CAPTURE[1] = TRIGGER;
				uint32_t timestamp = NRF_TIMER0->CC[1];
				uint8_t *ts = &receive.packet[1 + length - 2 + 1];
				ts[0] = timestamp;
				ts[1] = timestamp >> 8;
				ts[2] = timestamp >> 16;
				ts[3] = timestamp >> 24;
			}

			// make the received element valid at the back of the queue
			Radio::receiveQueue.addBack();

			// signal to handle() that a received packet is ready
			NRF_EGU0->TASKS_TRIGGER[1] = TRIGGER;
		}
	}

	// check if packet was received with crc error
	if (NRF_RADIO->EVENTS_CRCERROR) {
		NRF_RADIO->EVENTS_CRCERROR = 0;

		// worst case assumption as we don't know how long the packet was
		Radio::ifsDuration = minLifsDuration;
	}

	// check if transfer has ended (receive or send)
	if (NRF_RADIO->EVENTS_END) {
		NRF_RADIO->EVENTS_END = 0;

		// capture time when last packet was sent or received (done automatically by PPI channel 27)
		//NRF_TIMER0->TASKS_CAPTURE[2] = TRIGGER;

		// disable after send (done automatically by SHORTS)
		//if (Radio::sendState != SendState::IDLE)
		//	NRF_RADIO->TASKS_DISABLE = TRIGGER; -> DISABLED

		// decide what to do next
		switch (Radio::endAction) {
		case EndAction::NOP:
			// do nothing
			break;
		case EndAction::RECEIVE:
			// continue receiving
			if (Radio::receiverEnabled/* && !Radio::receiveQueue.isFull()*/)
				startReceive(); // -> END
			break;
		case EndAction::SEND_ACK:
			// enable sender for sending ack
			NRF_RADIO->TASKS_TXEN = TRIGGER; // -> TXREADY

			// set timer for ack turnaround duration
			NRF_TIMER0->CC[0] = NRF_TIMER0->CC[2] + ackTurnaroundDuration;
			NRF_TIMER0->EVENTS_COMPARE[0] = 0;
			NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE0, Set); // -> COMPARE[0]

			// now wait for timeout
			break;
		case EndAction::ON_SENT:
		{
			// determine interframe spacing duration
			uint8_t length = Radio::sendPacket[0];
			Radio::ifsDuration = length <= maxSifsLength ? minSifsDuration : minLifsDuration;

			// check if we have to wait for an ACK
			if (Radio::sendState == SendState::AWAIT_SENT_ACK) {
				Radio::sendState = SendState::AWAIT_ACK;

				// yes: set timer for ACK wait duration
				NRF_TIMER0->CC[1] = NRF_TIMER0->CC[2] + ackWaitDuration;
				NRF_TIMER0->EVENTS_COMPARE[1] = 0;
				NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE1, Set); // -> COMPARE[1]

				// now we either receive an ACK packet or timeout
			} else {
				// no: set state of sent packet to success, send state becomes idle
				setSendResult(Radio::backoffCount);
			}
		}
			break;
		}
	}

	// check if sender has been disabled
	if (NRF_RADIO->EVENTS_DISABLED) {
		NRF_RADIO->EVENTS_DISABLED = 0;

		// clear shortcut END -> DISABLE
		NRF_RADIO->SHORTS = 0;

		// enable receiver again if radio is active
		if (Radio::active) {
			NRF_RADIO->TASKS_RXEN = TRIGGER; // -> RXREADY
		}
	}
}

void TIMER0_IRQHandler(void) {
	// read interrupt enable flags
	uint32_t INTEN = NRF_TIMER0->INTENSET;

	// check if ack turnaround time (send ACK after receive) elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE0, Enabled) && NRF_TIMER0->EVENTS_COMPARE[0]) {
		NRF_TIMER0->EVENTS_COMPARE[0] = 0;

		// disable timer interrupt 0
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE0, Clear);

		// send ack
		startSendAck();
	}

	// check if ACK wait duration (receive ACK after send) has elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE1, Enabled) && NRF_TIMER0->EVENTS_COMPARE[1]) {
		NRF_TIMER0->EVENTS_COMPARE[1] = 0;

		// disable timer interrupt 1
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE1, Clear);

		if (Radio::ackRetryCount < Radio::maxAckRetryCount) {
			// ack was not received: retry
			++Radio::ackRetryCount;
			startBackoff();
		} else {
			// sent packet was not acknowledged: set result to failed, send state becomes idle
			setSendResult(0);

			// check if more to send
			selectForSend();
		}
	}

	// check if backoff period elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE3, Enabled) && NRF_TIMER0->EVENTS_COMPARE[3]) {
		NRF_TIMER0->EVENTS_COMPARE[3] = 0;

		// disable timer interrupt 3
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE3, Clear);

		// check if in receive state
		bool s = isReceiveState();

		// get time since last packet was received or sent
		NRF_TIMER0->TASKS_CAPTURE[3] = TRIGGER;
		uint32_t t = NRF_TIMER0->CC[3] - NRF_TIMER0->CC[2];

		// check if the radio is in the right state and inter frame spacing is respected
		if (s && t >= Radio::ifsDuration) {
			// start clear channel assessment
			startClearChannelAssessment();
		} else {
			// backoff and try again
			backoff();
		}
	}
}

void start(int channel) {
	assert(channel >= 10 && channel <= 26);

	// set channel
	NRF_RADIO->FREQUENCY = V(RADIO_FREQUENCY_FREQUENCY, (channel - 10) * 5);

	// start timer
	NRF_TIMER0->TASKS_CLEAR = TRIGGER;
	NRF_TIMER0->TASKS_START = TRIGGER;

	lock();

	// check if the radio is disabled
	if (isDisabled()) {
		// enable receiving mode (not the baseband decoder, packets will not be received yet)
		NRF_RADIO->TASKS_RXEN = TRIGGER; // -> RXREADY
	}

	Radio::active = true;

	unlock();
}

void stop() {
	// disable timer
	NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE0, Clear)
		| N(TIMER_INTENCLR_COMPARE1, Clear)
		| N(TIMER_INTENCLR_COMPARE3, Clear);
	NRF_TIMER0->TASKS_STOP = TRIGGER;

	lock();

	// reset state variables
	Radio::active = false;
	Radio::receiverEnabled = false;
	Radio::endAction = EndAction::NOP;

	// disable radio
	NRF_RADIO->SHORTS = 0;
	NRF_RADIO->TASKS_STOP = TRIGGER;
	NRF_RADIO->TASKS_EDSTOP = TRIGGER;
	NRF_RADIO->TASKS_CCASTOP = TRIGGER;
	NRF_RADIO->TASKS_DISABLE = TRIGGER; // -> DISABLED

	// clear queues
	Radio::receiveQueue.clear();
	Radio::sendState = SendState::IDLE;
	for (auto &context : Radio::contexts) {
		/*context.receiveQueue.resumeAll([](ReceiveParameters &p) {
			// coroutines waiting for receive get a null pointer
			p.data = nullptr;
			return true;
		});*/
		context.sendWaitlist.resumeAll([](SendParameters &p) {
			// coroutines waiting for send get a failure result
			p.result = 0;
			return true;
		});
	}

	unlock();
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {
	Radio::onEdReady = onReady;

	// shortcut: start energy detection on READY event
	NRF_RADIO->SHORTS = N(RADIO_SHORTS_READY_EDSTART, Enabled);

	// start energy detection if radio is in receive sate (RxIdle or Rx)
	if (isReceiveState()) {
		NRF_RADIO->SHORTS = 0;
		NRF_RADIO->TASKS_EDSTART = TRIGGER; // -> EDEND
	}
}

void enableReceiver(bool enable) {
	lock();
	{
		Radio::receiverEnabled = enable;

		// start receiver (base band decoder) if radio is in RxIdle state, otherwise do it on RXREADY
		if (isRxIdle() && enable/* && !Radio::receiveQueue.isFull()*/)
			// todo: maybe there is a race condition if the radio automatically switches to TXREADY after CCA here. maybe wait until CCA has ended
			startReceive(); // -> END
	}
	unlock();
}

void setLongAddress(uint64_t longAddress) {
	Radio::longAddress = longAddress;
}

uint64_t getLongAddress() {
	return Radio::longAddress;
}

void setPan(int index, uint16_t pan) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.pan = pan;
}

void setShortAddress(int index, uint16_t shortAddress) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.shortAddress = shortAddress;
}

void setFlags(int index, ContextFlags flags) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.flags = flags;
}

Awaitable<ReceiveParameters> receive(int index, Packet &packet) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = Radio::contexts[index];

	return {context.receiveWaitlist, packet};
}

Awaitable<SendParameters> send(int index, uint8_t *packet, uint8_t &result) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = Radio::contexts[index];

	// initialize result, is used to check if the send operation has finished
	result = 255;

	// get send flags
	int length = packet[0] - 2;
	auto sendFlags = SendFlags(packet[1 + length]);

	// check if we can immediately start to send (idle and don't need to wait for data request)
	//lock();
	if (Radio::sendState == SendState::IDLE && sendFlags == SendFlags::NONE)
		prepareForSend(index, context.flags, packet, result);
	//unlock();

	return {context.sendWaitlist, packet, result};
}

} // namespace Radio

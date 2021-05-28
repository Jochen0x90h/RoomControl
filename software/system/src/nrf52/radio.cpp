#include "../radio.hpp"
#include "loop.hpp"
#include "global.hpp"
#include "rng.hpp"
#include <debug.hpp>
#include <assert.hpp>
#include <Queue.hpp>


// debug
/*
#define initSignals() configureOutput(3), configureOutput(2)
#define setSendSignal(value) setOutput(3, value)
#define toggleSendSignal() toggleOutput(3)
#define setReceiveSignal(value) setOutput(2, value)
#define toggleReceiveSignal() toggleOutput(2)
*/
#define initSignals()
#define setSendSignal(value)
#define toggleSendSignal()
#define setReceiveSignal(value)
#define toggleReceiveSignal()


/*
	resources:
	NRF_RADIO
	NRF_TIMER0
		CC[0]: ack wait timeout, maximum time to wait for ack
		CC[1]: ack turnaround timeout, minimum time between received packet and ack
		CC[2]: time of last packet received or sent
		CC[3]: backoff timeout
	NRF_PPI
		CH[27]: RADIO->EVENTS_END -> TIMER0->TASKS_CAPTURE[2]
	EGU0
		TRIGGER[0]: energy detection
		TRIGGER[1]: receive queue
		TRIGGER[2]: send queue
*/

/*
	Glossary:
	CCA: Clear Channel Assessment (-> ED and/or carrier detection)
	ED: Energy Detection on a radio channel
	RFD: Reduced-function defice (can only talk to -> FFDs)
	FFD: Full-function device
	MLME: MAC Layer Management Entity, management service of the MAC layer
	MCPS: MAC Common Part Layer, data transport service of the MAC layer
	PIB:  PAN Information Base
*/
namespace radio {

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
	ON_SENT
};
EndAction endAction;


// energy detection
// ----------------

std::function<void (uint8_t)> onEdReady;


// context
// -------

uint32_t longAddressLo;
uint32_t longAddressHi;

struct Context {
	// flags and pan used for filtering by pan
	uint16_t volatile flags = 0;
	uint16_t volatile pan = 0xffff;
	uint16_t volatile shortAddress = 0xffff;
	
	// receive handler
	std::function<void (uint8_t *)> onReceived;
	
	bool filter(uint8_t const *data) const {
		uint8_t const *mac = data + 1;
		int flags = this->flags;
		
		// ALL: all packets pass
		if (flags & radio::ALL)
			return true;
		
		// reject frames with no sequence number
		if (mac[1] & 1)
			return false;
		
		// get frame type
		FrameType frameType = FrameType(mac[0] & 0x07);

		// beacon packets
		if ((flags & radio::TYPE_BEACON) && frameType == FrameType::BEACON)
			return true;

		if (mac[1] & 0x08) {
			// has destination address: check pan
			uint16_t pan = mac[3] | (mac[4] << 8);
			if (pan != 0xffff && pan != this->pan)
				return false;
				
			if (mac[1] & 0x04) {
				// long destination addressing mode
				if (flags & radio::DEST_LONG) {
					uint32_t longAddressLo = mac[5] | (mac[6] << 8) | (mac[7] << 16) | (mac[8] << 24);
					uint32_t longAddressHi = mac[9] | (mac[10] << 8) | (mac[11] << 16) | (mac[12] << 24);
					if (longAddressLo == radio::longAddressLo && longAddressHi == radio::longAddressHi)
						return true;
				}
			} else {
				// short destination addressing mode
				if ((flags & radio::DEST_SHORT) || ((flags & radio::TYPE_DATA_DEST_SHORT) && frameType == FrameType::DATA)) {
					uint16_t shortAddress = mac[5] | (mac[6] << 8);
					if (shortAddress == 0xffff || shortAddress == this->shortAddress)
						return true;
				}
			}
		}
		
		return false;
	}
};

Context contexts[RADIO_CONTEXT_COUNT];


// receiver
// --------

// receiver state (base band decoder)
bool volatile receiverEnabled = false;

// receive queue
struct Receive {
	// receive buffer: length, payload, LQI
	uint8_t data[1 + RADIO_MAX_PAYLOAD_LENGTH + 1];
};
volatile Queue<Receive, RADIO_RECEIVE_QUEUE_LENGTH> receiveQueue;

// start receiving packets
inline void startReceive() {
	setReceiveSignal(true);

	// continue to receive on END event
	radio::endAction = EndAction::RECEIVE;

	Receive &receive = radio::receiveQueue.getHead();	
	NRF_RADIO->PACKETPTR = intptr_t(receive.data);
	NRF_RADIO->TASKS_START = Trigger; // -> END
}


// sender
// ------

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

// send queue
struct Send {
	enum State : uint8_t {
		// send and don't wait for ack
		AWAIT_SENT,
		
		// wait for an ack from the destination
		AWAIT_ACK,
		
		// successfully sent
		SUCCESS,
		
		// send has failed, e.g. maxBackoffCount reached or no ack received
		FAILED,
		
		// onSent is currently being called
		CALL
	};

	uint8_t const *data;
	std::function<void (bool)> onSent;
	State state;
};
Queue<Send, RADIO_SEND_QUEUE_LENGTH> volatile sendQueue;

// start clear channel assessment
inline void startClearChannelAssessment() {
	// start clear channel assessment
	NRF_RADIO->TASKS_CCASTART = Trigger; // -> CCABUSY or CCAIDLE -> TXEN -> TXREADY
}

// backoff for csma-ca
inline void backoff() {

	// throw the dice to determine backoff period
	int backoff = (rng::get8() & ~(0xffffffff << radio::backoffExponent)) + 1;
	int backoffDuration = backoff * unitBackoffDuration;
	//int backoffDuration = unitBackoffDuration;

	// update backoff parameters
	radio::backoffExponent = min(radio::backoffExponent + 1, radio::maxBackoffExponent);
	++radio::backoffCount;

	// set timer for backoff duration
	NRF_TIMER0->TASKS_CAPTURE[3] = Trigger;
	NRF_TIMER0->CC[3] += backoffDuration;
	NRF_TIMER0->EVENTS_COMPARE[3] = 0;
	NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE3, Set);
}

inline void startBackoff() {
	radio::backoffExponent = radio::minBackoffExponent;
	radio::backoffCount = 0;
	backoff();
}

// start to send a packet
inline void startSend() {
setSendSignal(true);
	// handle sent packet on END event
	radio::endAction = EndAction::ON_SENT;

	Send &send = radio::sendQueue.get();
	NRF_RADIO->PACKETPTR = intptr_t(send.data);
	NRF_RADIO->TASKS_START = Trigger; // -> END	
}

// ack packet, set sequence number to ack[3]
uint8_t ackPacket[] = {5, 0x02, 0x00, 0};

// start to send an ack packet
inline void startSendAck() {
setSendSignal(true);

	// don't do anything on END event
	radio::endAction = EndAction::NOP;

	NRF_RADIO->PACKETPTR = intptr_t(radio::ackPacket);
	NRF_RADIO->TASKS_START = Trigger; // -> END	
}


// event loop handler chain
loop::Handler nextHandler;
void handle() {
	if (isInterruptPending(SWI0_EGU0_IRQn)) {
		// check energy detection
		if (NRF_EGU0->EVENTS_TRIGGERED[0]) {
			NRF_EGU0->EVENTS_TRIGGERED[0] = 0;
			radio::onEdReady(NRF_RADIO->EDSAMPLE);
		}
		
		// check receive queue
		if (NRF_EGU0->EVENTS_TRIGGERED[1]) {
			NRF_EGU0->EVENTS_TRIGGERED[1] = 0;
			do {
				Receive &receive = receiveQueue.get();
		
				// call receive handlers
				for (auto const &c : radio::contexts) {
					if (c.onReceived && c.filter(receive.data))
						c.onReceived(receive.data);
				}
				
				// remove element from queue and start receive if the buffer was full
				disableInterrupt(RADIO_IRQn);
				{
					bool full = radio::receiveQueue.full();
					
					// remove entry from receive queue unless queue was emptied in onReceived() by calling stop()
					if (!receiveQueue.empty())
						receiveQueue.remove();
					
					// continue receiving if the queue has now space again
					if (full) {
						startReceive(); // -> END
					}
				}
				enableInterrupt(RADIO_IRQn);
			} while (!radio::receiveQueue.empty());
		}
				
		// check send queue
		if (NRF_EGU0->EVENTS_TRIGGERED[2]) {
			NRF_EGU0->EVENTS_TRIGGERED[2] = 0;

			Send &send = sendQueue.get();
			bool success = send.state == Send::SUCCESS;
			
			// set to call state so that stop() doesn't remove the queue entry
			send.state = Send::CALL;
			if (send.onSent)
				send.onSent(success);
			
			sendQueue.remove();
			
			// send next packet in queue
			if (!sendQueue.empty())
				startBackoff();
		}

		// clear pending interrupt flag at NVIC
		clearInterrupt(SWI0_EGU0_IRQn);
	}

	// call next handler in chain
	radio::nextHandler();
}

void init() {
	initSignals();
	
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

	// capture time when packet was received or sent
	NRF_PPI->CHENSET = 1 << 27;

	// init event generator
	NRF_EGU0->INTENSET =
		N(EGU_INTENSET_TRIGGERED0, Set)
		| N(EGU_INTENSET_TRIGGERED1, Set)
		| N(EGU_INTENSET_TRIGGERED2, Set);

	// add to event loop handler chain
	radio::nextHandler = loop::addHandler(handle);
}

extern "C" {
	void RADIO_IRQHandler(void);
	void TIMER0_IRQHandler(void);
}

void RADIO_IRQHandler(void) {
	// check if ready to receive (RxIdle state)
	if (NRF_RADIO->EVENTS_RXREADY) {
		NRF_RADIO->EVENTS_RXREADY = 0;
	
		// shortcut: stop receiving and enable sender if channel is clear
		NRF_RADIO->SHORTS = N(RADIO_SHORTS_CCAIDLE_STOP, Enabled)
			| N(RADIO_SHORTS_CCAIDLE_TXEN, Enabled);
		
		// start first receive
		if (radio::receiverEnabled && !radio::receiveQueue.full())
			startReceive(); // -> END
	}

	// check if end of energy detection
	if (NRF_RADIO->EVENTS_EDEND) {
		NRF_RADIO->EVENTS_EDEND = 0;

		// signal end of energy detection to handle()
		NRF_EGU0->TASKS_TRIGGER[0] = Trigger;
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
		if (NRF_RADIO->EVENTS_CCAIDLE) {
			NRF_RADIO->EVENTS_CCAIDLE = 0;

			// start to send the first packet in queue		
			startSend();
		}
	}
	
	// check if packet was received with no crc error
	if (NRF_RADIO->EVENTS_CRCOK) {
		NRF_RADIO->EVENTS_CRCOK = 0;
	
		Receive &receive = radio::receiveQueue.getHead();
		uint8_t length = receive.data[0];
		radio::ifsDuration = length <= maxSifsLength ? minSifsDuration : minLifsDuration;

		// check if sent packet awaits an ack
		if (!sendQueue.empty()) {
			Send &send = sendQueue.get();
			if (send.state == Send::AWAIT_ACK) {
				// check if this is the ack packet that we are waiting for
				if (FrameType(receive.data[1] & 7) == FrameType::ACK && receive.data[3] == radio::ackPacket[3]) {
					// disable ack wait interrupt
					NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE1, Clear);

					// set state of sent packet to success and signal it to handle()
					send.state = Send::SUCCESS;
					NRF_EGU0->TASKS_TRIGGER[2] = Trigger;
				}
			}
		}

		// check if a context is interested in this packet and wants to handle ACK
		bool pass = false;
		bool ack = false;
		for (auto const &c : radio::contexts) {
			if (c.flags != 0 && c.filter(receive.data)) {
				pass = true;
				ack |= (c.flags & radio::HANDLE_ACK) != 0;
			}
		}			

		// check if ack is requested in ieee 802.15.4 header
		ack &= (receive.data[1] & 0x20) != 0;
		
		if (pass) {
			// increment head of queue because received element is ready now
			radio::receiveQueue.increment();

			// signal to handle() that a received packet is ready
			NRF_EGU0->TASKS_TRIGGER[1] = Trigger;
			
			// check if we need to send ack
			if (ack) {
				// copy frame sequence number to ack packet
				radio::ackPacket[3] = receive.data[3];

				// ack on END event
				radio::endAction = EndAction::SEND_ACK;
			}
		}
	}

	// check if packet was received with crc error
	if (NRF_RADIO->EVENTS_CRCERROR) {
		NRF_RADIO->EVENTS_CRCERROR = 0;
	
		// worst case assumption
		radio::ifsDuration = minLifsDuration;
	}


	// check if transfer has ended (receive or send)
	if (NRF_RADIO->EVENTS_END) {
		NRF_RADIO->EVENTS_END = 0;
setReceiveSignal(false);
setSendSignal(false);
		
		// done automatically: capture time when last packet was sent or received
		//NRF_TIMER0->TASKS_CAPTURE[2] = Trigger;
		
		switch (radio::endAction) {
		case EndAction::NOP:
			break;
		case EndAction::RECEIVE:
			// continue receiving
			if (radio::receiverEnabled && !radio::receiveQueue.full())
				startReceive(); // -> END
			break;
		case EndAction::SEND_ACK:
			{
				// enable sender for sending ack
				NRF_RADIO->TASKS_TXEN = Trigger; // -> TXREADY				
			
				// set timer for ack turnaround duration
				NRF_TIMER0->CC[0] = NRF_TIMER0->CC[2] + ackTurnaroundDuration;
				NRF_TIMER0->EVENTS_COMPARE[0] = 0;
				NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE0, Set); // -> COMPARE[0]
				// now wait for timeout
			}
			break;
		case EndAction::ON_SENT:
			{
				// get sent packet
				Send &send = radio::sendQueue.get();
	
				uint8_t length = send.data[0];
				radio::ifsDuration = length <= maxSifsLength ? minSifsDuration : minLifsDuration;
	
				// check if we have to wait for an ack
				if (send.state == Send::AWAIT_ACK) {
					// copy sequence number to be able to check it when ack arrives
					radio::ackPacket[3] = send.data[3];
					
					// set timer for ack wait duration
					NRF_TIMER0->CC[1] = NRF_TIMER0->CC[2] + ackWaitDuration;
					NRF_TIMER0->EVENTS_COMPARE[1] = 0;
					NRF_TIMER0->INTENSET = N(TIMER_INTENSET_COMPARE1, Set); // -> COMPARE[1]
				
					// now we either receive an ack packet or timeout 
				} else {
					// set state of sent packet to success and signal it to handle()
					send.state = Send::SUCCESS;
					NRF_EGU0->TASKS_TRIGGER[2] = Trigger;
				}
			}
			break;
		}
	}

	// check if sender has been disabled
	if (NRF_RADIO->EVENTS_DISABLED) {
		NRF_RADIO->EVENTS_DISABLED = 0;
	
		// enable receiver again if radio is active
		if (radio::active) {
			NRF_RADIO->TASKS_RXEN = Trigger; // -> RXREADY
			
		}
	}
}

void TIMER0_IRQHandler(void) {
	// read interrupt enable flags
	uint32_t INTEN = NRF_TIMER0->INTENSET;

	// check if ack turnaround time elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE0, Enabled) && NRF_TIMER0->EVENTS_COMPARE[0]) {
		NRF_TIMER0->EVENTS_COMPARE[0] = 0;

		// disable interrupt
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE0, Clear);
					
		// send ack
		startSendAck();
	}
	
	// check if ack wait duration has elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE1, Enabled) && NRF_TIMER0->EVENTS_COMPARE[1]) {
		NRF_TIMER0->EVENTS_COMPARE[1] = 0;

		// disable interrupt
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE1, Clear);

		// sent packet was not acknowledged and signal it to handle()
		radio::sendQueue.get().state = Send::FAILED;
		NRF_EGU0->TASKS_TRIGGER[2] = Trigger;
	}

	// check if backoff period elapsed
	if (TEST(INTEN, TIMER_INTENSET_COMPARE3, Enabled) && NRF_TIMER0->EVENTS_COMPARE[3]) {
		NRF_TIMER0->EVENTS_COMPARE[3] = 0;

		// disable interrupt
		NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE3, Clear);

		// get state
		bool s = isReceiveState();

		// get time since last packet was receive dor sent
		NRF_TIMER0->TASKS_CAPTURE[3] = Trigger;
		uint32_t t = NRF_TIMER0->CC[3] - NRF_TIMER0->CC[2];

		// check if the radio is in the right state and inter frame spacing is respected
		if (s && t >= radio::ifsDuration) {
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
	NRF_TIMER0->TASKS_START = Trigger;

	disableInterrupt(RADIO_IRQn);
	
	// enable receiving mode (not the baseband decoder, packets will not be received yet)
	if (isDisabled())
		NRF_RADIO->TASKS_RXEN = Trigger; // -> RXREADY

	radio::active = true;
	
	enableInterrupt(RADIO_IRQn);
}

void stop() {
	// disable timer
	NRF_TIMER0->INTENCLR = N(TIMER_INTENCLR_COMPARE0, Clear)
		| N(TIMER_INTENCLR_COMPARE1, Clear)
		| N(TIMER_INTENCLR_COMPARE3, Clear);
	NRF_TIMER0->TASKS_STOP = Trigger;

	disableInterrupt(RADIO_IRQn);

	// reset state variables
	radio::active = false;
	radio::receiverEnabled = false;
	radio::endAction = EndAction::NOP;

	// disable radio
	NRF_RADIO->SHORTS = 0;
	NRF_RADIO->TASKS_STOP = Trigger;	
	NRF_RADIO->TASKS_EDSTOP = Trigger;
	NRF_RADIO->TASKS_CCASTOP = Trigger;
	NRF_RADIO->TASKS_DISABLE = Trigger; // -> DISABLED

	// clear queues
	radio::receiveQueue.clear();
	if (!radio::sendQueue.empty()) {
		Send &send = sendQueue.get();		
		radio::sendQueue.clear();
		
		// preserve element if onSent is currently called
		if (send.state == Send::CALL)
			radio::sendQueue.increment();
	}
	
	enableInterrupt(RADIO_IRQn);
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {
	radio::onEdReady = onReady;
	
	// shortcut: start energy detection on READY event
	NRF_RADIO->SHORTS = N(RADIO_SHORTS_READY_EDSTART, Enabled);

	// start energy detection if radio is in receive sate (RxIdle or Rx)
	if (isReceiveState()) {
		NRF_RADIO->SHORTS = 0;
		NRF_RADIO->TASKS_EDSTART = Trigger; // -> EDEND
	}
}

void enableReceiver(bool enable) {
	disableInterrupt(RADIO_IRQn);
	{
		radio::receiverEnabled = enable;
		
		// start receiver (base band decoder) if radio is in RxIdle state, otherwise do it on RXREADY
		if (isRxIdle() && enable && !radio::receiveQueue.full())
			startReceive(); // -> END
	}
	enableInterrupt(RADIO_IRQn);
}

void setLongAddress(uint64_t longAddress) {
	radio::longAddressLo = uint32_t(longAddress);
	radio::longAddressHi = uint32_t(longAddress >> 32);
}

void setFlags(int index, uint16_t flags) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.flags = flags;
}

void setPan(int index, uint16_t pan) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.pan = pan;
}

void setShortAddress(int index, uint16_t shortAddress) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.shortAddress = shortAddress;
}

void setReceiveHandler(int index, std::function<void (uint8_t *)> const &onReceived) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];
	
	c.onReceived = onReceived;
}

bool send(int index, uint8_t const* data, std::function<void (bool)> const &onSent) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	
	// check if send queue is full
	if (radio::sendQueue.full())
		return false;

	Context &c = radio::contexts[index];

	disableInterrupt(RADIO_IRQn);
	{
		bool empty = radio::sendQueue.empty();
		Send &send = radio::sendQueue.add();
		
		// set data, callback and state
		send.data = data;
		send.onSent = onSent;
		send.state = c.flags & HANDLE_ACK ? Send::AWAIT_ACK : Send::AWAIT_SENT;
		
		// try to send packet if queue was empty by starting backoff sequence
		if (empty) {
			startBackoff();
		}
	}
	enableInterrupt(RADIO_IRQn);
	
	return true;
}


} // namespace radio

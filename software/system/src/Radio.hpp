#pragma once

#include "RadioDefs.hpp"
#include <Coroutine.hpp>
#include <functional>
#include <boardConfig.hpp>


namespace Radio {

enum class SendFlags : uint8_t {
	NONE = 0,
	AWAIT_DATA_REQUEST = 1,
};
FLAGS_ENUM(SendFlags);


/**
 * Receive: one byte link quality indicator (LQI) and 4 byte timestamp
 */
constexpr int RECEIVE_EXTRA_LENGTH = 1 + 4;

/**
 * Send: one byte send flags
 */
constexpr int SEND_EXTRA_LENGTH = 1;

/**
 * Radio packet containing one byte length, the payload, and extra data.
 * The length in the first byte is the length of the payload and crc (2 byte). The length of the
 * payload, which starts at packet[1], is therefore packet[0] - 2;
 */
using Packet = uint8_t[1 + RADIO_MAX_PAYLOAD_LENGTH + RECEIVE_EXTRA_LENGTH];


// Internal helper: Stores the receive parameters and a reference to the result value in the awaitable during co_await
struct ReceiveParameters : public WaitlistElement {
	Packet &packet;

	// default constructor
	ReceiveParameters(Packet &packet) : packet(packet) {}

	// move constructor
	ReceiveParameters(ReceiveParameters &&p) noexcept;
	
	// add to list
	void add(WaitlistHead &head) noexcept;

	// remove from list
	void remove() noexcept;
};

// Internal helper: Stores the send parameters in the awaitable during co_await
struct SendParameters : public WaitlistElement {
	uint8_t *packet;
	uint8_t &result;

	// default constructor
	SendParameters(uint8_t *packet, uint8_t &result) : packet(packet), result(result) {}

	// move constructor
	SendParameters(SendParameters &&p) noexcept;
	
	// add to list
	void add(WaitlistHead &head) noexcept;

	// remove from list
	void remove() noexcept;
};


/**
 * Initialize the radio. Depends on the random number generator, also call rng::init()
 */
void init();

/**
 * Start the radio in receiving mode but with no baseband decoding. For this call enableReceive(true)
 * @param channel 10 - 26, 2400MHz - 2480MHz
 */
void start(int channel);

/**
 * Stop the radio. Need to call start() again
 */
void stop();

/**
 * Receiver energy detection (ED) for channel selection. Radio must be started using start(), but
 * enableReceive() is not necessary. The detected energy is in the range [0, 127]. To convert to
 * IEEE 802.15.4 energy detect level, multiply by four and clamp to 255.
 * @param onReady called when operation finished with detected energy
 */
void detectEnergy(std::function<void (uint8_t)> const &onReady);

/**
 * Enable receiver (base band decoding). When true, receive handlers will be called when a packet arrives.
 * @param enable enable state
 */
void enableReceiver(bool enable);

/**
 * Set long address which is global for all contexts
 * @param address 64 bit long address
 */
void setLongAddress(uint64_t address);

/**
 * Get current long address
 */
uint64_t getLongAddress();

/**
 * Set pan id, default is 0xffff (broadcast)
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param pan pan id
 */
void setPan(int index, uint16_t pan);

/**
 * Set short address, default is 0xffff (broadcast)
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param shortAddress short address
 */
void setShortAddress(int index, uint16_t shortAddress); 

/**
 * Set filter and configuration flags
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param flags flags
 */
void setFlags(int index, ContextFlags flags);

/**
 * Suspend execution using co_await until a packet was received
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param packet received packet, first byte is length of the following payload + 2 for CRC (not included in data)
 */
[[nodiscard]] Awaitable<ReceiveParameters> receive(int index, Packet &packet);

/**
 * Suspend execution using co_await until send is finished
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param packet packet to send, first byte is length of the following payload + 2 for CRC (not included in data)
 * @param result number of backoffs needed when successful, zero on failure
 */
[[nodiscard]] Awaitable<SendParameters> send(int index, uint8_t *packet, uint8_t &result);

} // namespace Radio

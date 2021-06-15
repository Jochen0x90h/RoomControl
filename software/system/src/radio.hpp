#pragma once

#include "radioDefs.hpp"
#include "sysConfig.hpp"
#include <Coroutine.hpp>
#include <functional>


namespace radio {

/**
 * Receive Packet containing one byte length, the payload and one byte link quality indicator (LQI).
 * The length in the first byte is the length of the payload and crc (2 byte). The length of the
 * payload, which starts at packet[1], is therefore packet[0] - 2;
 */
using Packet = uint8_t[1 + RADIO_MAX_PAYLOAD_LENGTH + 1];

// Internal helper: Stores the parameters and a reference to the result value in the awaitable during co_await
struct ReceiveParameters {
	Packet &packet;
};

// Internal helper: Stores the parameters in the awaitable during co_await
struct SendParameters {
	uint8_t const *packet;
	uint8_t &result;
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
 * Wait for beacons (after a beacon request was sent) for baseSuperFrameDuration * 2^n + 1
 * @param n exponent n in wait time baseSuperFrameDuration * (2^n + 1) where in is 0 to 14
 * @param timeout called when wait time has elapsed
 */
//void wait(int n, std::function<void ()> const &onTimeout);

/**
 * Set long address which is global for all contexts
 * @param address 64 bit long address
 */
void setLongAddress(uint64_t address);

/**
 * Set filter and configuration flags
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param flags flags
 */
void setFlags(int index, ContextFlags flags);

/**
 * Set pan, default is 0xffff (broadcast)
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param pan pan
 */
void setPan(int index, uint16_t pan);

/**
 * Set short address, default is 0xffff (broadcast)
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param shortAddress short address
 */
void setShortAddress(int index, uint16_t shortAddress); 

/**
 * Suspend execution using co_await until data was received
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param data received data, first byte is length of the following payload + 2 for CRC (not included in data)
 */
Awaitable<ReceiveParameters> receive(int index, Packet &packet);

/**
 * Suspend execution using co_await until send is finished
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param data data to send, first byte is length of the following payload + 2 for CRC (not included in data)
 * @param result number of backoffs needed when successful, zero on failure
 */
Awaitable<SendParameters> send(int index, uint8_t const *packet, uint8_t &result);

} // namespace radio

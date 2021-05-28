#pragma once

#include "radioDefs.hpp"
#include <functional>


namespace radio {

/**
 * Initialize the radio
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
void setFlags(int index, uint16_t flags);

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
 * Set handler that gets called when a packet was received and it passes the filters for the context.
 * The payload data starts at data[1] and the length of the payload is in first byte and gets calculated as
 * length = data[0] - 2 (crc is not included). After the last payload byte, a link quality indicator is transferred.
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param onReceived called when data was received, must not be null
 */
void setReceiveHandler(int index, std::function<void (uint8_t *)> const &onReceived);

/**
 * Send data over radio when no other carrier was detected
 * @param index context index (number of contexts defined by RADIO_CONTEXT_COUNT in sysConfig.hpp)
 * @param data data to send, first byte is length of the following payload + 2 for CRC (not included in data)
 * @param onSent called when finished sending with parameter true on success and false on failure
 * @return true on success, false if busy with previous send
 */
bool send(int index, uint8_t const *data, std::function<void (bool)> const &onSent);

} // namespace radio

#pragma once

#include <cstdint>
#include <functional>


namespace radio {

// filter flags (filtered packets must have a sequence number)

// all packets pass
constexpr int ALL = 1;

// beacon packets pass
constexpr int TYPE_BEACON = 2;

// packets pass when destination pan and short address match or are broadcast
constexpr int DEST_SHORT = 4;

// data packets pass when destination pan and short address match or are broadcast (green power)
constexpr int TYPE_DATA_DEST_SHORT = 8;

// packets pass where destination pan and long address match or are broadcast
constexpr int DEST_LONG = 16;

// handle ack for own packets in radio driver to meet turnaround time. Make sure the destination address filter is set
constexpr int HANDLE_ACK = 32;


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
 * enableReceive() is not necessary.
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
 * Allocate a radio context. Call the following methods to configure it
 * @return id of radio context (not zero)
 */
uint8_t allocate();

/**
 * Set filter and configuration flags
 * @param id context id
 * @param flags flags
 */
void setFlags(uint8_t id, uint16_t flags);

/**
 * Set pan, default is 0xffff (broadcast)
 * @param id context id
 * @param pan pan
 */
void setPan(uint8_t id, uint16_t pan);

/**
 * Set short address, default is 0xffff (broadcast)
 * @param id context id
 * @param shortAddress short address
 */
void setShortAddress(uint8_t id, uint16_t shortAddress); 

/**
 * Set handler that gets called when a packet was received and it passes the filters for the context.
 * The payload data starts at data[1] and the length of the payload is in first byte and gets calculated as
 * length = data[0] - 2 (crc is not included). After the last payload byte, a link quality indicator is transferred.
 * @param id context id
 * @param onReceived called when data was received, must not be null
 */
void setReceiveHandler(uint8_t id, std::function<void (uint8_t *)> const &onReceived);

/**
 * Send data over radio when no other carrier was detected
 * @param id context id
 * @param data data to send, first byte is length of the following payload + 2 for CRC (not included in data)
 * @param onSent called when finished sending with parameter true on success and false on failure
 * @return true on success, false if busy with previous send
 */
bool send(uint8_t id, uint8_t const *data, std::function<void (bool)> const &onSent);


} // namespace radio

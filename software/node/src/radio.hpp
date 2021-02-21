#pragma once

#include <cstdint>
#include <functional>


namespace radio {

/**
 * Initialze the radio
 */
void init();

/**
 * Handle radio events
 */
void handle();

/**
 * Set radio channel
 */
void setChannel(int channel);

/**
 * Send data over radio when no other carrier was detected
 * @param data data to send
 * @param length data length
 * @param onSent called when finished sending
 * @return true on success, false if busy with previous send
 */
bool send(uint8_t const *data, int length, std::function<void ()> onSent);

/**
 * Set handler that gets called when data was received
 * @param onReceived called when data was received
 */
void setReceiveHandler(std::function<void (uint8_t const *, int)> onReceived);

} // namespace radio

#pragma once

#include <cstdint>
#include <functional>


namespace bus {

/**
 * Initialze the bus
 */
void init();

/**
 * Handle bus events
 */
void handle();

/**
 * Send data on the bus, cancels any running transfer
 * @param txData data to transmit
 * @param txLength length of data to transmit
 * @param onRx called when data was received on the bus (transmit data and reply by a device). Zero length is error 
 */
void transferBus(uint8_t const *txData, int txLength, std::function<void (uint8_t const *, int)> onRx);

/**
 * Set handler that gets called when a device requests to be queried
 * @param onRequest called when a device requests to be queried
 */ 
void setRequestHandler(std::function<void (uint8_t const *, int)> onRequest);

} // namespace system

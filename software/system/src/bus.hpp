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
 * @param rxData data to receive, always receives own transmit data before reply by a device
 * @param rxLength maximum length of data to receive
 * @param onTransferred called when data was transferred. Zero length is error 
 */
void transfer(uint8_t const *txData, int txLength, uint8_t *rxData, int rxLength,
	std::function<void (int)> const &onTransferred);

/**
 * Set handler that gets called when a device requests to be queried
 * @param onRequest called when a device requests to be queried
 */ 
void setRequestHandler(std::function<void (uint8_t)> const &onRequest);

} // namespace system

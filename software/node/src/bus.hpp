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
 * Transfer data on the bus, cancels any running transfer
 * @param sendData data to send
 * @param sendLength length of data to send
 * @param receiveData data to receive
 * @param receiveLength length of data to receive
 * @param onTransferred completion handler 
 */
void transferBus(uint8_t const *sendData, int sendLength, uint8_t *receiveData, int receiveLength,
	std::function<void ()> onTransferred);

/**
 * Set handler that gets called when a device sends a wakeup request
 * @param onWakeup wakeup handler
 */ 
void setWakeupHandler(std::function<void ()> onWakeup);

} // namespace system

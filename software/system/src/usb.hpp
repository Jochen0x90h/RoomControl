#pragma once

#include "usbDefs.hpp"
#include <Data.hpp>
#include <cstdint>
#include <functional>


namespace usb {

/**
 * Initialize USB
 * @param getDescriptor callback for obtaining descriptors
 * @param onSetConfiguration callback for setting the configuration (libusb_set_configuration() on host), always called from event loop
 * @param onRequest callback for vendor specific request
 */
void init(
	std::function<Data (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t bConfigurationValue)> const &onSetConfiguration,
	std::function<bool (uint8_t bRequest, uint16_t wValue, uint16_t wIndex)> const &onRequest);

/**
 * Enable endpoints. Can be done in onSetConfiguration. Endpoint 0 should stay enabled
 */
void enableEndpoints(uint8_t inFlags, uint8_t outFlags);

/**
 * Send data over an endpoint (IN transfer)
 * @param endpont endpoint index (1-7)
 * @param data data to send, must be in ram and 32 bit aligned
 * @param length data length
 * @param onSent called when finished sending
 * @return true on success, false if busy with previous send
 */
bool send(int endpoint, void const *data, int length, std::function<void ()> const &onSent);

/**
 * Receive data over an endpoint (OUT transfer)
 * @param endpont endpoint index (1-7)
 * @param data data to receive, must be in ram and 32 bit aligned
 * @param maxLength maximum length of data to receive
 * @param onReceived called when data was received
 */
bool receive(int endpoint, void *data, int maxLength, std::function<void (int)> const &onReceived);

} // namespace usb

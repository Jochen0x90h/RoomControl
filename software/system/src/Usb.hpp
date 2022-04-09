#pragma once

#include <usb.hpp>
#include <Coroutine.hpp>
#include <Data.hpp>
#include <cstdint>
#include <functional>


namespace Usb {

// Internal helper: Stores the parameters and a reference to the result value in the awaitable during co_await
struct ReceiveParameters {
	int &length;
	void *data;
};

// Internal helper: Stores the parameters in the awaitable during co_await
struct SendParameters {
	int length;
	void const *data;
};


/**
 * Initialize USB
 * @param getDescriptor callback for obtaining descriptors
 * @param onSetConfiguration callback for setting the configuration (libusb_set_configuration() on host), always called from event loop
 * @param onRequest callback for vendor specific request
 */
void init(
	std::function<Data (usb::DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t bConfigurationValue)> const &onSetConfiguration,
	std::function<bool (uint8_t bRequest, uint16_t wValue, uint16_t wIndex)> const &onRequest);

/**
 * Enable endpoints. Can be done in onSetConfiguration. Endpoint 0 should stay enabled
 * @param inFlags, an enabled flag for each in endpoint
 * @param outFlags, an enabled flag for each out endpoint
 */
void enableEndpoints(uint8_t inFlags, uint8_t outFlags);

/**
 * Suspend execution using co_await until data is received from an endpoint (IN transfer)
 * @param index endpoint index (1-7)
 * @param length length of data buffer
 * @param receivedLength number of bytes actually received
 * @param data data to receive, must be in ram and 32 bit aligned
 */
[[nodiscard]] Awaitable<ReceiveParameters> receive(int index, int &length, void *data);

/**
 * Suspend execution using co_await until data is sent over an endpoint (IN transfer)
 * @param index endpoint index (1-7)
 * @param length data length
 * @param data data to send, must be in ram and 32 bit aligned
 */
[[nodiscard]] Awaitable<SendParameters> send(int index, int length, void const *data);

} // namespace Usb

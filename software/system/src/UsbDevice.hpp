#pragma once

#include <usb.hpp>
#include <Coroutine.hpp>
#include <Data.hpp>
#include <cstdint>
#include <functional>


/**
 * Interface to an USB device
 *
 * https://www.beyondlogic.org/usbnutshell/usb1.shtml
 */
class UsbDevice {
public:

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


	virtual ~UsbDevice();

	/**
	 * Enable endpoints. Can be done in onSetConfiguration. Endpoint 0 should stay enabled
	 * @param inFlags an enabled flag for each in endpoint
	 * @param outFlags an enabled flag for each out endpoint
	 */
	virtual void enableEndpoints(uint8_t inFlags, uint8_t outFlags) = 0;

	/**
	 * Suspend execution using co_await until data is received from an endpoint (OUT transfer)
	 * @param index endpoint index (1-7)
	 * @param length in: length of data buffer, out: length of data actually received
	 * @param data data to receive, must be in RAM
	 */
	[[nodiscard]] virtual Awaitable<ReceiveParameters> receive(int index, int &length, void *data) = 0;

	/**
	 * Suspend execution using co_await until data is sent over an endpoint (IN transfer)
	 * @param index endpoint index (1-7)
	 * @param length data length
	 * @param data data to send, must be in RAM
	 */
	[[nodiscard]] virtual Awaitable<SendParameters> send(int index, int length, void const *data) = 0;

};

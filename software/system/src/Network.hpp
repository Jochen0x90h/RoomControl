#pragma once

#include <String.hpp>
#include <Coroutine.hpp>
#include <boardConfig.hpp>


namespace Network {

struct Endpoint {
	union {
		uint8_t u8[16];
		uint8_t u32[4];
	} address;
	uint16_t port;

	static Endpoint fromString(String s, uint16_t defaultPort);

	bool operator ==(Endpoint const &e) const {
		if (e.port != this->port)
			return false;
		for (int i = 0; i < 4; ++i) {
			if (e.address.u32[i] != this->address.u32[i])
				return false;
		}
		return true;
	}
};


// Internal helper: Stores the receive parameters and a reference to the result value in the awaitable during co_await
struct ReceiveParameters {
	Endpoint* source;
	int *length;
	void *data;
};

// Internal helper: Stores the send parameters in the awaitable during co_await
struct SendParameters {
	Endpoint const *destination;
	int length;
	void const *data;
};


/**
 * Initialize the radio. Depends on the random number generator, also call rng::init()
 */
void init();

/**
 * Set local port number
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param port local port number
 */
void setLocalPort(int index, uint16_t port);

/**
 * Receive data
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param source source of received data
 * @param length in: length of data buffer, out: number of bytes received
 * @param receivedLength number of bytes actually received
 * @param data data to receive, must be in ram
 * @return use co_await on return value to await received data
 */
Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data);

/**
 * Send data
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param destination destination of sent data
 * @param length data length
 * @param data data to send, must be in ram
 * @return use co_await on return value to await completion of send operation
 */
[[nodiscard]] Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data);
[[nodiscard]] inline Awaitable<SendParameters> send(int index, Endpoint const &destination, Array<uint8_t const> data) {
	return send(index, destination, data.count(), data.data());
}

} // namespace Network

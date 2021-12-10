#pragma once

#include "sysConfig.hpp"
#include <String.hpp>
#include <Coroutine.hpp>


namespace network {

struct Endpoint {

	static Endpoint fromString(String s, uint16_t defaultPort);

	uint8_t address[16];
	uint16_t port;
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
 * Suspend execution using co_await until data was received
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param source source of received data
 * @param length in: length of data buffer, out: number of bytes received
 * @param receivedLength number of bytes actually received
 * @param data data to receive, must be in ram
 */
Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data);

/**
 * Suspend execution using co_await until send is finished
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param destination destination of sent data
 * @param length data length
 * @param data data to send, must be in ram
 */
Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data);
inline Awaitable<SendParameters> send(int index, Endpoint const &destination, Array<uint8_t const> data) {
	return send(index, destination, data.length, data.data);
}

} // namespace network

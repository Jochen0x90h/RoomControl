#pragma once

#include <String.hpp>
#include <Coroutine.hpp>
#include <netinet/in.h>


namespace Network {

union Address {
	uint8_t u8[16];
	uint32_t u32[4];

	static Address fromString(String s);

	bool isLinkLocal() const {
		return this->u32[0] == htonl(0xfe800000U) && this->u32[1] == 0;
	}

	bool operator ==(Address const &a) const {
		for (int i = 0; i < 4; ++i) {
			if (a.u32[i] != this->u32[i])
				return false;
		}
		return true;
	}
};

struct Endpoint {
	Address address;
	uint16_t port;

	//static Endpoint fromString(String s, uint16_t defaultPort);

	bool operator ==(Endpoint const &e) const {
		return e.address == this->address && e.port == this->port;
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
 * Open a network connection on a local port
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param port local port number
 */
bool open(int index, uint16_t port);

/**
 * Join a multicast group
 * @param index context index (number of contexts defined by NETWORK_CONTEXT_COUNT in sysConfig.hpp)
 * @param multicastGroup
 * @return
 */
bool join(int index, Address const &multicastGroup);

/**
 * Close a network connection
 * @param index
 */
void close(int index);

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

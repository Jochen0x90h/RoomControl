#pragma once

#include <cstdint>
#include <Coroutine.hpp>


/*
	A node on the bus which is controlled by BusMaster.
	See BusMaster for description
*/
class BusNode {
public:
	struct ReceiveParameters {
		int *length;
		uint8_t *data;
	};

	struct SendParameters {
		int length;
		const uint8_t *data;
	};


	virtual ~BusNode();

	/**
	 * Receive data from bus master
	 * @param length in: maximum length of data to receive, out: actual length of data received
	 * @param data data to read
	 * @return use co_await on return value to await received data
	 */
	[[nodiscard]] virtual Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) = 0;

	/**
	 * Send data to bus master
	 * @param length length of data to write
	 * @param data data to write
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<SendParameters> send(int length, uint8_t const *data) = 0;

};
/*
namespace BusNode {

struct ReceiveParameters {
	int *receiveLength;
	uint8_t *receiveData;
};

struct SendParameters {
	int sendLength;
	uint8_t const *sendData;
};

/ **
 * Initialize the bus node
 * /
void init();

/ **
 * Receive data from bus master
 * @param length in: maximum length of data to receive, out: actual length of data received
 * @param data data to read
 * @return use co_await on return value to await received data
 * /
[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data);

/ **
 * Send data to bus master
 * @param length length of data to write
 * @param data data to write
 * @return use co_await on return value to await completion
 * /
[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data);

} // namespace BusNode
*/
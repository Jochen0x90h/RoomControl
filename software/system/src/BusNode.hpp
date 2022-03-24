#pragma once

#include <cstdint>
#include <Coroutine.hpp>


/*
	A node on the bus which is controlled by BusMaster.
	See BusMaster for description
*/
namespace BusNode {

struct SendParameters {
	// write data
	int writeLength;
	uint8_t const *writeData;
};

struct ReceiveParameters {
	// read data
	int *readLength;
	uint8_t *readData;
};

/**
 * Initialize the bus node
 */
void init();

/**
 * Receive data from bus master
 * @param length length of data to write
 * @param data data to write
 * @return use co_await on return value to await received data
 */
[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data);

/**
 * Send data to bus master
 * @param length length of data to write
 * @param data data to write
 * @return use co_await on return value to await completion
 */
[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data);

} // namespace BusNode

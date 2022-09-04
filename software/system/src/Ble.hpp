#pragma once

#include <String.hpp>
#include <Coroutine.hpp>
#include <netinet/in.h> // todo: implement own htonl


/*
	BLE (Bluetooth Low Energy) scanning and L2CAP layer
*/
namespace Ble {

struct Address {
	enum class Type : uint8_t {
		Public,
		Random,
	};

	uint8_t u8[6];
	Type type;

	//static Address fromString(String s);

	bool operator ==(Address const &a) const {
		for (int i = 0; i < 6; ++i) {
			if (this->u8[i] != a.u8[i])
				return false;
		}
		return this->type == a.type;
	}
};

union Uuid {
	uint8_t u8[16];
	uint16_t u16[8];
	uint32_t u32[4];

	/**
	 * Check if this is a 16 bit UUID which has the form 0000XXXX-0000-1000-8000-00805f9b34fb
	 */
	bool isShort() const {
		return u32[0] == u32L(0x5f9b34fb) && u32[1] == u32L(0x80000080)
			&& u32[2] == u32L(0x00001000) && u16[7] == 0;
	}

	uint8_t *setShort() {
		u32[0] = u32L(0x5f9b34fb);
		u32[1] = u32L(0x80000080);
		u32[2] = u32L(0x00001000);
		u16[7] = 0;
		return u8 + 12;
	}

	bool operator ==(Uuid const &b) const {
		for (int i = 0; i < 4; ++i) {
			if (this->u32[i] != b.u32[i])
				return false;
		}
		return true;
	}
};

struct ScanResult {
	Address address;
};

struct ScanParameters {
	ScanResult *result;
};

struct ReceiveParameters {
	int *length;
	uint8_t *data;
};

struct SendParameters {
	int length;
	uint8_t const *data;
};

/**
 * Initialize the BLE driver
 */
void init();

/**
 * Scan for BLE devices
 * @param result scan result
 * @return use co_await on return value to await a scan result
 */
[[nodiscard]] Awaitable<ScanParameters> scan(ScanResult &result);

/**
 * Open a connection to a Bluetooth device
 * @param index context index (number of contexts defined by BLUETOOTH_CONTEXT_COUNT in sysConfig.hpp)
 * @param address address of device
 */
bool open(int index, Address const &address);

/**
 * Close a connection to a Bluetooth device
 * @param index context index
 */
void close(int index);

/**
 * Receive data from a Bluetooth device
 * @param length in: maximum length of data to receive, out: actual length of data received
 * @param data data to receive
 * @return use co_await on return value to await received data
 */
[[nodiscard]] Awaitable<ReceiveParameters> receive(int index, int &length, uint8_t *data);

/**
 * Send data to a Bluetooth device
 * @param length length of data to send
 * @param data data to send
 * @return use co_await on return value to await completion
 */
[[nodiscard]] Awaitable<SendParameters> send(int index, int length, uint8_t const *data);

} // namespace Ble

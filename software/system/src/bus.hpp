#pragma once

#include <cstdint>
#include <Coroutine.hpp>
#include <enum.hpp>


/*
	The bus is based on UART (1 start bit, 8 data bits, 1 stop bit, 19200 bps) over LIN physical layer. This means that
	zeros are dominant and ones are recessive similar to CAN. Each device checks after each sent byte if the same byte
	was received. If not, it transfer is aborted as it is assumed that another device is sending at the same time.
	
	Protocol:
	The first part (before '|') is always sent by the master. The second part is sent by any node, simultaneous sending
	is resolved by bus arbitration. Messages are acknowledged by the receiver.
	BREAK SYNC 0 | <Command>
	BREAK SYNC | <Message> | <Ack>

	Command:
	A device can be set to commissioning mode e.g. by pressing a button or button combination for a longer time or by
	power cycle, depending on security requirements. All devices in commissioning mode will send an enumerate command,
	simultaneuous sending is resolved by bus arbitration. Commissioning command by master overrides all enumerate
	commands (by sending a zero) and causes the commissioned device to leave commissioning mode.
	Enumerate: <encoded device id> <list of endpoints> <mic(default key)>
	Commission: 0 <device id> <address> <key> <mic(default key)>

	Message:
	The message starts with the encoded device address for bus arbitration
	<encoded address> <security counter> <endpoint index> <message> <mic(key)>

	Ack:
	CRC-8 sent in response as acknowledgement


	<encoded device id>:
	Each byte encodes 3 bit of the device id as number of bits from 1 to 8, 11 bytes in total
	
	<address>:
	One byte unique address that gets assigned to a device during commissioning
	
	<encoded address>:
	Two bytes, first is ((address & 7) + 1), second is (address / 8) encoded as number of bits from 0 to 8
	
	<security counter>:
	Four bytes security counter
	
	<mic>
	Message integrity code using the default key or a configured network key	
*/
namespace bus {

enum class LevelControlCommand : uint8_t {
	// command
	SET = 0,
	INCREASE = 1,
	DECREASE = 2,
	COMMAND_MASK = 3,
	
	// time mode
	DURATION = 0x4,
	SPEED = 0x8,
	MODE_MASK = 0xc,
};
FLAGS_ENUM(LevelControlCommand);

struct Parameters {
	// write data
	int writeLength;
	uint8_t const *writeData;
	
	// read data
	int &readLength;
	uint8_t *readData;
};

/**
 * Initialize the bus
 */
void init();

/**
 * Transfer data to/from bus device. Use co_await to await end of transfer
 * @param writeLength length of data to write
 * @param writeData data to write, must be in ram, not in flash
 * @param readLength in: maximum length of data to read, out: actual length of data read
 * @param readData data to read
 */
Awaitable<Parameters> transfer(int writeLength, uint8_t const *writeData, int &readLength, uint8_t *readData);

/**
 * Wait for a request by a device to transfer data
 */
Awaitable<> request();

} // namespace system

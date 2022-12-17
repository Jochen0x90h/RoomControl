#pragma once

#include <cstdint>
#include <Coroutine.hpp>


/*
	The bus is based on UART (1 start bit, 8 data bits, 1 stop bit, 19200 bps) over LIN physical layer. This means that
	zeros are dominant and ones are recessive similar to CAN. Each device checks after each sent byte if the same byte
	was received. If not, the transfer is aborted as it is assumed that another device is sending at the same time. This
	means that the part of a message that is used for bus arbitration, for example the node address, needs to be encoded
	in a way that one sender "wins" without data corruption. A suitable encoding is to transfer 0 to 8 zero bits in an
	arbitration byte.
	
	Protocol:
	The master always starts by sending a BREAK which are 13 zero bits and a stop bit followed by SYNC which is 0x55.
	Then a command or message follows which is sent by the master or a node. A message is acknowledged by the receiver.
 	If a node wants to send a command or a message, it sends a single bit (pulls the bus low for one bit time) to wake
 	up the master which then starts by sending a BREAK.
	BREAK SYNC | <Command>
	BREAK SYNC | <Message> | <Ack>

 	BREAK:
 	13 zero bits followed by one stop bit

 	SYNC:
 	0x55

	<Command>:
	A device can be set to commissioning mode e.g. by pressing a button or button combination for a longer time or by
	power cycle, depending on security requirements. All devices in commissioning mode will send an enumerate command,
	simultaneous sending is resolved by bus arbitration. Commissioning command by master overrides all enumerate
	commands (by sending a leading zero) and causes the commissioned device to leave commissioning mode.
	Enumerate: 0 <Encoded Device Id> <Protocol Version> <Endpoint Count> <MIC(default key)>
	Commission: 0 0 <Device Id> <Address> <Key> <MIC(default key)>

	<Message>:
	The message starts with the encoded device address for bus arbitration (always loses against commands as it never
	starts with zero) and contains either an attribute read request, attribute data or a plug message
	Attribute Read: <Encoded Address> <Security Counter> <255> <Endpoint Index> <Attribute Index> <MIC(key)>
	Attribute Data: <Encoded Address> <Security Counter> <255> <Endpoint Index> <Attribute Index> <Data> <MIC(key)>
	Plug Message: <Encoded Address> <Security Counter> <Endpoint Index> <Plug Index> <Message> <MIC(key)>

	<Ack> (not implemented yet):
	CRC-8 of Command or Message (not including sync) sent in response as acknowledgement

	<Encoded Device Id>:
	Each byte encodes 3 bit of the device id as number of bits from 1 to 8, starting with lowest bits, 11 bytes in total
	
	<Address>:
	One byte unique address that gets assigned to a device during commissioning
	
	<Encoded Address>:
	Two bytes, first is ((address & 7) + 1) and therefore never 0 to to be different from commands,
	second is (address / 8) encoded as number of bits from 0 to 8
	
	<Security Counter>:
	Four bytes security counter
	
	<MIC(key)>
	Message integrity code using the default key or a configured network key	
*/
class BusMaster {
public:
	struct ReceiveParameters {
		int *length;
		uint8_t *data;
	};

	struct SendParameters {
		int length;
		const uint8_t *data;
	};


	virtual ~BusMaster();

	/**
	 * Receive data from a bus node
	 * @param length in: maximum length of data to receive, out: actual length of data received
	 * @param data data to receive
	 * @return use co_await on return value to await received data
	 */
	[[nodiscard]] virtual Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) = 0;

	/**
	 * Send data to a bus node
	 * @param length length of data to send
	 * @param data data to send
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<SendParameters> send(int length, uint8_t const *data) = 0;
};

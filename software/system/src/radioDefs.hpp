#pragma once

#include "enum.hpp"
#include <cstdint>


namespace radio {

// radio context flags (packets except for PASS_ALL must have a sequence number)
enum class ContextFlags : uint16_t {
	NONE = 0,
	
	/**
	 * Pass all packets to receive handler of the context (even when SEQUENCE_NUMBER_SUPPRESSION is on)
	 */
	PASS_ALL = 1,

	/**
	 * Pass beacon packets
	 */
	PASS_TYPE_BEACON = 2,

	/**
	 * Pass packets when destination pan and short address match or are broadcast
	 */
	PASS_DEST_SHORT = 4,

	/**
		Pass data packets when destination pan and short address match or are broadcast (self-powered devices such
		as EnOcean switches)
	*/
	PASS_TYPE_DATA_DEST_SHORT = 8,

	/**
		Pass packets where destination pan and long address match or are broadcast
	*/
	PASS_DEST_LONG = 16,

	/**
	 * Handle ack for own packets in radio driver to meet turnaround time.
	 * For received packets, an ACK is sent automatically (Make sure the destination address filter is set).
	 * For sent packets, success is reported after ACK was received. When ACK is not received, resend is attempted
	 * 2 more times
	 */
	HANDLE_ACK = 32,
};
FLAGS_ENUM(ContextFlags)


// enum for remote controlling the radio, e.g. via usb
enum class Request : uint8_t {
	RESET = 0,
	START = 1,
	STOP = 2,
	ENABLE_RECEIVER = 3,
	SET_LONG_ADDRESS = 4,
	SET_FLAGS = 5,
	SET_PAN = 6,
	SET_SHORT_ADDRESS = 7
};
/*
// result of energy detection and send for remote controlling, e.g. via usb
namespace Result {
	// wait for result to be available, only internal use
	constexpr uint8_t WAITING = 0;
	
	// maximum value of energy detection, range is [0, 127]
	constexpr uint8_t MAX_ED_VALUE = 127;
	
	// start of result values (0: failure, 1: success with one backoff, 2: success with two backoffs...)
	constexpr uint8_t MIN_RESULT_VALUE = 128;
}
*/
} // namespace radio

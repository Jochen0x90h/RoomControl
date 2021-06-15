#pragma once

#include "enum.hpp"
#include <cstdint>


namespace radio {

// radio context flags (packets except for PASS_ALL must have a sequence number)
enum class ContextFlags : uint16_t {
	NONE = 0,
	
	// pass all packets to receive handler of the context (even when SEQUENCE_NUMBER_SUPPRESSION is on)
	PASS_ALL = 1,

	// pass beacon packets
	PASS_TYPE_BEACON = 2,

	// pass packets when destination pan and short address match or are broadcast
	PASS_DEST_SHORT = 4,

	// pass data packets when destination pan and short address match or are broadcast (green power)
	PASS_TYPE_DATA_DEST_SHORT = 8,

	// packets pass where destination pan and long address match or are broadcast
	PASS_DEST_LONG = 16,

	// handle ack for own packets in radio driver to meet turnaround time. Make sure the destination address filter is set
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

// result of energy detection and send for remote controlling, e.g. via usb
namespace Result {
	// wait for result to be available, only internal use
	constexpr uint8_t WAITING = 0;
	
	// maximum value of energy detection, range is [0, 127]
	constexpr uint8_t MAX_ED_VALUE = 127;
	
	// start of result values (0: failure, 1: success with one backoff, 2: success with two backoffs...)
	constexpr uint8_t MIN_RESULT_VALUE = 128;
}

} // namespace radio

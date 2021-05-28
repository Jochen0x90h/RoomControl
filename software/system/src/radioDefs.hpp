#pragma once

#include <cstdint>


namespace radio {

// filter flags (filtered packets except for ALL must have a sequence number)

// all packets pass
constexpr int ALL = 1;

// beacon packets pass
constexpr int TYPE_BEACON = 2;

// packets pass when destination pan and short address match or are broadcast
constexpr int DEST_SHORT = 4;

// data packets pass when destination pan and short address match or are broadcast (green power)
constexpr int TYPE_DATA_DEST_SHORT = 8;

// packets pass where destination pan and long address match or are broadcast
constexpr int DEST_LONG = 16;

// handle ack for own packets in radio driver to meet turnaround time. Make sure the destination address filter is set
constexpr int HANDLE_ACK = 32;


// ieee 802.15.4 frame type
enum class FrameType {
	BEACON = 0,
	DATA = 1,
	ACK = 2,
	COMMAND = 3
};

// ieee 802.15.4 addressing mode
enum class AddressingMode {
	NONE = 0,
	SHORT = 2,
	LONG = 3
};

// ieee 802.15.4 command
enum class Command {
	ASSOCIATION_REQUEST = 1,
	ASSOCIATION_RESPONSE = 2,
	DATA_REQUEST = 4,
	BEACON_REQUEST = 7,
};


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
	
	// send was successful
	constexpr uint8_t SUCCESS = 128;
	
	// send failed
	constexpr uint8_t FAILURE = 255;
}

} // namespace radio

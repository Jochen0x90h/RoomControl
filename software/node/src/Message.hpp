#pragma once

#include <cstdint>
#include "bus.hpp"


/**
 * Reuse endpoint type for now
 */
 /*
using MessageType = bus::EndpointType;

inline bool isInput(MessageType type) {
	return (type & MessageType::DIRECTION_MASK) == MessageType::IN;
}

inline bool isOutput(MessageType type) {
	return (type & MessageType::DIRECTION_MASK) == MessageType::OUT;
}

// number of switch commands
constexpr int SWITCH_COMMAND_COUNT = 5;


struct Message {
	// command
	uint8_t command;

	// transition speed or rate for MOVE_TO_... messages
	uint16_t transition;

	union {
		int32_t i;
		float f;
	} value;
};


// type and source or destination "address" of a message
struct MessageInfo {
	// message type
	MessageType type = MessageType::UNKNOWN;

	union {
		// source or destination interface device
		struct {
			uint8_t id;
			uint8_t endpointIndex;
		} device;

		// source or destination mqtt topic
		struct {
			uint16_t id;
		} topic;

		// source or destination function plug
		struct {
			uint8_t id;
			uint8_t connectionIndex;
		} plug;
	};
};
*/

/**
* Reuse endpoint type for now
*/
using MessageType2 = bus::EndpointType2;

inline bool isInput(MessageType2 type) {
	return (type & MessageType2::DIRECTION_MASK) == MessageType2::IN;
}

inline bool isOutput(MessageType2 type) {
	return (type & MessageType2::DIRECTION_MASK) == MessageType2::OUT;
}

inline bool isBinaryOneShot(MessageType2 type) {
	return (type & MessageType2::SWITCH_BINARY_CATEGORY) == bus::EndpointType2::SWITCH_BINARY_ONESHOT;
}

inline bool isTernaryOneShot(MessageType2 type) {
	return (type & MessageType2::SWITCH_TERNARY_CATEGORY) == bus::EndpointType2::SWITCH_TERNARY_ONESHOT;
}

union Value {
	uint8_t u8;
	uint32_t i32;
	float f;
};

struct Message2 {
	Value value;
	uint8_t command;
	uint16_t transition;
};


// type and source or destination "address" of a message
struct MessageInfo2 {
	// message type
	MessageType2 type = MessageType2(0);

	union {
		// source or destination interface device
		struct {
			uint8_t id;
			uint8_t endpointIndex;
		} device;

		// source or destination mqtt topic
		struct {
			uint16_t id;
		} topic;

		// source or destination function plug
		struct {
			uint8_t id;
			uint8_t connectionIndex;
		} plug;
	};
};


struct ConvertOptions {
	static constexpr int MAX_VALUE_COUNT = 3;

	// mapping from input command to output command in a connection
	uint16_t commands;
	uint16_t transition;
	union {
		float f[MAX_VALUE_COUNT];
	} value;

	int getCommand(int index) const {return (this->commands >> index * 3) & 7;}
};

/*
bool isCompatible(MessageType dstType, MessageType srcType);

bool convertCommand(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertFloatValue(MessageType dstType, Message &dst, MessageType srcType, float src, ConvertOptions const &convertOptions);
bool convertSetFloatValue(MessageType dstType, Message &dst, MessageType srcType, Message const &src, ConvertOptions const &convertOptions);

// get name of message type
String getTypeName(MessageType messageType);
*/

// get human readable label of message type
String getTypeLabel(MessageType2 type);

// get usage
enum class Usage : uint8_t {
	ACTIVATE,
	AMPERE,
	ATMOSPHERIC_PRESSURE,
	COLOR_TEMPERATURE,
	ENABLE,
	FEEZER_TEMPERATURE,
	FRIDGE_TEMPERATURE,
	HIGH_VOLTAGE,
	KELVIN,
	LOW_VOLTAGE,
	MAINS_VOLTAGE,
	NONE,
	OCCUPANCY,
	OFF_ON,
	OFF_ON2,
	OPEN_CLOSE,
	OUTDOOR_TEMPERATURE,
	OVEN_TEMPERATURE,
	PASCAL,
	PERCENT,
	PERCENTAGE,
	RELEASE_PRESS,
	ROOM_TEMPERATURE,
	STOP_OPEN_CLOSE,
	STOP_START,
	UNIT_INTERVAL,
	UP_DOWN,
	VOLT,
	WATT,
	WATT_HOURS,
};
Usage getUsage(MessageType2 type);

bool isCompatible(MessageType2 dstType, MessageType2 srcType);

bool convertSwitch(MessageType2 dstType, Message2 &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertFloat(MessageType2 dstType, Message2 &dst, float src, ConvertOptions const &convertOptions);
bool convertFloatCommand(MessageType2 dstType, Message2 &dst, float src, uint8_t command, ConvertOptions const &convertOptions);
bool convertFloatTransition(MessageType2 dstType, Message2 &dst, float src, uint8_t command, uint16_t transition, ConvertOptions const &convertOptions);

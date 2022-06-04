#pragma once

#include <cstdint>
#include "bus.hpp"


/**
 * Reuse endpoint type for now
 */
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


// source or destination "address" of a message
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


bool isCompatible(MessageType dstType, MessageType srcType);

bool convertCommand(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertFloatValue(MessageType dstType, Message &dst, MessageType srcType, float src, ConvertOptions const &convertOptions);
bool convertSetFloatValue(MessageType dstType, Message &dst, MessageType srcType, Message const &src, ConvertOptions const &convertOptions);

// get name of message type
String getTypeName(MessageType messageType);

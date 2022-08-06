#pragma once

#include <cstdint>
#include "bus.hpp"


/**
* Reuse endpoint type for now
*/
using MessageType = bus::PlugType;

inline bool isInput(MessageType type) {
	return (type & MessageType::DIRECTION_MASK) == MessageType::IN;
}

inline bool isOutput(MessageType type) {
	return (type & MessageType::DIRECTION_MASK) == MessageType::OUT;
}

union Value {
	uint8_t u8;
	uint16_t u16;
	float f32;
};

struct Message {
	Value value;
	uint8_t command;
	uint16_t transition;
};


// type and source or destination "address" of a message
struct MessageInfo {
	// message type
	MessageType type = MessageType(0);

	union {
		// source or destination interface device
		struct {
			uint8_t id;
			uint8_t plugIndex;
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


// get human readable label of message type
String getTypeLabel(MessageType type);

#include "Usage.generated.hpp"

// get usage
Usage getUsage(MessageType type);

bool isCompatible(MessageType dstType, MessageType srcType);

bool convertSwitch(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertFloat(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertFloatCommand(MessageType dstType, Message &dst, float src, uint8_t command, ConvertOptions const &convertOptions);
bool convertFloatTransition(MessageType dstType, Message &dst, float src, uint8_t command, uint16_t transition, ConvertOptions const &convertOptions);

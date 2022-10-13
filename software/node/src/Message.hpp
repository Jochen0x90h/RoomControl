#pragma once

#include "bus.hpp"
#include "Coroutine.hpp"
#include <cstdint>


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
	int8_t i8;
	uint8_t u8;
	uint16_t u16;
	float f32;
};

struct Message {
	Value value;
	uint8_t command;
	uint16_t transition;
};

/**
 * Info for the receiver of a message
 */
/*struct MessageInfo {
	// subscribers: connection index, listeners: device index
	uint8_t sourceIndex;

	// subscribers: receiver device id, listeners: sender device id
	uint8_t deviceId;

	// subscribers: receiver plug index, listeners: sender plug index
	uint8_t plugIndex;

	// subscribers: connection index
	uint8_t connectionIndex;

	// message type
	MessageType type;
};

struct MessageParameters {
public:
	// info about the message source for the subscriber to identify the message
	MessageInfo &info;

	// message (length is defined by the message type)
	void *message;
};

using MessageBarrier = Barrier<MessageParameters>;
*/

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
bool convertInt8(MessageType dstType, Message &dst, int src, ConvertOptions const &convertOptions);
bool convertFloat(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertFloatCommand(MessageType dstType, Message &dst, float src, uint8_t command, ConvertOptions const &convertOptions);
bool convertFloatTransition(MessageType dstType, Message &dst, float src, uint8_t command, uint16_t transition, ConvertOptions const &convertOptions);

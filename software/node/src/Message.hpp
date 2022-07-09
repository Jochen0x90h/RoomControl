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

union Value {
	uint8_t u8;
	uint32_t i32;
	float f;
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

enum class Usage : uint8_t {
	ACTIVATION,
	AMPERE,
	CLOSED_OPEN,
	CLOSED_OPEN_TOGGLE,
	COUNTER,
	ELECTRIC_METER,
	ENABLED,
	LOCK,
	LOCK_TOGGLE,
	LUX,
	NONE,
	OCCUPANCY,
	OFF_ON,
	OFF_ON1_ON2,
	OFF_ON_TOGGLE,
	PASCAL,
	PERCENT,
	PRESSURE_ATMOSPHERIC,
	RELEASED_PRESSED,
	RELEASED_UP_DOWN,
	STOPPED_OPENING_CLOSING,
	TEMPERATURE,
	TEMPERATURE_COLOR,
	TEMPERATURE_FEEZER,
	TEMPERATURE_FREEZER,
	TEMPERATURE_FRIDGE,
	TEMPERATURE_OUTDOOR,
	TEMPERATURE_OVEN,
	TEMPERATURE_ROOM,
	TILT_LOCK,
	UNIT_INTERVAL,
	VOLTAGE,
	VOLTAGE_HIGH,
	VOLTAGE_LOW,
	VOLTAGE_MAINS,
	WATT,
};

// get usage
Usage getUsage(MessageType type);

bool isCompatible(MessageType dstType, MessageType srcType);

bool convertSwitch(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertFloat(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertFloatCommand(MessageType dstType, Message &dst, float src, uint8_t command, ConvertOptions const &convertOptions);
bool convertFloatTransition(MessageType dstType, Message &dst, float src, uint8_t command, uint16_t transition, ConvertOptions const &convertOptions);

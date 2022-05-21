#pragma once

#include <cstdint>
#include "bus.hpp"


using MessageType = bus::EndpointType;


struct IndexedCommand {
	uint8_t command;
	uint8_t index;
};

struct FloatWithFlag {
	uint32_t value;
	
	FloatWithFlag() = default;
	
	FloatWithFlag(float value) : value(*reinterpret_cast<uint32_t *>(&value) & ~1) {}

	operator float() const {
		uint32_t value = this->value & ~1;
		return *reinterpret_cast<float *>(&value);
	}
	
	FloatWithFlag &operator =(float value) {
		this->value = *reinterpret_cast<uint32_t *>(&value) & ~1;
		return *this;
	}
	
	bool getFlag() const {
		return (this->value & 1) != 0;
	}
	
	void set(float value, bool flag) {
		this->value = (*reinterpret_cast<uint32_t *>(&value) & ~1) | int(flag);
	}

	bool isPositive() const {
		return (this->value & 0x80000000) == 0;
	}
	bool isNegative() const {
		return (this->value & 0x80000000) != 0;
	}

	FloatWithFlag operator -() const {
		FloatWithFlag ff;
		ff.value = this->value ^ 0x80000000;
		return ff;
	}
};

inline float operator +(FloatWithFlag const &a, float b) {
	return float(a) + b;
}

union ConvertOptions {
	int32_t i32;
	uint32_t u32;
	float f;
	FloatWithFlag ff;
};

struct MoveToLevelMessage {
	// flag false: absolute level (text command e.g. "0.5"), true: relative level (text command e.g. "+ 0.1" "- +0.1")
	FloatWithFlag level;

	// flag false: 0: move duration in seconds, true: move speed in 1/s
	FloatWithFlag move;
};

union Message {
	uint8_t command; // on/off, up/down etc.
	IndexedCommand indexedCommand;
	FloatWithFlag level; // flag false: absolute level, true: relative level
	MoveToLevelMessage moveToLevel;
	float temperature;
	FloatWithFlag setTemperature; // flag false: absolute temperature, true: relative temperature
	float pressure;
	float airHumidity;
	float airVoc;

	// use this to copy the message
	uint32_t data[2];
};


bool isCompatibleIn(MessageType dstType, MessageType srcType);

bool convertCommandIn(MessageType dstType, Message &dst, MessageType srcType, uint8_t src, ConvertOptions const &convertOptions);
bool convertTemperatureIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertPressureIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertAirHumidityIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);
bool convertAirVocIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions);


bool isCompatibleOut(bus::EndpointType dstType, MessageType srcType);

bool convertCommandOut(uint8_t &dst, MessageType srcType, Message const &src, ConvertOptions const &convertOptions);

// get name of message type
String getTypeName(MessageType messageType);

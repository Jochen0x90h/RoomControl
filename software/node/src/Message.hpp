#pragma once

#include <cstdint>


enum class MessageType : uint8_t {
	UNKNOWN,

	// on/off, 0: off, 1: on, 2(!): toggle
	// conversions:
	// ON_OFF -> ON_OFF2 : off -> on, on -> off
	// TRIGGER -> ON_OFF : active -> toggle
	// TRIGGER -> ON_OFF2 : active -> on, inactive -> off
	// UP_DOWN -> ON_OFF : up -> off, down -> on
	// UP_DOWN -> ON_OFF2 : up -> on, down -> off
	ON_OFF,
	ON_OFF2,

	// trigger, 0(#): inactive, 1(!): active
	// conversions:
	// UP_DOWN -> TRIGGER : up -> active
	// UP_DOWN -> TRIGGER2 : down -> active
	TRIGGER,
	TRIGGER2,
	
	// up/down, 0(#): inactive, 1(+): up, 2(-): down
	// conversions:
	// TRIGGER -> UP_DOWN : press -> up
	// TRIGGER -> UP_DOWN2 : press -> down
	// UP_DOWN -> UP_DOWN2 : up -> down, down -> up
	UP_DOWN,
	UP_DOWN2,

	// level as float in range [0.0, 1.0]
	LEVEL,
	MOVE_TO_LEVEL,

	// temperature
	CELSIUS,
	FAHRENHEIT,
	
	// air pressure
	HECTOPASCAL,

	// air humidity
	PERCENTAGE,

	// voc
	OHM
};


struct FloatWithFlag {
	uint32_t value;
	
	FloatWithFlag() = default;
	
	FloatWithFlag(float value) : value(*reinterpret_cast<uint32_t *>(&value) & ~1) {
	}
	
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
};

inline float operator +(FloatWithFlag const &a, float b) {
	return float(a) + b;
}

struct MoveToLevelMessage {
	// flag false: absolute level (text command e.g. "0.5"), true: relative level (text command e.g. "+ 0.1" "- +0.1")
	FloatWithFlag level;

	// flag false: 0: move duration in seconds, true: move rate in 1/s
	FloatWithFlag move;
};

union Message {
	uint8_t onOff;
	uint8_t trigger;
	uint8_t upDown;
	FloatWithFlag level; // flag false: absolute level, true: relative level
	MoveToLevelMessage moveToLevel;
	FloatWithFlag temperature; // flag false: absolute temperature, true: relative temperature
	float airPressure;
	float resistance;

	// use this to copy the message
	uint32_t data[2];
};

bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage);

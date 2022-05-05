#pragma once

#include <cstdint>


enum class MessageType : uint8_t {
	UNKNOWN,

	// on/off: 0("0"): off, 1("1"): on, 2("!"): toggle
	// conversions:
	// ON_OFF_IN -> ON_OFF : identity
	// TRIGGER_IN -> ON_OFF : activate -> toggle
	// UP_DOWN_IN -> ON_OFF : up -> off, down -> on
	// ON_OFF -> ON_OFF_OUT : identity
	// ON_OFF -> TRIGGER_OUT : off -> activate
	// ON_OFF -> UP_DOWN_OUT : off -> up, on -> down
	ON_OFF,

	// ON_OFF_IN -> ON_OFF2 : inverse
	// UP_DOWN_IN -> ON_OFF2 : up -> on, down -> off
	// ON_OFF2 -> ON_OFF_OUT : inverse
	// ON_OFF2 -> TRIGGER_OUT : on -> activate
	// ON_OFF2 -> UP_DOWN_OUT : off -> down, on -> up
	ON_OFF2,


	// trigger, 0(#): inactive, 1(!): activate
	// conversions:
	// TRIGGER_IN -> TRIGGER : identity
	// UP_DOWN_IN -> TRIGGER : inactive -> inactive, up -> activate, down -> (discard)
	// TRIGGER -> TRIGGER_OUT : identity
	// TRIGGER -> UP_DOWN_OUT : inactive -> inactive, activate -> up
	TRIGGER,

	// TRIGGER_IN -> TRIGGER2 : identity
	// UP_DOWN_IN -> TRIGGER2 : inactive -> inactive, up -> (discard), down -> activate
	// TRIGGER -> TRIGGER_OUT : identity
	// TRIGGER -> UP_DOWN_OUT : inactive -> inactive, activate -> down
	TRIGGER2,

	// trigger, up/down: 0("#"): inactive, 1("+"): up, 2("-"): down
	// conversions:
	// ON_OFF_IN -> UP_DOWN : off -> up, on -> down
	// TRIGGER_IN -> UP_DOWN : inactive -> inactive, activate -> up
	// UP_DOWN_IN -> UP_DOWN : identity
	// UP_DOWN -> ON_OFF_OUT : up -> off, down -> on
	// UP_DOWN -> TRIGGER_OUT : inactive -> inactive, up -> activate, down -> (discard)
	// UP_DOWN -> UP_DOWN_OUT : identity
	UP_DOWN,

	// ON_OFF_IN -> UP_DOWN2 : off -> down, on -> up
	// TRIGGER_IN -> UP_DOWN2 : inactive -> inactive, activate -> down
	// UP_DOWN_IN -> UP_DOWN2 : inverse
	// UP_DOWN2 -> ON_OFF_OUT : up -> on, down -> off
	// UP_DOWN2 -> TRIGGER_OUT : inactive -> inactive, up -> (discard), down -> activate
	// UP_DOWN2 -> UP_DOWN_OUT : inverse
	UP_DOWN2,

	// level as float in range [0.0, 1.0]
	LEVEL,
	MOVE_TO_LEVEL,

	// temperature (Kelvin)
	TEMPERATURE,
	//CELSIUS,
	//FAHRENHEIT,
	
	// pressure (Pascal)
	PRESSURE,
	//HECTOPASCAL,

	// air humidity (percentage)
	AIR_HUMIDITY,

	// voc (Ohm)
	AIR_VOC
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
	float pressure;
	float airHumidity;
	float airVoc;

	// use this to copy the message
	uint32_t data[2];
};

bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage);

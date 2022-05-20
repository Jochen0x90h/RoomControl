#pragma once

#include <cstdint>


enum class MessageType : uint8_t {
	UNKNOWN,

	// command: 0("0"): off, 1("1"): on, 2("!"): trigger/toggle, 3(#): release, 4("+"): up, 5("-"): down
	COMMAND,
	INDEXED_COMMAND,

	// on/off: 0("0"): off, 1("1"): on, 2("!"): toggle
	ON_OFF,
	ON_OFF1 = ON_OFF,
	ON_OFF2,
	ON_OFF3,
	ON_OFF4,

	// trigger, 0(#): inactive, 1(!): activate
	TRIGGER,
	TRIGGER1 = TRIGGER,
	TRIGGER2,

	// trigger, up/down: 0("#"): inactive, 1("+"): up, 2("-"): down
	UP_DOWN,
	UP_DOWN1 = UP_DOWN,
	UP_DOWN2,

	// level as float in range [0.0, 1.0]
	LEVEL,
	MOVE_TO_LEVEL,

	// temperature (Kelvin)
	TEMPERATURE,
	SET_TEMPERATURE, // set value for temperature
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

enum class Command : uint8_t {
	// on/off switch
	OFF = 0,
	ON = 1,
	TOGGLE = 2,

	// button
	TRIGGER = 2,

	// rocker
	UP = 3,
	DOWN = 4,

	// button or rocker returns to default position
	RELEASE = 5,
};
constexpr uint8_t flag(Command command) {return 1 << int(command);}
constexpr bool hasFlag(uint8_t flags, Command command) {return (flags & (1 << int(command))) != 0;}
constexpr int selectCommand(uint8_t flags, Command command) {
	return hasFlag(flags, Command::RELEASE) ? int(Command::RELEASE) : 7;
}
constexpr int selectCommand(uint8_t flags, Command command, Command command2) {
	return hasFlag(flags, command) ? int(command) :
		(hasFlag(flags, command2) ? int(command2) : 7);
}

struct IndexedCommand {
	Command command;
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
	Command command;
	IndexedCommand indexedCommand;
	uint8_t onOff;
	uint8_t trigger;
	uint8_t upDown;
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

bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage);



bool convertOnOffIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertTriggerIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);
bool convertUpDownIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions);


/*
// ON_OFF_IN -> ON_OFF1 : identity
// ON_OFF_IN -> ON_OFF2 : inverse
// ON_OFF_IN -> ON_OFF3 : on -> toggle, toggle -> on
// ON_OFF_IN -> ON_OFF4 : off -> toggle, toggle -> off
// TRIGGER_IN -> ON_OFF1 : activate -> toggle
// TRIGGER_IN -> ON_OFF2 : activate -> toggle
// TRIGGER_IN -> ON_OFF3 : activate -> on
// TRIGGER_IN -> ON_OFF4 : activate -> off
// UP_DOWN_IN -> ON_OFF1 : up -> on, down -> off
// UP_DOWN_IN -> ON_OFF2 : up -> off, down -> on
// UP_DOWN_IN -> ON_OFF3 : up -> toggle
// UP_DOWN_IN -> ON_OFF4 : down -> toggle

// ON_OFF_IN -> TRIGGER1 : off -> inactive, on -> activate, toggle -> activate
// ON_OFF_IN -> TRIGGER2 : off -> activate, on -> inactive, toggle -> activate
// TRIGGER_IN -> TRIGGER1 : identity
// TRIGGER_IN -> TRIGGER2 : identity
// UP_DOWN_IN -> TRIGGER1 : inactive -> inactive, up -> activate
// UP_DOWN_IN -> TRIGGER2 : inactive -> inactive, down -> activate

// ON_OFF_IN -> UP_DOWN1 : off -> inactive, on -> up, toggle -> up
// ON_OFF_IN -> UP_DOWN2 : off -> inactive, on -> down, toggle -> down
// TRIGGER_IN -> UP_DOWN1 : inactive -> inactive, activate -> up
// TRIGGER_IN -> UP_DOWN2 : inactive -> inactive, activate -> down
// UP_DOWN_IN -> UP_DOWN1 : identity
// UP_DOWN_IN -> UP_DOWN2 : inverse


// ON_OFF_IN -> ON_OFF1 : identity
// ON_OFF_IN -> ON_OFF2 : inverse
// ON_OFF_IN -> ON_OFF3 : on -> toggle, toggle -> on
// ON_OFF_IN -> ON_OFF4 : off -> toggle, toggle -> off
// ON_OFF_IN -> TRIGGER1 : off -> inactive, on -> activate, toggle -> activate
// ON_OFF_IN -> TRIGGER2 : off -> activate, on -> inactive, toggle -> activate
// ON_OFF_IN -> UP_DOWN1 : off -> inactive, on -> up, toggle -> up
// ON_OFF_IN -> UP_DOWN2 : off -> inactive, on -> down, toggle -> down
bool convertOnOffIn(MessageType dstType, Message &dst, uint8_t src);

// TRIGGER_IN -> ON_OFF1 : activate -> toggle
// TRIGGER_IN -> ON_OFF2 : activate -> toggle
// TRIGGER_IN -> ON_OFF3 : activate -> on
// TRIGGER_IN -> ON_OFF4 : activate -> off
// TRIGGER_IN -> TRIGGER1 : identity
// TRIGGER_IN -> TRIGGER2 : identity
// TRIGGER_IN -> UP_DOWN1 : inactive -> inactive, activate -> up
// TRIGGER_IN -> UP_DOWN2 : inactive -> inactive, activate -> down
bool convertTriggerIn(MessageType dstType, Message &dst, uint8_t src);

// UP_DOWN_IN -> ON_OFF1 : up -> on, down -> off
// UP_DOWN_IN -> ON_OFF2 : up -> off, down -> on
// UP_DOWN_IN -> ON_OFF3 : up -> toggle
// UP_DOWN_IN -> ON_OFF4 : down -> toggle
// UP_DOWN_IN -> TRIGGER1 : inactive -> inactive, up -> activate
// UP_DOWN_IN -> TRIGGER2 : inactive -> inactive, down -> activate
// UP_DOWN_IN -> UP_DOWN1 : identity
// UP_DOWN_IN -> UP_DOWN2 : inverse
bool convertUpDownIn(MessageType dstType, Message &dst, uint8_t src);
*/

// ON_OFF -> ON_OFF_OUT : identity
// ON_OFF2 -> ON_OFF_OUT : inverse
// UP_DOWN -> ON_OFF_OUT : inactive -> off, up -> on, down -> (discard)
// UP_DOWN2 -> ON_OFF_OUT : inactive -> off, down -> on, up -> (discard)
bool convertOnOffOut(uint8_t &dst, MessageType srcType, Message const &src);

// ON_OFF -> TRIGGER_OUT : off -> activate
// ON_OFF2 -> TRIGGER_OUT : on -> activate
// TRIGGER -> TRIGGER_OUT : identity
// UP_DOWN -> TRIGGER_OUT : inactive -> inactive, up -> activate, down -> (discard)
// UP_DOWN2 -> TRIGGER_OUT : inactive -> inactive, up -> (discard), down -> activate
bool convertTriggerOut(uint8_t &dst, MessageType srcType, Message const &src);

// ON_OFF -> UP_DOWN_OUT : off -> up, on -> down
// ON_OFF2 -> UP_DOWN_OUT : off -> down, on -> up
// TRIGGER -> UP_DOWN_OUT : inactive -> inactive, activate -> up
// TRIGGER2 -> UP_DOWN_OUT : inactive -> inactive, activate -> down
// UP_DOWN -> UP_DOWN_OUT : identity
// UP_DOWN2 -> UP_DOWN_OUT : inverse
bool convertUpDownOut(uint8_t &dst, MessageType srcType, Message const &src);

// mode: inverse alternate

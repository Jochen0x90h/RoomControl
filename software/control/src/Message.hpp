#pragma once

#include <cstdint>


enum class MessageType : uint8_t {
	UNKNOWN,

	// on/off, 0: off, 1: on, 2: toggle
	// conversions:
	// ON_OFF -> ON_OFF2 : off -> on, on -> off
	// TRIGGER -> ON_OFF : active -> toggle
	// TRIGGER -> ON_OFF2 : active -> on, inactive -> off
	// UP_DOWN -> ON_OFF : up -> off, down -> on
	// UP_DOWN -> ON_OFF2 : up -> on, down -> off
	ON_OFF,
	ON_OFF2,

	// trigger, 0: inactive, 1: active
	// conversions:
	// UP_DOWN -> TRIGGER : up -> active
	// UP_DOWN -> TRIGGER2 : down -> active
	TRIGGER,
	TRIGGER2,
	
	// up/down, 0: inactive, 1: up, 2: down
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
};

bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage);



/**
 * Unit and data type
 */
 /*
enum class UnitType : uint8_t {
	UNKNOWN,

	// binary, 0: off, 1: on, 2: toggle
	// conversions:
	// SWITCH -> SWITCH2 : off -> on, on -> off
	// TRIGGER -> SWITCH : press -> toggle
	// TRIGGER -> SWITCH2 : press -> on, release -> off
	// UP_DOWN -> SWITCH : up -> off, down -> on
	// UP_DOWN -> SWITCH2 : up -> on, down -> off
	SWITCH_U8,
	SWITCH2_U8,

	// button, 0: release, 1: press
	// conversions:
	// UP_DOWN -> TRIGGER : up -> press
	// UP_DOWN -> TRIGGER2 : down -> press
	TRIGGER_U8,
	TRIGGER2_U8,
	
	// rocker, 0: release, 1: up, 2: down
	// conversions:
	// TRIGGER -> UP_DOWN : press -> up
	// TRIGGER -> UP_DOWN2 : press -> down
	// UP_DOWN -> UP_DOWN2 : up -> down, down -> up
	UP_DOWN_U8,
	UP_DOWN2_U8,

	// level stored as float in range [0.0, 1.0]
	LEVEL_F32,

	// temperature
	CELSIUS_F32,
	FAHRENHEIT_F32,
	
	// air pressure
	HECTOPASCAL_F32,
	
	// voc
	OHM_F32
};
*/
/**
 * Variant containing all data types for UnitType
 */
/*union Variant {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	float f32;
};

bool convert(UnitType dstType, void *dstData, UnitType srcType, void const *srcData);

*/
/**
 * Data type
 */
 /*
enum class DataType : uint8_t {
	UNKNOWN,
	
	// unsigned integer, 8 bit
	U8,

	// unsigned integer, 16 bit
	U16,

	// unsigned integer, 32 bit
	U32,

	// float, 32 bit
	F32,
};

inline DataType getDataType(UnitType unitType) {
	switch (unitType) {
	case UnitType::UNKNOWN:
		break;
	case UnitType::SWITCH_U8:
	case UnitType::SWITCH2_U8:
	case UnitType::TRIGGER_U8:
	case UnitType::TRIGGER2_U8:
	case UnitType::UP_DOWN_U8:
	case UnitType::UP_DOWN2_U8:
		return DataType::U8;
	case UnitType::LEVEL_F32:
	case UnitType::CELSIUS_F32:
	case UnitType::FAHRENHEIT_F32:
	case UnitType::HECTOPASCAL_F32:
		return DataType::F32;
	}
	return DataType::UNKNOWN;
}
*/

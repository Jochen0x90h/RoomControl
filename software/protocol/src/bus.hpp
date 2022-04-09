#pragma once

#include "MessageReader.hpp"
#include "MessageWriter.hpp"
#include <enum.hpp>
#include <cstdint>


namespace bus {

extern uint8_t const defaultKey[16];
extern AesKey const defaultAesKey;


/**
 * Type of device endpoint such as button, relay or temperature sensor
 */
 enum class EndpointType : uint8_t {
	// if a device contains multiple sub-devices, a separator can be used to structure the endpoints
	//SEPARATOR = 0,
	
	// discrete types
	// --------------
	
	// on/off switch with two stable states and toggle command (0: off, 1: on, 2: toggle)
	ON_OFF = 0x01,
	ON_OFF_IN = 0x81,
	ON_OFF_OUT = 0x01,

	// trigger (button, motion detector), returns to inactive state (0: inactive, 1: active)
	TRIGGER = 0x02,
	TRIGGER_IN = 0x82,
	TRIGGER_OUT = 0x02,

	// up/down (rocker, blind), returns to inactive state when not pressed (0: inactive, 1: up, 2: down)
	UP_DOWN = 0x03,
	UP_DOWN_IN = 0x83,
	UP_DOWN_OUT = 0x03,


	// level, color etc.
	// -----------------
	
	// level in percent (brightness of a light bulb)
	LEVEL = 0x10,
	LEVEL_IN = 0x90,
	LEVEL_OUT = 0x10,

	// color
	COLOR_IN = 0x91,
	COLOR_OUT = 0x11,
	

	// environment
	// -----------
	
	// temperature (Celsius, Fahrenheit, Kelvin)
	TEMPERATURE = 0x20,
	TEMPERATURE_IN = 0xa0,
	TEMPERATURE_OUT = 0x20,

	// air pressure (hectopascal)
	AIR_PRESSURE = 0x21,
	AIR_PRESSURE_IN = 0xa1,

	// air humidity (percent)
	AIR_HUMIDITY = 0x22,
	AIR_HUMIDITY_IN = 0xa2,

	// air volatile organic components
	AIR_VOC = 0x23,
	AIR_VOC_IN = 0xa3,

	// illuminance (lux, https://en.wikipedia.org/wiki/Lux)
	ILLUMINANCE = 0x24,
	ILLUMINANCE_IN = 0xa4,

	
	// electrical energy
	// -----------------
	
	// voltage (V, mV)
	VOLTAGE = 0x30,
	VOLTAGE_IN = 0xb0,
	
	// current (A, mA)
	CURRENT = 0x31,
	CURRENT_IN = 0xb1,

	// battery level (percent)
	BATTERY_LEVEL = 0x32,
	BATTERY_LEVEL_IN = 0xb2,
	
	// energy counter (kWh, Wh)
	ENERGY_COUNTER = 0x33,
	ENERGY_COUNTER_IN = 0xb3,
	
	// energy (W, mW)
	POWER = 0x34,
	POWER_IN = 0xb4,

	
	TYPE_MASK = 0x7f,
	
	IN = 0x80,
	OUT = 0x00,
	DIRECTION_MASK = 0x80
};
FLAGS_ENUM(EndpointType);



enum class LevelControlCommand : uint8_t {
	// command
	SET = 0,
	INCREASE = 1,
	DECREASE = 2,
	COMMAND_MASK = 3,
	
	// time mode
	DURATION = 0x4,
	SPEED = 0x8,
	MODE_MASK = 0xc,
};
FLAGS_ENUM(LevelControlCommand);




class MessageReader : public DecryptReader {
public:
	MessageReader(int length, uint8_t *data) : DecryptReader(length, data) {}

	/**
	 * Read a value from 0 to 8 from bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 * @return value
	 */
	uint8_t arbiter();

	/**
	 * Read an encoded device id
	 * @return device id
	 */
	uint32_t id();
};

class MessageWriter : public EncryptWriter {
public:
	template <int N>
	MessageWriter(uint8_t (&message)[N]) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + N)
#endif
	{}

	MessageWriter(int length, uint8_t *message) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + length)
#endif
	{}

	/**
	 * Write a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 * @param value value in range 0 to 8 to write
	 */
	void arbiter(uint8_t value) {
		u8(~(0xff >> value));
	}

	/**
	 * Write an encoded device id
	 * @param id device id to encode
	 */
	void id(uint32_t id);

	int getLength() {
		int length = this->current - this->begin;
#ifdef EMU
		assert(this->current <= this->end);
#endif
		return length;
	}

	uint8_t *begin;
#ifdef EMU
	uint8_t *end;
#endif
};

} // namespace bus

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
	TYPE_MASK = 0x3f,
	DIRECTION_MASK = 0xc0,

	// direction relative to device, i.e. IN is device input, OUT is device output (can be subscribed to)
	IN = 0x80,
	OUT = 0x40,

	// endpoint types
	// --------------
	UNKNOWN = 0,

	// off/on switch with two stable states (0: off, 1: on)
	OFF_ON = 0x01,
	OFF_ON_IN = OFF_ON | IN,
	OFF_ON_OUT = OFF_ON | OUT,

	// off/on switch with two stable states and toggle command (0: off, 1: on, 2: toggle)
	OFF_ON_TOGGLE = 0x02,
	OFF_ON_TOGGLE_IN = OFF_ON_TOGGLE | IN,
	OFF_ON_TOGGLE_OUT = OFF_ON_TOGGLE | OUT,

	// trigger (button, motion detector), returns to inactive state when released (0: release, 1: trigger)
	TRIGGER = 0x03,
	TRIGGER_IN = TRIGGER | IN,
	TRIGGER_OUT = TRIGGER | OUT,

	// up/down (rocker, blind), returns to inactive state when released (0: release, 1: up, 2: down)
	UP_DOWN = 0x04,
	UP_DOWN_IN = UP_DOWN | IN,
	UP_DOWN_OUT = UP_DOWN | OUT,

	// open/close (window) (0: open, 1: close)
	OPEN_CLOSE = 0x05,
	OPEN_CLOSE_IN = OPEN_CLOSE | IN,
	OPEN_CLOSE_OUT = OPEN_CLOSE | OUT,


	// level, color etc.
	// -----------------
	
	// measured level (percent) level in percent (e.g position of blind)
	LEVEL = 0x10,
	LEVEL_IN = LEVEL | IN,
	LEVEL_OUT = LEVEL | OUT,

	// set level (e.g. brightness of a light bulb)
	SET_LEVEL = 0x11,
	SET_LEVEL_IN = SET_LEVEL | IN,
	SET_LEVEL_OUT = SET_LEVEL | OUT,

	// set level with duration (e.g. brightness of a light bulb)
	MOVE_TO_LEVEL = 0x12,
	MOVE_TO_LEVEL_IN = MOVE_TO_LEVEL | IN,
	MOVE_TO_LEVEL_OUT = MOVE_TO_LEVEL | OUT,

	// color
	COLOR = 0x13,
	COLOR_IN = COLOR | IN,
	COLOR_OUT = COLOR | OUT,

	// set color
	SET_COLOR = 0x14,
	SET_COLOR_IN = SET_COLOR | IN,
	SET_COLOR_OUT = SET_COLOR | OUT,

	// color temperature
	COLOR_TEMPERATURE = 0x16,
	COLOR_TEMPERATURE_IN = COLOR | IN,
	COLOR_TEMPERATURE_OUT = COLOR | OUT,

	// set color temperature
	SET_COLOR_TEMPERATURE = 0x17,
	SET_COLOR_TEMPERATURE_IN = SET_COLOR | IN,
	SET_COLOR_TEMPERATURE_OUT = SET_COLOR | OUT,


	// environment
	// -----------
	
	// measured air temperature (1/20 Kelvin, displayed in Celsius or Fahrenheit dependent on user setting)
	AIR_TEMPERATURE = 0x20,
	AIR_TEMPERATURE_IN = AIR_TEMPERATURE | IN,
	AIR_TEMPERATURE_OUT = AIR_TEMPERATURE | OUT,

	// set temperature (absolute/relative)
	SET_AIR_TEMPERATURE = 0x21,
	SET_AIR_TEMPERATURE_IN = SET_AIR_TEMPERATURE | IN,
	SET_AIR_TEMPERATURE_OUT = SET_AIR_TEMPERATURE | OUT,

	// air humidity (percent)
	AIR_HUMIDITY = 0x22,
	AIR_HUMIDITY_IN = AIR_HUMIDITY | IN,
	AIR_HUMIDITY_OUT = AIR_HUMIDITY | OUT,

	// set air humidity (percent)
	SET_AIR_HUMIDITY = 0x23,
	SET_AIR_HUMIDITY_IN = SET_AIR_HUMIDITY | IN,
	SET_AIR_HUMIDITY_OUT = SET_AIR_HUMIDITY | OUT,

	// air pressure (Pascal, displayed in hectopascal, kilopascal or megapascal dependent on user setting)
	AIR_PRESSURE = 0x24,
	AIR_PRESSURE_IN = AIR_PRESSURE | IN,
	AIR_PRESSURE_OUT = AIR_PRESSURE | OUT,

	// air volatile organic components
	AIR_VOC = 0x25,
	AIR_VOC_IN = AIR_VOC | IN,
	AIR_VOC_OUT = AIR_VOC | OUT,

	// illuminance (lux, https://en.wikipedia.org/wiki/Lux)
	ILLUMINANCE = 0x26,
	ILLUMINANCE_IN = ILLUMINANCE | IN,
	ILLUMINANCE_OUT = ILLUMINANCE | OUT,

	
	// electrical energy
	// -----------------
	
	// voltage (V, mV)
	VOLTAGE = 0x30,
	VOLTAGE_IN = VOLTAGE | IN,
	VOLTAGE_OUT = VOLTAGE | OUT,

	// current (A, mA)
	CURRENT = 0x31,
	CURRENT_IN = CURRENT | IN,
	CURRENT_OUT = CURRENT | OUT,

	// battery level (percent)
	BATTERY_LEVEL = 0x32,
	BATTERY_LEVEL_IN = BATTERY_LEVEL | IN,
	BATTERY_LEVEL_OUT = BATTERY_LEVEL | OUT,

	// energy counter (kWh, Wh)
	ENERGY_COUNTER = 0x33,
	ENERGY_COUNTER_IN = ENERGY_COUNTER | IN,
	ENERGY_COUNTER_OUT = ENERGY_COUNTER | OUT,

	// energy (W, mW)
	POWER = 0x34,
	POWER_IN = POWER | IN,
	POWER_OUT = POWER | OUT,
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

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

	// direction relative to bus master, i.e. IN is host input and device output
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
	
	// level in percent (brightness of a light bulb, position of blind), optionally with duration/speed
	LEVEL = 0x10,
	LEVEL_IN = LEVEL | IN,
	LEVEL_OUT = LEVEL | OUT,

	MOVE_TO_LEVEL = 0x11,
	MOVE_TO_LEVEL_OUT = MOVE_TO_LEVEL | OUT,

	// color
	COLOR = 0x12,
	COLOR_IN = COLOR | IN,
	COLOR_OUT = COLOR | OUT,
	

	// environment
	// -----------
	
	// temperature (1/20 Kelvin)
	TEMPERATURE = 0x20,
	TEMPERATURE_IN = TEMPERATURE | IN,
	TEMPERATURE_OUT = TEMPERATURE | OUT,

	// set temperature (absolute/relative)
	SET_TEMPERATURE = 0x21,
	SET_TEMPERATURE_IN = SET_TEMPERATURE | IN,
	SET_TEMPERATURE_OUT = SET_TEMPERATURE | OUT,

	// pressure (pascal)
	PRESSURE = 0x22,
	PRESSURE_IN = PRESSURE | IN,

	// air humidity (percent)
	AIR_HUMIDITY = 0x23,
	AIR_HUMIDITY_IN = AIR_HUMIDITY | IN,

	// air volatile organic components
	AIR_VOC = 0x24,
	AIR_VOC_IN = AIR_VOC | IN,

	// illuminance (lux, https://en.wikipedia.org/wiki/Lux)
	ILLUMINANCE = 0x25,
	ILLUMINANCE_IN = ILLUMINANCE | IN,

	
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

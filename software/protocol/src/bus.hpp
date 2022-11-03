#pragma once

#include "MessageReader.hpp"
#include "MessageWriter.hpp"
#include <enum.hpp>
#include <cstdint>


namespace bus {

extern uint8_t const defaultKey[16];
extern AesKey const defaultAesKey;


enum class Attribute : uint8_t {
	HW_VERSION = 0,
	MANUFACTURER_NAME = 1,
	MODEL_IDENTIFIER = 2,
	DATE_CODE = 3,
	POWER_SOURCE = 4,
	PLUG_LIST = 5,
};


#include "PlugType.generated.hpp"


/*
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
*/



class MessageReader : public DecryptReader {
public:
	MessageReader(int length, uint8_t *data) : DecryptReader(length, data) {}

	/**
	 * READ a value from 0 to 8 from bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 * @return value
	 */
	uint8_t arbiter();

	/**
	 * READ an encoded device id
	 * @return device id
	 */
	uint32_t id();

	/**
	 * READ an encoded address
	 * @return address
	 */
	uint8_t address() {
		auto a1 = arbiter();
		auto a2 = arbiter();
		return (a1 - 1) | (a2 << 3);
	}
};

class MessageWriter : public EncryptWriter {
public:
	template <int N>
	MessageWriter(uint8_t (&message)[N]) : EncryptWriter(message)
#ifdef DEBUG
		, end(message + N)
#endif
	{}

	MessageWriter(int length, uint8_t *message) : EncryptWriter(message)
#ifdef DEBUG
		, end(message + length)
#endif
	{}

	/**
	 * WRITE a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 * @param value value in range 0 to 8 to write
	 */
	void arbiter(uint8_t value) {
		u8(~(0xff >> value));
	}

	/**
	 * WRITE an encoded device id
	 * @param id device id to encode
	 */
	void id(uint32_t id);

	/**
	 * WRITE an ecoded address
	 * @return
	 */
	void address(uint8_t address) {
		arbiter((address & 7) + 1);
		arbiter(address >> 3);
	}

	/**
	 * Get number of bytes written so far
	 * @return current length
	 */
	int getLength() {
		int length = int(this->current - this->begin);
#ifdef DEBUG
		assert(this->current <= this->end);
#endif
		return length;
	}

#ifdef DEBUG
	uint8_t *end;
#endif
};

} // namespace bus

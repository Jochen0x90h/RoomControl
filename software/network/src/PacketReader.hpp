#pragma once

#include <enum.hpp>
#include <cstdint>


// helper class for receiving data
class PacketReader {
public:
	/**
	 * Construct from radio packet where the length (including 2 byte crc) is in the first byte
	 */
	PacketReader(uint8_t *packet) : current(packet + 1), end(packet + 1 + packet[0] - 2) {}
	
	/**
	 * Construct from data buffer
	 */
	PacketReader(uint8_t *data, uint8_t *end) : current(data), end(end) {}

	uint8_t int8() {
		uint8_t value = this->current[0];
		++this->current;
		return value;
	}

	uint8_t peekInt8() {
		return this->current[0];
	}
	
	template <typename T>
	T enum8() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		T value = T(this->current[0]);
		++this->current;
		return value;
	}

	template <typename T>
	T peekEnum8() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		return T(this->current[0]);
	}

	uint16_t int16() {
		int16_t value = this->current[0] | (this->current[1] << 8);
		this->current += 2;
		return value;
	}

	template <typename T>
	T enum16() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		return T(int16());
	}

	uint32_t int32() {
		uint32_t value = this->current[0] | (this->current[1] << 8) | (this->current[2] << 16) | (this->current[3] << 24);
		this->current += 4;
		return value;
	}

	uint64_t int64() {
		uint64_t lo = int32();
		return lo | (uint64_t(int32()) << 32);
	}

	void skip(int n) {
		this->current += n;
	}

	int getRemaining() {
		return this->end - this->current;
	}

	bool atEnd() {
		return this->current >= this->end;
	}


	uint8_t *current;
	uint8_t *end;
};

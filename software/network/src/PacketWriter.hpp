#pragma once

#include <enum.hpp>
#include <cstdint>


// helper class for sending data
class PacketWriter {
public:
	PacketWriter(uint8_t *packet) : packet(packet), current(packet + 1) {}
		
	void int8(uint8_t value) {
		this->current[0] = value;
		++this->current;
	}
	
	template <typename T>
	void enum8(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		this->current[0] = uint8_t(value);
		++this->current;
	}
	
	void int16(uint16_t value) {
		this->current[0] = value;
		this->current[1] = value >> 8;
		this->current += 2;
	}

	template <typename T>
	void enum16(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		int16(uint16_t(value));
	}

	void int32(uint32_t value) {
		this->current[0] = value;
		this->current[1] = value >> 8;
		this->current[2] = value >> 16;
		this->current[3] = value >> 24;
		this->current += 4;
	}

	void int64(uint64_t value) {
		int32(value);
		int32(value >> 32);
	}

	void skip(int n) {
		this->current += n;
	}

	// write length to first byte of buffer (number of bytes written + 2 for crc)
	uint8_t const *finish() {
		this->packet[0] = this->current - (this->packet + 1) + 2; // for crc
		return this->packet;
	}

	uint8_t *packet;
	uint8_t *current;
};

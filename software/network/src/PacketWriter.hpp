#pragma once

#include "DataBuffer.hpp"
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

	template <int N>
	void data(DataBuffer<N> const &d) {
		for (int i = 0; i < N; ++i)
			this->current[i] = d.data[i];
		this->current += N;
	}

	void skip(int n) {
		this->current += n;
	}
/*
	void setLength() {
		this->packet[0] = this->current - (this->packet + 1) + 2; // for crc
	}
*/
	/**
	 * Set send flags and lenght of packet
	 */
	void finish(radio::SendFlags sendFlags) {
		*this->current = uint8_t(sendFlags);
		this->packet[0] = this->current - (this->packet + 1) + 2; // for crc
	}

	uint8_t *packet;
	uint8_t *current;
};

#pragma once

#include <cstdint>


template <int L>
class DataBuffer {
public:

	void setInt8(int index, uint8_t value) {
		this->data[index] = value;
	}
	
	void setBigEndianInt16(int index, uint16_t value) {
		this->data[index + 0] = value >> 8;
		this->data[index + 1] = value;
	}

	void xorBigEndianInt16(int index, uint16_t value) {
		this->data[index + 0] ^= value >> 8;
		this->data[index + 1] ^= value;
	}

	void setLittleEndianInt32(int index, uint32_t value) {
		this->data[index + 0] = value;
		this->data[index + 1] = value >> 8;
		this->data[index + 2] = value >> 16;
		this->data[index + 3] = value >> 24;
	}

	void setData(int index, uint8_t const *data, int length) {
		int l = min(length, L - index);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it = *data;
	}

	void xorData(int index, uint8_t const *data, int length) {
		int l = min(length, L - index);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void pad(int index) {
		int l = L - index;
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it)
			*it = 0;
	}

	uint8_t operator[] (int index) const {return this->data[index];}

	uint8_t data[L];
};

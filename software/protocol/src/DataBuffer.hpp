#pragma once

#include <util.hpp>
#include <assert.hpp>
#include <Array.hpp>
#include <cstdint>



template <int N>
class DataBuffer {
public:

	void setU8(int index, uint8_t value) {
		this->data[index] = value;
	}
	
	void setU16B(int index, uint16_t value) {
		this->data[index + 0] = value >> 8;
		this->data[index + 1] = value;
	}

	void xorU16B(int index, uint16_t value) {
		this->data[index + 0] ^= value >> 8;
		this->data[index + 1] ^= value;
	}

	void setU32L(int index, uint32_t value) {
		this->data[index + 0] = value;
		this->data[index + 1] = value >> 8;
		this->data[index + 2] = value >> 16;
		this->data[index + 3] = value >> 24;
	}

	void setU64L(int index, uint64_t value) {
		setU32L(index, uint32_t(value));
		setU32L(index + 4, uint32_t(value >> 32));
	}

	template <int M>
	void setData(int index, DataBuffer<M> const &buffer) {
		int l = min(N - index, M);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data;
		for (; it < end; ++it, ++data)
			*it = *data;
	}

	template <int M>
	void setData(int index, Array<uint8_t const, M> buffer) {
		int l = min(N - index, M);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data();
		for (; it < end; ++it, ++data)
			*it = *data;
	}
	
	void setData(int index, int length, uint8_t const *data) {
		int l = min(N - index, length);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it = *data;
	}
	
	template <int M>
	void xorData(int index, DataBuffer<M> const &buffer) {
		int l = min(N - index, M);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	template <int M>
	void xorData(int index, Array<uint8_t const, M> buffer) {
		int l = min(N - index, M);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data();
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}
	
	void xorData(int index, int length, uint8_t const *data) {
		int l = min(N - index, length);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void fill(uint8_t value) {
		uint8_t *it = this->data;
		uint8_t *end = it + N;
		for (; it < end; ++it)
			*it = value;
	}

	/**
	 * Pad with zeros from index to end of buffer
	 * @param index index where to start padding
	 * @param value padding value
	 */
	void pad(int index, uint8_t value = 0) {
		int l = N - index;
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it)
			*it = value;
	}

	uint8_t &operator[] (int index) {
		assert(index >= 0 && index < N);
		return this->data[index];
	}

	operator Array<uint8_t, N>() {return Array<uint8_t, N>(this->data);}
	operator Array<uint8_t const, N>() const {return Array<uint8_t const, N>(this->data);}

	template <int M>
	Array<uint8_t, M> array(int index) {
		assert(index >= 0 && index + M <= N);
		return Array<uint8_t, M>(this->data + index);
	}
	
	uint8_t *begin() {return this->data;}
	uint8_t *end() {return this->data + N;}

	uint8_t data[N];
};

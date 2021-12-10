#pragma once

#include <util.hpp>
#include <cstdint>
#include <assert.hpp>

template <int N>
class DataBufferRef;

template <int N>
class DataBufferConstRef;

template <int N>
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

	void setLittleEndianInt64(int index, uint64_t value) {
		setLittleEndianInt32(index, uint32_t(value));
		setLittleEndianInt32(index + 4, uint32_t(value >> 32));
	}

	void setData(int index, uint8_t const *data, int length) {
		int l = min(N - index, length);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it = *data;
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

	void xorData(int index, uint8_t const *data, int length) {
		int l = min(N - index, length);
		uint8_t *it = this->data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void xorData(DataBufferConstRef<N> buffer) {
		uint8_t const *data = buffer.data;
		uint8_t *it = this->data;
		uint8_t *end = it + N;
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

	uint8_t operator[] (int index) const {
		assert(index >= 0 && index < N);
		return this->data[index];
	}

	template <int M>
	DataBufferRef<M> getRef(int index) {
		assert(index >= 0 && index + M <= N);
		return {this->data + index};
	}

	uint8_t *begin() {return this->data;}
	uint8_t *end() {return this->data + N;}

	uint8_t data[N];
};

template <int N>
class DataBufferRef {
public:

	DataBufferRef(uint8_t *data) : data(data) {}
	
	DataBufferRef(DataBuffer<N> &buffer) : data(buffer.data) {}

	void xorData(DataBufferConstRef<N> buffer) {
		uint8_t const *data = buffer.data;
		uint8_t *it = this->data;
		uint8_t *end = it + N;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void fill(uint8_t value) {
		uint8_t *it = this->data;
		uint8_t *end = it + N;
		for (; it < end; ++it)
			*it = value;
	}

	uint8_t operator[] (int index) const {
		assert(index >= 0 && index < N);
		return this->data[index];
	}

	uint8_t *data;
};
/*
template <int N, typename T>
struct CheckLength {};
  
template <int N, typename T, int M>
struct CheckLength<N, T[M]>  {
	static_assert(M >= N);
};
*/

template <int N>
class DataBufferConstRef {
public:

	DataBufferConstRef(uint8_t const *data) : data(data) {}

	DataBufferConstRef(DataBuffer<N> const &buffer) : data(buffer.data) {}
	
	DataBufferConstRef(DataBufferRef<N> buffer) : data(buffer.data) {}
/*
	template<typename T>
	constexpr DataBufferConstRef(T &array)
		: data(array)
	{
		CheckLength<N, array>();
	}
*/

	uint8_t operator[] (int index) const {
		assert(index >= 0 && index < N);
		return this->data[index];
	}

	uint8_t const *data;
};

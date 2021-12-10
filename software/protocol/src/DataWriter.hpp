#pragma once

#include "DataBuffer.hpp"
#include "crypt.hpp"
#include "Nonce.hpp"
#include <enum.hpp>
#include <cstdint>



/**
 * Helper class for constructing structured data for messages, radio packets and the like
 */
class DataWriter {
public:
	DataWriter(uint8_t *data) : current(data) {}
		
	void uint8(uint8_t value) {
		this->current[0] = value;
		++this->current;
	}
	
	template <typename T>
	void enum8(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		this->current[0] = uint8_t(value);
		++this->current;
	}
	
	void uint16(uint16_t value) {
		this->current[0] = value;
		this->current[1] = value >> 8;
		this->current += 2;
	}

	template <typename T>
	void enum16(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		uint16(uint16_t(value));
	}

	void uint32(uint32_t value) {
		this->current[0] = value;
		this->current[1] = value >> 8;
		this->current[2] = value >> 16;
		this->current[3] = value >> 24;
		this->current += 4;
	}

	void uint64(uint64_t value) {
		uint32(value);
		uint32(value >> 32);
	}

	/**
	 * Add the contents of a data buffer
	 */
	template <int N>
	void data(DataBuffer<N> const &buffer) {
		for (int i = 0; i < N; ++i)
			this->current[i] = buffer.data[i];
		this->current += N;
	}
	
	/**
	 * Add the contents of an array, cast each element to uint8_t
	 */
	template <typename T>
	void data(Array<T> array) {
		for (int i = 0; i < array.length; ++i)
			this->current[i] = uint8_t(array.data[i]);
		this->current += array.length;
	}

	/**
	 * Add string contents without length 
	 */
	void data(String str) {
		for (int i = 0; i < str.length; ++i)
			this->current[i] = uint8_t(str.data[i]);
		this->current += str.length;
	}

	/**
	 * Skip bytes (does not modify the skipped bytes)
	 */
	void skip(int n) {
		this->current += n;
	}


	// current position in data
	uint8_t *current;
};


class EncryptWriter : public DataWriter {
public:
	EncryptWriter(uint8_t *data) : DataWriter(data) {}

	/**
	 * Set start of header at current position ("string a" for encryption)
	 */
	void setHeader() {this->header = this->current;}

	/**
	 * Set start of message at current position
	 */
	void setMessage() {this->message = this->current;}

	/**
	 * Encrypt in-place, call after message has been written
	 */
	void encrypt(int micLength, Nonce const &nonce, AesKey const &aesKey) {
		auto header = this->header;
		auto message = this->message;
		int headerLength = message - header;
		int payloadLength = this->current - message;
		skip(micLength);

		// encrypt in-place
		::encrypt(message, nonce, header, headerLength, message, payloadLength, micLength, aesKey);
	}
	

	uint8_t const *header;
	uint8_t *message;
};

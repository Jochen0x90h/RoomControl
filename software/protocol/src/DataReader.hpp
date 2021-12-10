#pragma once

#include "crypt.hpp"
#include "Nonce.hpp"
#include <enum.hpp>
#include <cstdint>


/**
 * Helper class for reading structured data from messages, radio packets and the like
 */
class DataReader {
public:

	/**
	 * Constructor. Data is not const to support in-place decryption
	 * @param data data to read from
	 */
	DataReader(uint8_t *data) : current(data) {}

	uint8_t uint8() {
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

	uint16_t uint16() {
		int16_t value = this->current[0] | (this->current[1] << 8);
		this->current += 2;
		return value;
	}

	template <typename T>
	T enum16() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		return T(uint16());
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


	uint8_t *current;
};


class DecryptReader : public DataReader {
public:
	DecryptReader(int length, uint8_t *data) : DataReader(data), end(data + length) {}

	/**
	 * Set start of header at current position ("string a" for encryption)
	 */
	void setHeader() {this->header = this->current;}

	/**
	 * Set start of header at given position ("string a" for encryption)
	 */
	void setHeader(uint8_t const *header) {this->header = header;}

	/**
	 * Set start of message at current position
	 */
	void setMessage() {this->message = this->current;}

	/**
	 * Set start of message at given position
	 */
	void setMessage(uint8_t *message) {this->message = message;}

	/**
	 * Decrypt in-place, call after header has been read
	 */
	bool decrypt(int micLength, Nonce const &nonce, AesKey const &aesKey) {
		auto header = this->header;
		auto message = this->message;
		int headerLength = message - header;
		int payloadLength = this->end - micLength - message;
		
		// cut off message integrity code
		this->end -= micLength;
		
		// encrypt in-place
		return ::decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, aesKey);
	}

	/**
	 * Get remaining number of bytes in the message
	 */
	int getRemaining() {
		return this->end - this->current;
	}


	uint8_t *end;
	uint8_t const *header;
	uint8_t *message;
};

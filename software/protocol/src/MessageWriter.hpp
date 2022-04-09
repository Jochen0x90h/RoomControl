#pragma once

#include "DataBuffer.hpp"
#include "crypt.hpp"
#include "Nonce.hpp"
#include <Array.hpp>
#include <String.hpp>
#include <cstdint>


struct MessageWriter {
	uint8_t *current;

	explicit MessageWriter(uint8_t *message) : current(message) {}

	void u8(uint8_t value) {
		this->current[0] = value;
		++this->current;
	}

	template <typename T>
	void e8(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		this->current[0] = uint8_t(value);
		++this->current;
	}

	void u16L(uint16_t value) {
		auto current = this->current;
		current[0] = value;
		current[1] = value >> 8;
		this->current += 2;
	}

	void u16B(uint16_t value) {
		auto current = this->current;
		current[0] = value >> 8;
		current[1] = value;
		this->current += 2;
	}

	template <typename T>
	void e16L(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		u16L(uint16_t(value));
	}

	template <typename T>
	void e16B(T value) {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		u16B(uint16_t(value));
	}

	void u32L(uint32_t value) {
		auto current = this->current;
		current[0] = value;
		current[1] = value >> 8;
		current[2] = value >> 16;
		current[3] = value >> 24;
		this->current += 4;
	}

	void u32B(uint32_t value) {
		auto current = this->current;
		current[0] = value >> 24;
		current[1] = value >> 16;
		current[2] = value >> 8;
		current[3] = value;
		this->current += 4;
	}

	void u64L(uint64_t value) {
		u32L(value);
		u32L(value >> 32);
	}

	void u64B(uint64_t value) {
		u32B(value >> 32);
		u32B(value);
	}

	/**
	 * Add the contents of a data buffer
	 */
	template <int N>
	void data(DataBuffer<N> const &buffer) {
		auto current = this->current;
		for (int i = 0; i < N; ++i)
			current[i] = buffer.data[i];
		this->current += N;
	}

	/**
	 * Add the contents of an array, cast each element to uint8_t
	 */
	template <typename T, int N>
	void data(Array<T, N> array) {
		auto current = this->current;
		for (int i = 0; i < array.count(); ++i)
			current[i] = uint8_t(array[i]);
		this->current += array.count();
	}

	/**
	 * Add the contents of an array, cast each element to uint8_t
	 */
	template <typename T, int N>
	void data(T (&array)[N]) {
		auto current = this->current;
		for (int i = 0; i < N; ++i)
			current[i] = uint8_t(array[i]);
		this->current += N;
	}

	void data(int length, uint8_t const *data) {
		auto current = this->current;
		for (int i = 0; i < length; ++i)
			current[i] = data[i];
		this->current += length;
	}

	/**
	 * Add string contents without length
	 */
	void string(String const &str) {
		auto current = this->current;
		for (int i = 0; i < str.length; ++i)
			current[i] = uint8_t(str.data[i]);
		this->current += str.length;
	}

	/**
	 * Skip bytes (does not modify the skipped bytes)
	 */
	void skip(int n) {
		this->current += n;
	}

	// fulfill stream concept
	MessageWriter &operator <<(char ch) {u8(ch); return *this;}
	MessageWriter &operator <<(String const &str) {string(str); return *this;}

};


class EncryptWriter : public MessageWriter {
public:

	/**
	 * Constructor. Sets start of header at current position ("string a" for encryption)
	 */
	explicit EncryptWriter(uint8_t *message) : MessageWriter(message) {}

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
	 * Encrypt in-place, call after message has been written
	 */
	void encrypt(int micLength, Nonce const &nonce, AesKey const &aesKey) {
		auto header = this->header;
		auto message = this->message;
		assert(header != nullptr);
		assert(message != nullptr);
		int headerLength = message - header;
		int payloadLength = this->current - message;
		this->skip(micLength);

		// encrypt in-place
		::encrypt(message, header, headerLength, message, payloadLength, micLength, nonce, aesKey);
	}


	uint8_t const *header = nullptr;
	uint8_t *message = nullptr;
};

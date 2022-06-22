#pragma once

#include "crypt.hpp"
#include "Nonce.hpp"
#include <String.hpp>
#include <cstdint>


class MessageReader {
public:

	MessageReader() = default;
	MessageReader(uint8_t *message, uint8_t *end) : current(message), end(end) {}
	MessageReader(int length, uint8_t *message) : current(message), end(message + length) {}

	uint8_t u8() {
		uint8_t value = this->current[0];
		++this->current;
		return value;
	}

	uint8_t peekU8() const {
		return this->current[0];
	}

	template <typename T>
	T e8() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		T value = T(this->current[0]);
		++this->current;
		return value;
	}

	template <typename T>
	T peekE8() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
		return T(this->current[0]);
	}

	uint16_t u16L() {
		auto current = this->current;
		int16_t value = current[0] | (current[1] << 8);
		this->current += 2;
		return value;
	}

	uint16_t u16B() {
		auto current = this->current;
		int16_t value = (current[0] << 8) | current[1];
		this->current += 2;
		return value;
	}

	template <typename T>
	T e16L() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		return T(u16L());
	}

	template <typename T>
	T e16B() {
		static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
		return T(u16B());
	}

	uint32_t u32L() {
		auto current = this->current;
		uint32_t value = current[0] | (current[1] << 8) | (current[2] << 16) | (current[3] << 24);
		this->current += 4;
		return value;
	}

	uint32_t u32B() {
		auto current = this->current;
		uint32_t value = (current[0] << 24) | (current[1] << 16) | (current[2] << 8) | current[3];
		this->current += 4;
		return value;
	}

	float f32L() {
		union Value {
			uint32_t i;
			float f;
		};
		Value v = {.i = u32L()};
		return v.f;
	}

	uint64_t u64L() {
		auto lo = uint64_t(u32L());
		return lo | (uint64_t(u32L()) << 32);
	}

	uint64_t u64B() {
		auto hi = uint64_t(u32B()) << 32;
		return hi | uint64_t(u32B());
	}

	/**
	 * Read a buffer with given length
	 * @tparam N length of buffer
	 * @return buffer
	 */
	template <int N>
	Array<uint8_t const, N> data8() {
		auto ar = this->current;
		this->current += N;
		return Array<uint8_t const, N>(ar);
	}

	template <typename T>
	void data16L(int count, T *data) {
		for (int i = 0; i < count; ++i) {
			uint16_t value = this->current[i * 2] | (this->current[i * 2 + 1] << 8);
			data[i] = T(value);
		}
		this->current += count * 2;
	}

	/**
	 * Read a string until end of data
	 * @return string
	 */
	String string() {
		auto str = this->current;
		this->current = this->end;
		return {int(this->end - str), str};
	}

	/**
	 * Read a string with given length
	 * @param length length of string
	 * @return string
	 */
	String string(int length) {
		auto str = this->current;
		this->current += length;
		return {length, str};
	}

	/**
	 * Read a string that represents a floating point number (e.g. 1.3, no exponential notation)
	 * @return string containing a floating point number
	 */
	String floatString() {
		auto str = this->current;
		auto it = str;
		while (it < this->end) {
			if ((*it < '0' || *it > '9') && *it != '.')
				break;
			++it;
		}
		return String(it - str, str);
	}

	/**
	 * Skip some bytes
	 * @param n number of bytes to skip
	 */
	void skip(int n) {
		this->current += n;
	}

	void skipSpace() {
		auto it = this->current;
		while (it < this->end && (*it == ' ' || *it == '\t'))
			++it;
		this->current = it;
	}

	/**
	 * Check if the reader is still valid, i.e. did not read past the end
	 * @return true when before or at the end
	 */
	bool isValid() const {
		return this->current <= this->end;
	}

	/**
	 * Check if we are at the end of the message
	 * @return true when at or past the end
	 */
	bool atEnd() const {
		return this->current >= this->end;
	}

	/**
	 * Get remaining number of bytes in the message
	 * @return number of remaining bytes
	 */
	int getRemaining() const {
		return int(this->end - this->current);
	}

	uint8_t *current;
	uint8_t *end;
};


class DecryptReader : public MessageReader {
public:

	DecryptReader() = default;
	DecryptReader(uint8_t *message, uint8_t *end) : MessageReader(message, end) {}
	DecryptReader(int length, uint8_t *message) : MessageReader(length, message) {}

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
	void setMessageFromEnd(int offset) {this->message = this->end - offset;}

	/**
	 * Decrypt in-place, call after header has been read
	 */
	bool decrypt(int micLength, Nonce const &nonce, AesKey const &aesKey) {
		auto header = this->header;
		auto message = this->message;
		assert(header != nullptr);
		assert(message != nullptr);
		auto mic = this->end - micLength;
		int headerLength = message - header;
		int payloadLength = mic - message; // is zero when only the mic is present

		// cut off message integrity code
		this->end -= micLength;

		// encrypt in-place
		return ::decrypt(message, header, headerLength, message, payloadLength, micLength, nonce, aesKey);
	}

protected:

	uint8_t const *header = nullptr;
	uint8_t *message = nullptr;
};

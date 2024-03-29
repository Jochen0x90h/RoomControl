#pragma once

#include <assert.hpp>
#include <cstdint>


/**
 * Array with known size that only references the data
 * @tparam T type of buffer elements
 * @tparam N size of buffer
 */
template <typename T, int N = -1>
class Array {
public:

	/**
	 * Construct from pointer
	 */
	explicit Array(T *buffer) noexcept : buffer(buffer) {}

	/**
	 * Construct from C-array or C-string
	 */
	template <typename T2>
	constexpr Array(T2 (&array)[N]) noexcept : buffer(array) {}

	/**
	 * Default copy constructor
	 */
	Array(Array const &array) = default;

	/**
	 * Copy construct from buffer with different data type (e.g. non-const to const)
	 */
	template <typename T2>
	Array(Array<T2, N> array) noexcept : buffer(array.data()) {}

	/**
	 * Default assignment operator
	 */
	Array &operator =(Array const &buffer) = default;

	/**
	 * Assignment operator from buffer with different data type (e.g. non-const to const)
	 */
	template <typename T2>
	Array &operator =(Array<T2, N> array) {
		this->buffer = array.data();
		return *this;
	}

	/**
	 * Check if the array is empty
	 * @return true when empty
	 */
	static bool isEmpty() {return N == 0;}

	/**
	 * Number of elements in the array
	 * @return number of elements
	 */
	static int count() {return N;}

	/**
	 * Fill whole buffer with a value
	 * @param value fill value
	 */
	void fill(T const &value) {
		for (auto &element : *this) {
			element = value;
		}
	}

	/**
	 * Assign from an iterator, also cast each value to destination type
	 * @param src source iterator
	 */
	template <typename InputIt>
	void assign(InputIt src) {
		auto it = begin();
		auto e = end();
		for (; it < e; ++it, ++src) {
			*it = T(*src);
		}
	}

	/**
	 * Comparison operator
	 */
	bool operator ==(Array<T const, N> array) const {
		auto b = array.data();
		for (int i = 0; i < N; ++i) {
			if (this->buffer[i] != b[i])
				return false;
		}
		return true;
	}

	/**
	 * Element-wise xor assignment operator
	 */
	Array &operator ^=(Array<T const, N> array) {
		auto b = array.data();
		for (int i = 0; i < N; ++i)
			this->buffer[i] ^= b[i];
		return *this;
	}

	/**
	 * Index operator
	 */
	T &operator [](int index) const {
		assert(uint32_t(index) < N);
		return this->buffer[index];
	}

	template <int M>
	Array<T, M> array(int index) const {
		assert(uint32_t(index) <= N - M);
		return Array<T, M>(this->buffer + index);
	}

	/**
	 * Get pointer to data
	 */
	T *data() {return this->buffer;}

	/**
	 * Iterators
	 */
	T *begin() const {return this->buffer;}
	T *end() const {return this->buffer + N;}

protected:

	T *buffer;
};


/**
 * Array with variable size that only references the data, similar to std::span
 * @tparam T type of buffer element, e.g. int const for a bufferof constant integers
 */
template <typename T>
class Array<T, -1> {
public:

	/**
	 * Default constructor
	 */
	constexpr Array() noexcept : length(0), buffer(nullptr) {}

	/**
	 * Construct from C-array or C-string
	 */
	template <int N>
	constexpr Array(T (&array)[N]) noexcept : length(N), buffer(array) {}

	/**
	 * Construct from lenth and data
	 */
	Array(int length, T *data) noexcept : length(length), buffer(data) {}

	/**
	 * Default copy constructor
	 */
	Array(Array const &array) = default;

	/**
	 * Copy construct from buffer with different data type (e.g. non-const to const)
	 */
	template <typename T2>
	Array(Array<T2> array) noexcept : length(array.count()), buffer(array.data()) {}

	/**
	 * Default assignment operator
	 */
	Array &operator =(Array const &array) = default;

	/**
	 * Check if the array is empty
	 * @return true when empty
	 */
	bool isEmpty() {return this->length <= 0;}

	/**
	 * Number of elements in the array
	 * @return number of elements
	 */
	int count() const {return this->length;}

	/**
	 * Fill whole buffer with a value
	 * @param value fill value
	 */
	void fill(T const &value) {
		for (auto &element : *this) {
			element = value;
		}
	}

	/**
	 * Assign from an iterator, also cast each value to destination type
	 * @param src source iterator
	 */
	template <typename InputIt>
	void assign(InputIt src) {
		auto it = begin();
		auto e = end();
		for (; it < e; ++it, ++src) {
			*it = T(*src);
		}
	}

	/**
	 * Index operator
	 */
	T &operator [](int index) const {
		assert(uint32_t(index) < this->length);
		return this->buffer[index];
	}

	/**
	 * Get pointer to the data
	 */
	T *data() {return this->buffer;}

	/**
	 * Iterators
	 */
	T *begin() const {return this->buffer;}
	T *end() const {return this->buffer + this->length;}

protected:

	int length;
	T *buffer;
};

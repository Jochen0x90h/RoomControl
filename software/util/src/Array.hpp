#pragma once

#include <assert.hpp>

/**
 * Array wrapper, only references the data, similar to std::span
 * @tparam T type of array element, e.g. int const for an array of constant integers
 */
template <typename T>
struct Array {
	int const length;
	T *const data;
		
	constexpr Array() : length(0), data(nullptr) {}

	template <int N>
	constexpr Array(T (&array)[N]) : length(N), data(array) {}

	Array(int length, T *data) : length(length), data(data) {}

	bool isEmpty() {return this->length <= 0;}

	T &operator [](int index) const {
		assert(index >= 0 && index < this->length);
		return this->data[index];
	}

	const T *begin() const {return this->data;}
	const T *end() const {return this->data + this->length;}
};

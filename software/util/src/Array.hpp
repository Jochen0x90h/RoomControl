#pragma once

#include <assert.hpp>

/**
 * Array wrapper, only references the data
 * @tparam T array element
 */
template <typename T>
struct Array {
	int const length;
	T const *const data;
		
	constexpr Array() : length(0), data(nullptr) {}

	template <int N>
	constexpr Array(T const (&array)[N]) : length(N), data(array) {}

	Array(int length, T const *data) : length(length), data(data) {}

	bool empty() {return this->length <= 0;}

	T const &operator [](int index) const {
		assert(index >= 0 && index < this->length);
		return this->data[index];
	}

	const T *begin() const {return this->data;}
	const T *end() const {return this->data + this->length;}
};

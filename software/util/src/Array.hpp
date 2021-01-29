#pragma once


/**
 * Array, only references the data
 * @tparam T array element
 */
template <typename T>
struct Array {
	T const *const data;
	int const length;
		
	constexpr Array() : data(nullptr), length(0) {}

	template <int N>
	constexpr Array(T const (&array)[N]) : data(array), length(N) {}

	Array(T const *data, int length) : data(data), length(length) {}

	bool empty() {return this->length <= 0;}

	T const &operator [](int index) const {
		assert(index >= 0 && index < this->length);
		return this->data[index];
	}

	const T *begin() const {return this->data;}
	const T *end() const {return this->data + this->length;}
};

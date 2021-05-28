#pragma once

#include <assert.hpp>

/**
 * Data wrapper, only references the data
 */
struct Data {
	void const *const data;
	int const size;
		
	constexpr Data() : data(nullptr), size(0) {}

	template <typename T>
	constexpr Data(T const *data) : data(data), size(sizeof(T)) {}

	Data(void const *data, int size) : data(data), size(size) {}

	template <typename T>
	T const &cast() {
		assert(sizeof(T) >= this->size);
		return *reinterpret_cast<T const *>(this->data);
	}
};

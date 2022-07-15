#pragma once

#include <assert.hpp>

/**
 * Data wrapper, only references the data
 */
class ConstData {
public:

	constexpr ConstData() : s(0), d(nullptr) {}

	ConstData(void const *data, int size) : s(size), d(data) {}

	template <typename T>
	constexpr ConstData(T const *data) : s(sizeof(T)), d(data) {}

	/**
	 * Get size of data in bytes
	 * @return size
	 */
	int size() {return this->s;}

	/**
	 * Get pointer to the data
	 * @return data
	 */
	void const *data() {return this->d;}

	template <typename T>
	T const *cast() {
		assert(sizeof(T) >= this->s);
		return reinterpret_cast<T const *>(this->d);
	}

protected:

	int const s;
	void const *const d;
};

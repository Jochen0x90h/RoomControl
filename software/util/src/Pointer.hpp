#pragma once

#include <assert.hpp>

/**
 * Unique pointer
 * @tparam T pointer type
 */
template <typename T>
struct Pointer {
	T *ptr;
		
	constexpr Pointer() : ptr(nullptr) {}

	Pointer(T *ptr) : ptr(ptr) {}

	Pointer(Pointer &&ptr) : ptr(ptr.ptr) {
		ptr.ptr = nullptr;
	}

	~Pointer() {
		if (this->ptr != nullptr)
			delete this->ptr;
	}

	Pointer &operator =(T *ptr) {
		if (this->ptr != nullptr)
			delete this->ptr;
		this->ptr = ptr;
	}

	T &operator *() {
		return *this->ptr;
	}

	T *operator ->() {
		return this->ptr;
	}
};

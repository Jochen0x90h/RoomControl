#pragma once

#include <assert.hpp>

/**
 * Unique pointer
 * @tparam T pointer type
 */
template <typename T>
struct Pointer {
	T *p;
		
	constexpr Pointer() : p(nullptr) {}

	Pointer(T *p) : p(p) {}

	Pointer(Pointer &&p) : p(p.p) {
		p.p = nullptr;
	}

	~Pointer() {
		if (this->p != nullptr)
			delete this->p;
	}

	Pointer & operator =(T *p) {
		if (this->p != nullptr)
			delete this->p;
		this->p = p;		
	}

	T & operator *() {
		return *this->p;
	}

	T * operator ->() {
		return this->p;
	}
};

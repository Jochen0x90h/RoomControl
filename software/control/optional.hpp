#pragma once

#include "defines.hpp"


template <typename T>
class optional {
public:
	bool valid;
	T value;

	optional(NullType) : valid(false) {}

	optional(T value) : valid(true), value(value) {
	}

	T const &operator *() const {
		return this->value;
	}

	T &operator *() {
		return this->value;
	}

	T const *operator ->() const {
		return &this->value;
	}

	T *operator ->() {
		return &this->value;
	}

	optional<T> &operator =(NullType) {
		this->valid = false;
		return *this;
	}

	optional<T> &operator =(T value) {
		this->valid = true;
		this->value = value;
		return *this;
	}

	bool operator == (NullType) const {
		return !this->valid;
	}

	bool operator != (NullType) const {
		return this->valid;
	}

	operator bool () const {
		return this->valid;
	}
};

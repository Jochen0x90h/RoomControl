#pragma once

#include "StringOperators.hpp"
#include "convert.hpp"
#include "util.hpp"


/**
 * String buffer with fixed maximum length
 */
template <int L>
class StringBuffer {
public:
	StringBuffer() : index(0) {}

	template <typename T>
	StringBuffer(T str) : index(0) {
		(*this) += str;
	}

	template <typename T>
	StringBuffer &operator =(T str) {
		this->index = 0;
		return (*this) += str;
	}

	StringBuffer &operator +=(char ch) {
		if (this->index < L)
			this->data[this->index++] = ch;
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
		return *this;
	}

	StringBuffer &operator +=(const String &str) {
		char *dst = this->data + this->index;
		int l = min(str.length, L - this->index);
		array::copy(dst, dst + l, str.begin());
		this->index += l;
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
		return *this;
	}

	template <typename T>
	StringBuffer &operator +=(Dec<T> dec) {
		uint32_t value = dec.value;
		if (dec.value < 0) {
			if (this->index < L)
				this->data[this->index++] = '-';
			value = -dec.value;
		}
		this->index += toString(value, this->data + this->index, L - this->index, dec.digitCount);
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
		return *this;
	}

	//StringBuffer &operator +=(int dec) {
	//	return operator +=(Dec<int>{dec, 1});
	//}

	template <typename T>
	StringBuffer &operator +=(Hex<T> hex) {
		this->index += hexToString(hex.value, this->data + this->index, L - this->index, hex.digitCount);
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
		return *this;
	}
	
	StringBuffer &operator +=(Flt flt) {
		this->index += toString(flt.value, this->data + this->index, L - this->index, flt.digitCount,
			flt.decimalCount);
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
		return *this;
	}

	template <typename A, typename B>
	StringBuffer &operator +=(Plus<A, B> const &plus) {
		(*this) += plus.a;
		return (*this) += plus.b;
	}

	char operator [](int index) const {return this->data[index];}
	char &operator [](int index) {return this->data[index];}

	operator String() {
		return {this->data, this->index};
	}
	String string() const {
		return {this->data, this->index};
	}

	void clear() {
		this->index = 0;
		#ifdef DEBUG
		this->data[0] = 0;
		#endif
	}

	bool empty() {return this->index == 0;}

	int length() {return this->index;}

	void resize(int length) {
		this->index = length;
		#ifdef DEBUG
		this->data[this->index] = 0;
		#endif
	}

protected:

	#ifdef DEBUG
	char data[L + 1];
	#else
	char data[L];
	#endif
	uint8_t index;
};

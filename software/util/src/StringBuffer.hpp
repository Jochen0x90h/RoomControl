#pragma once

#include "String.hpp"


/**
 * String buffer with fixed maximum length
 * @tparam N number of 8 bit chars in the buffer
 */
template <int N>
class StringBuffer {
	template <int M>
	friend int toString(int length, char *str, StringBuffer<M> const &b);

public:
	StringBuffer() : index(0) {}

	template <typename T>
	StringBuffer(T const &str) : index(0) {
		(*this) += str;
	}

	bool isEmpty() const {return this->index == 0;}

	int count() const {return this->index;}

	void clear() {
		this->index = 0;
#ifdef DEBUG
		this->buffer[0] = 0;
#endif
	}

	void resize(int length) {
		this->index = length;
#ifdef DEBUG
		this->buffer[this->index] = 0;
#endif
	}

	template <typename T>
	StringBuffer &operator =(T str) {
		this->index = 0;
		return (*this) += str;
	}

	StringBuffer &operator +=(char ch) {
		if (this->index < N)
			this->buffer[this->index++] = ch;
#ifdef DEBUG
		this->buffer[this->index] = 0;
#endif
		return *this;
	}

	StringBuffer &operator +=(String const &str) {
		int l = min(N - this->index, str.count());
		array::copy(l, this->buffer + this->index, str.data);
		this->index += l;
#ifdef DEBUG
		this->buffer[this->index] = 0;
#endif
		return *this;
	}

	template <typename T>
	StringBuffer &operator +=(T const &value) {
		*this << value;
		return *this;
	}

	// fulfill stream concept
	StringBuffer &operator <<(char ch) {
		return *this += ch;
	}
	StringBuffer &operator <<(String const &str) {
		return *this += str;
	}

	char operator [](int index) const {return this->buffer[index];}
	char &operator [](int index) {return this->buffer[index];}

	char const *data() const {return this->buffer;}

	operator String() const {
		return {this->index, this->buffer};
	}
	String string() const {
		return {this->index, this->buffer};
	}
	
protected:

#ifdef DEBUG
	char buffer[N + 1];
#else
	char buffer[N];
#endif
	typename UInt<N>::Type index;
};

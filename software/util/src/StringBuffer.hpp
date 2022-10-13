#pragma once

#include "String.hpp"
#include "Stream.hpp"


/**
 * String buffer with fixed maximum length
 * @tparam N number of 8 bit chars in the buffer
 */
template <int N>
class StringBuffer {
	//template <int M>
	//friend int toString(int length, char *str, StringBuffer<M> const &b);

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

	/**
	 * Plus assign for character
	 * @param ch character
	 * @return reference to this StringBuffer
	 */
	StringBuffer &operator +=(char ch) {
		if (this->index < N)
			this->buffer[this->index++] = ch;
#ifdef DEBUG
		this->buffer[this->index] = 0;
#endif
		return *this;
	}

	/**
	 * Plus assign for string
	 * @param str string
	 * @return reference to this StringBuffer
	 */
	StringBuffer &operator +=(String const &str) {
		int l = min(N - this->index, str.count());
		array::copy(l, this->buffer + this->index, str.data);
		this->index += l;
#ifdef DEBUG
		this->buffer[this->index] = 0;
#endif
		return *this;
	}

	/**
	 * Generic plus assign, works when there is an operator << implemented for Stream
	 * @tparam T type
	 * @param value value
	 * @return reference to this StringBuffer
	 */
	template <typename T>
	StringBuffer &operator +=(T const &value) {
		Stream s(*this);
		s << value;
		return *this;
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

/*
	Stream &operator <<(char ch) override {
		return *this += ch;
	}
	Stream &operator <<(String const &str) override {
		return *this += str;
	}
	Stream &operator <<(Command command) override {
		return *this;
	}
*/
	class Stream : public ::Stream {
	public:
		StringBuffer<N> &buffer;

		Stream(StringBuffer<N> &buffer) : buffer(buffer) {}
		~Stream() override {}
		Stream &operator <<(char ch) override {
			this->buffer += ch;
			return *this;
		}
		Stream &operator <<(String const &str) override {
			this->buffer += str;
			return *this;
		}
		Stream &operator <<(Command command) override {
			return *this;
		}
	};

	/**
	 * Clear the buffer and return a new stream
	 * @return stream
	 */
	Stream stream() {
		this->index = 0;
		return {*this};
	}

protected:

#ifdef DEBUG
	char buffer[N + 1];
#else
	char buffer[N];
#endif
	typename UInt<N>::Type index;
};

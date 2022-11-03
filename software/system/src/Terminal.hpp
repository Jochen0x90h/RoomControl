#pragma once

#include <StringBuffer.hpp>


namespace Terminal {

inline void init() {}

/**
 * WRITE a string to a terminal (blocking)
 * @param index of channel
 * @param str string to write
 */
void write(int index, String const& str);


class Stream : public ::Stream {
public:
	int index;

	Stream(int index) : index(index) {}

	~Stream() override {}

	Stream &operator <<(char ch) override {
		Terminal::write(this->index, String(1, &ch));
		return *this;
	}
	
	Stream &operator <<(String const &str) override {
		Terminal::write(this->index, str);
		return *this;
	}

	Stream &operator <<(Command command) override {
		return *this;
	}
};

extern Stream out;
extern Stream err;

} // namespace Terminal

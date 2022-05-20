#pragma once

#include <StringBuffer.hpp>


namespace Terminal {

/**
 * Write a string to a terminal (blocking)
 * @param index of channel
 * @param str string to write
 */
void write(int index, String const& str);


struct Stream {
	int index;
	
	Stream &operator <<(char ch) {
		Terminal::write(this->index, String(1, &ch));
		return *this;
	}
	
	Stream &operator <<(String const &str) {
		Terminal::write(this->index, str);
		return *this;
	}
};

extern Stream out;
extern Stream err;

} // namespace Terminal

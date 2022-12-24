#pragma once

#include "Output.hpp"
#include <StringBuffer.hpp>
#include <appConfig.hpp>


namespace debug {

inline void setRed() {Output::set(OUTPUT_DEBUG_RED);}
inline void clearRed() {Output::clear(OUTPUT_DEBUG_RED);}
inline void setRed(bool value) {Output::set(OUTPUT_DEBUG_RED, value);}
inline void toggleRed() {Output::toggle(OUTPUT_DEBUG_RED);}

inline void setGreen() {Output::set(OUTPUT_DEBUG_GREEN);}
inline void clearGreen() {Output::clear(OUTPUT_DEBUG_GREEN);}
inline void setGreen(bool value) {Output::set(OUTPUT_DEBUG_GREEN, value);}
inline void toggleGreen() {Output::toggle(OUTPUT_DEBUG_GREEN);}

inline void setBlue() {Output::set(OUTPUT_DEBUG_BLUE);}
inline void clearBlue() {Output::clear(OUTPUT_DEBUG_BLUE);}
inline void setBlue(bool value) {Output::set(OUTPUT_DEBUG_BLUE, value);}
inline void toggleBlue() {Output::toggle(OUTPUT_DEBUG_BLUE);}

inline void set(int state) {
	setRed(state & 1);
	setGreen(state & 2);
	setBlue(state & 4);
}

enum Color {
	BLACK = 0,

	RED = 1,
	GREEN = 2,
	BLUE = 4,

	YELLOW = 3,
	MAGENTA = 5,
	CYAN = 6,

	WHITE = 7,
};

inline void set(Color color) {
	int c = int(color);
	setRed(c & 1);
	setGreen(c & 2);
	setBlue(c & 4);
}

class Counter {
public:

	Counter &operator ++() {
		++this->c;
		set(this->c);
		return *this;
	}

	Counter &operator --() {
		--this->c;
		set(this->c);
		return *this;
	}

	int c = 0;
};

} // namespace debug

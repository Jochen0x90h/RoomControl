#pragma once

#include "Output.hpp"
#include <StringBuffer.hpp>
#include <appConfig.hpp>


namespace Debug {

inline void setRedLed() {Output::set(OUTPUT_DEBUG_RED);}
inline void clearRedLed() {Output::clear(OUTPUT_DEBUG_RED);}
inline void setRedLed(bool value) {Output::set(OUTPUT_DEBUG_RED, value);}
inline void toggleRedLed() {Output::toggle(OUTPUT_DEBUG_RED);}

inline void setGreenLed() {Output::set(OUTPUT_DEBUG_GREEN);}
inline void clearGreenLed() {Output::clear(OUTPUT_DEBUG_GREEN);}
inline void setGreenLed(bool value) {Output::set(OUTPUT_DEBUG_GREEN, value);}
inline void toggleGreenLed() {Output::toggle(OUTPUT_DEBUG_GREEN);}

inline void setBlueLed() {Output::set(OUTPUT_DEBUG_BLUE);}
inline void clearBlueLed() {Output::clear(OUTPUT_DEBUG_BLUE);}
inline void setBlueLed(bool value) {Output::set(OUTPUT_DEBUG_BLUE, value);}
inline void toggleBlueLed() {Output::toggle(OUTPUT_DEBUG_BLUE);}

inline void setLeds(int state) {
	Debug::setRedLed(state & 1);
	Debug::setGreenLed(state & 2);
	Debug::setBlueLed(state & 4);
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

inline void setColor(Color color) {
	int c = int(color);
	Debug::setRedLed(c & 1);
	Debug::setGreenLed(c & 2);
	Debug::setBlueLed(c & 4);
}

class Counter {
public:

	Counter &operator ++() {
		++this->c;
		setLeds(this->c);
		return *this;
	}

	Counter &operator --() {
		--this->c;
		setLeds(this->c);
		return *this;
	}

	int c = 0;
};

} // namespace Debug

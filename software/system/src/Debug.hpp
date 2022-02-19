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

} // namespace Debug

#pragma once

#include "output.hpp"
#include <StringBuffer.hpp>
#include <appConfig.hpp>


namespace debug {

inline void setRedLed() {output::set(OUTPUT_DEBUG_RED);}
inline void clearRedLed() {output::clear(OUTPUT_DEBUG_RED);}
inline void setRedLed(bool value) {output::set(OUTPUT_DEBUG_RED, value);}
inline void toggleRedLed() {output::toggle(OUTPUT_DEBUG_RED);}

inline void setGreenLed() {output::set(OUTPUT_DEBUG_GREEN);}
inline void clearGreenLed() {output::clear(OUTPUT_DEBUG_GREEN);}
inline void setGreenLed(bool value) {output::set(OUTPUT_DEBUG_GREEN, value);}
inline void toggleGreenLed() {output::toggle(OUTPUT_DEBUG_GREEN);}

inline void setBlueLed() {output::set(OUTPUT_DEBUG_BLUE);}
inline void clearBlueLed() {output::clear(OUTPUT_DEBUG_BLUE);}
inline void setBlueLed(bool value) {output::set(OUTPUT_DEBUG_BLUE, value);}
inline void toggleBlueLed() {output::toggle(OUTPUT_DEBUG_BLUE);}

} // namespace debug

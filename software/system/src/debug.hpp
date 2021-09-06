#pragma once

#include "out.hpp"
#include <StringBuffer.hpp>


namespace debug {

inline void setRedLed() {out::set(0);}
inline void clearRedLed() {out::clear(0);}
inline void setRedLed(bool value) {out::set(0, value);}
inline void toggleRedLed() {out::toggle(0);}

inline void setGreenLed() {out::set(1);}
inline void clearGreenLed() {out::clear(1);}
inline void setGreenLed(bool value) {out::set(1, value);}
inline void toggleGreenLed() {out::toggle(1);}

inline void setBlueLed() {out::set(2);}
inline void clearBlueLed() {out::clear(2);}
inline void setBlueLed(bool value) {out::set(2, value);}
inline void toggleBlueLed() {out::toggle(2);}

} // namespace debug

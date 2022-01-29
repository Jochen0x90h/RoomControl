#pragma once

#include "gpio.hpp"
#include <StringBuffer.hpp>


namespace debug {

inline void setRedLed() {gpio::set(0);}
inline void clearRedLed() {gpio::clear(0);}
inline void setRedLed(bool value) {gpio::set(0, value);}
inline void toggleRedLed() {gpio::toggle(0);}

inline void setGreenLed() {gpio::set(1);}
inline void clearGreenLed() {gpio::clear(1);}
inline void setGreenLed(bool value) {gpio::set(1, value);}
inline void toggleGreenLed() {gpio::toggle(1);}

inline void setBlueLed() {gpio::set(2);}
inline void clearBlueLed() {gpio::clear(2);}
inline void setBlueLed(bool value) {gpio::set(2, value);}
inline void toggleBlueLed() {gpio::toggle(2);}

} // namespace debug

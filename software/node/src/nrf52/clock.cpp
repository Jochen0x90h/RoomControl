#include "clock.hpp"
#include "util.hpp"

namespace clock {

std::function<void ()> onSecondElapsed = nop;
uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t weekday = 0;

void init() {
	// use channel 1 of RTC0 which always gets started in system::init()
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE1, Set);
	NRF_RTC0->CC[1] = (NRF_RTC0->COUNTER + 16384 + 256) & ~16383;
}

void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[1]) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_RTC0->EVENTS_COMPARE[1] = 0;
		clearInterrupt(RTC0_IRQn);
	
		NRF_RTC0->CC[1] = (NRF_RTC0->COUNTER + 16384 + 256) & ~16383;
		
		// increment clock time
		if (++seconds == 60) {
			seconds = 0;
			if (++minutes == 60) {
				minutes = 0;
				if (++hours == 24) {
					hours = 0;
					if (++weekday == 7)
						weekday = 0;
				}
			}
		}
	
		onSecondElapsed();
	}
}

ClockTime getTime() {
	return ClockTime(weekday, hours, minutes, seconds);
}

void setSecondTick(std::function<void ()> onElapsed) {
	onSecondElapsed = onElapsed;
}

} // namespace system

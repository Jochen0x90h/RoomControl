#include "../calendar.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace calendar {

std::function<void ()> onSecond[CALENDAR_SECOND_HANDLER_COUNT];
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
		if (++calendar::seconds == 60) {
			calendar::seconds = 0;
			if (++calendar::minutes == 60) {
				calendar::minutes = 0;
				if (++calendar::hours == 24) {
					calendar::hours = 0;
					if (++calendar::weekday == 7)
						calendar::weekday = 0;
				}
			}
		}
	
		// call second handlers
		for (auto const &h : calendar::onSecond) {
			if (h)
				h();
		}
	}
}

ClockTime getTime() {
	return ClockTime(calendar::weekday, calendar::hours, calendar::minutes, calendar::seconds);
}

int8_t addSecondHandler(std::function<void ()> const &onSecond) {
	assert(onSecond);
	for (int i = 0; i < CALENDAR_SECOND_HANDLER_COUNT; ++i) {
		if (!calendar::onSecond[i]) {
			calendar::onSecond[i] = onSecond;
			return i;
		}
	}
	
	// error: handler array is full
	assert(false);
	return -1;
}

} // namespace calendar

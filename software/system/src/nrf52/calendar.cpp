#include "../calendar.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace calendar {

std::function<void ()> onSecond[CALENDAR_SECOND_HANDLER_COUNT];
uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t weekday = 0;

// event loop handler chain
loop::Handler nextHandler;
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
		for (auto const &handler : calendar::onSecond) {
			if (handler)
				handler();
		}
	}
	
	// call next handler in chain
	calendar::nextHandler();
}

void init() {
	// use channel 1 of RTC0
	NRF_RTC0->CC[1] = (NRF_RTC0->COUNTER + 16384 + 256) & ~16383;
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE1, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = Trigger;

	// add to event loop handler chain
	calendar::nextHandler = loop::addHandler(handle);
}

ClockTime now() {
	return ClockTime(calendar::weekday, calendar::hours, calendar::minutes, calendar::seconds);
}

void setSecondHandler(int index, std::function<void ()> const &onSecond) {
	assert(uint(index) < CALENDAR_SECOND_HANDLER_COUNT);

	calendar::onSecond[index] = onSecond;
}

} // namespace calendar

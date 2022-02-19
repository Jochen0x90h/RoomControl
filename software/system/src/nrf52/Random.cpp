#include "defs.hpp"
#include <assert.hpp>
#include <util.hpp>
#include <cstdint>


/*
	Resources:
		NRF_RNG
*/
namespace Random {

uint8_t queue[16] __attribute__((aligned(4)));
volatile int readIndex;
volatile int writeIndex;

void init() {
	// does not matter if init() gets called multiple times

	Random::readIndex = array::count(queue);
	Random::writeIndex = 0;
	NRF_RNG->INTENSET = N(RNG_INTENSET_VALRDY, Enabled);
	enableInterrupt(RNG_IRQn);

	// immediately start to generate the first random numbers
	NRF_RNG->TASKS_START = TRIGGER;
}

extern "C" {
	void RNG_IRQHandler(void);
}

void RNG_IRQHandler(void) {
	if (NRF_RNG->EVENTS_VALRDY) {
		// new value is ready: clear pending interrupt flag at peripheral
		NRF_RNG->EVENTS_VALRDY = 0;
	
		int r = Random::readIndex;
		int w = Random::writeIndex;
		if (r - w > array::count(queue))
			w = r - array::count(queue);
		
		queue[w & (array::count(queue) - 1)] = NRF_RNG->VALUE;
		
		++w;
		
		// check if we catched up with the read index
		if (r - w <= 0)
			NRF_RNG->TASKS_STOP = TRIGGER;
			
		Random::writeIndex = w;
	}	
}

uint8_t int8() {	
	// assert that random number generator was initialized
	assert(NRF_RNG->INTENSET == N(RNG_INTENSET_VALRDY, Enabled));

	int index = Random::readIndex;
	Random::readIndex = index + 1;
	NRF_RNG->TASKS_START = TRIGGER;
	return queue[index & (array::count(queue) - 1)];
}

uint64_t int64() {
	// assert that random number generator was initialized
	assert(NRF_RNG->INTENSET == N(RNG_INTENSET_VALRDY, Enabled));

	int index = (Random::readIndex + 7) & ~7;
	Random::readIndex = index + 8;
	NRF_RNG->TASKS_START = TRIGGER;
	return *(uint64_t*)&queue[index & (array::count(queue) - 1)];
}

} // namespace Random
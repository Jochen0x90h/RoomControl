#include "global.hpp"
#include <assert.hpp>
#include <util.hpp>
#include <cstdint>


namespace rng {

uint8_t queue[16] __attribute__((aligned(4)));
volatile int readIndex;
volatile int writeIndex;

void init() {
	rng::readIndex = array::size(queue);
	rng::writeIndex = 0;
	NRF_RNG->INTENSET = N(RNG_INTENSET_VALRDY, Enabled);
	enableInterrupt(RNG_IRQn);

	// immediately start to generate the first random numbers
	NRF_RNG->TASKS_START = Trigger;
}

extern "C" {
	void RNG_IRQHandler(void);
}

void RNG_IRQHandler(void) {
	if (NRF_RNG->EVENTS_VALRDY) {
		// new value is ready: clear pending interrupt flag at peripheral
		NRF_RNG->EVENTS_VALRDY = 0;
	
		int r = rng::readIndex;
		int w = rng::writeIndex;
		if (r - w > array::size(queue))
			w = r - array::size(queue);
		
		queue[w & (array::size(queue) - 1)] = NRF_RNG->VALUE;
		
		++w;
		
		// check if we catched up with the read index
		if (r - w <= 0)
			NRF_RNG->TASKS_STOP = Trigger;
			
		rng::writeIndex = w;
	}	
}

uint8_t int8() {	
	// assert that random number generator was initialized
	assert(NRF_RNG->INTENSET == N(RNG_INTENSET_VALRDY, Enabled));

	int index = rng::readIndex;
	rng::readIndex = index + 1;
	NRF_RNG->TASKS_START = Trigger;
	return queue[index & (array::size(queue) - 1)];
}

uint64_t int64() {
	// assert that random number generator was initialized
	assert(NRF_RNG->INTENSET == N(RNG_INTENSET_VALRDY, Enabled));

	int index = (rng::readIndex + 7) & ~7;
	rng::readIndex = index + 8;
	NRF_RNG->TASKS_START = Trigger;
	return *(uint64_t*)&queue[index & (array::size(queue) - 1)];
}

} // namespace rng

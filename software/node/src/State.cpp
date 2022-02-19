#include "State.hpp"
#include <appConfig.hpp>


Coroutine PersistentStateManager::updater() {
	while (true) {
		// wait for a state to change
		co_await this->barrier.wait();
		
		// store states to persistent memory
		while (!this->list.isEmpty()) {
			auto &state = *reinterpret_cast<PersistentState<uint8_t> *>(this->list.next);
			
			int size = state.size;
			uint8_t offset = state.command[2];
			uint8_t counter = state.command[3 + size];

			// increment sequence counter (4 bit counter, each bit is stored as 01 or 10 for robustness)
			for (uint8_t bit = 1; bit != 0; bit <<= 2) {
				// 01 -> 10
				// 10 -> 11 -> 01 + carry (continue with next bit)
				counter += bit;
				if ((counter & bit) != 0)
					counter &= ~(bit << 1);
				else
					break;
			}
			state.command[3 + size] = counter;

			// toggle offset
			if ((counter & 1) == 0)
				state.command[2] += size + 1;
		
			#ifdef STATE_DEBUG_PRINT
				std::cout << "state: write " << int(size) << " bytes at " << std::dec << int(state.command[2]) << ", count " << std::hex << int(counter) << std::endl;
			#endif
			co_await Spi::transfer(SPI_FRAM, 3 + size + 1, state.command, 0, nullptr);
			
			// restore offset
			state.command[2] = offset;
			
			// remove from list
			state.remove();
		}
	}
}

int PersistentStateManager::allocateInternal(int size) {
	assert(size <= 4);
	
	int half = array::count(this->allocationTable) / 2;
	int start = size <= 2 ? 0 : half;
	for (int i = start; i < array::count(this->allocationTable); ++i) {
		
		uint32_t a = this->allocationTable[i];
		
		// check if at least one block is free
		if (a != 0xffffffff) {
			for (int j = 0; j < 32; ++j) {
				// check if this block is free
				int bit = 1 << j;
				if ((a & bit) == 0) {
					a |= bit;
					this->allocationTable[i] = a;
					if (i < half) {
						// 6 byte block
						return (i * 32 + j) * 16;
					} else {
						// 10 byte block
						return ((i - half) * 32 + j) * 16 + 6;
					}
				}
			}
		}
	}
	
	// todo: handle out of memory. Ignore at first as non volatile memory is quite large (1024 blocks)
	assert(false);
	return FRAM_SIZE - 10;
}

AwaitableCoroutine PersistentStateManager::restore(PersistentStateBase &state) {
	// get offset and size
	int offset = (state.command[1] << 8) | state.command[2];
	int size = state.size;

	// mark block of this state as used in allocation table of the state manaager
	int blockIndex = offset / 16;
	uint32_t bit = 1 << (blockIndex & 31);
	int index = blockIndex / 32;
	if ((offset & 15) > 0)
		index += array::count(state.manager->allocationTable) / 2;
	state.manager->allocationTable[index] |= bit;

	// read back sequence counters which determine which of the two values is the latest one
	uint8_t command[3];
	command[0] = FRAM_READ;
	command[1] = offset >> 8; // upper 8 bit of offset
	uint8_t read[4];
	
	// read sequence counter 1
	command[2] = offset + size;
	co_await Spi::transfer(SPI_FRAM, 3, command, 3 + 1, read);
	uint8_t c1 = read[3];

	// read sequence counter 2
	command[2] = offset + size * 2 + 1;
	co_await Spi::transfer(SPI_FRAM, 3, command, 3 + 1, read);
	uint8_t c2 = read[3];
	
	#ifdef STATE_DEBUG_PRINT
		std::cout << "c1: " << std::hex << int(c1) << " c2: " << int(c2) << std::dec << std::endl;
	#endif

	// check if any counter is valid (bits 1, 3, 5, 7 must be the inverse of bits 0, 2, 4, 6)
	bool c1Valid = ((c1 ^ (c1 >> 1)) & 0x55) == 0x55;
	bool c2Valid = ((c2 ^ (c2 >> 1)) & 0x55) == 0x55;
	
	// read back state value and sequence counter (overwrites state.command)
	if (c1Valid || c2Valid) {
		command[2] = offset;
		if (int8_t(c1 - c2) < 0)
			command[2] = offset + size + 1;
		co_await Spi::transfer(SPI_FRAM, 3, command, 3 + size + 1, state.command);
	}
	
	// set command for writing the state value
	state.command[0] = FRAM_WRITE;
	state.command[1] = offset >> 8;
	state.command[2] = offset;
}

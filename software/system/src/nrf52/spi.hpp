#pragma once

#include "../spi.hpp"


// internal interface if spi
namespace spi {

extern int taskCount;

struct Task {
	int csPin;
	
	intptr_t writeData;
	int writeLength;
	intptr_t readData;
	int readLength;
	
	std::function<void ()> onTransferred;
	
	Task() : onTransferred([]() {}) {}
};

Task &allocateTask();

void transfer();

} // namespace spi

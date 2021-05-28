#pragma once

#include "../spi.hpp"
#include <Queue.hpp>
#include <sysConfig.hpp>


// internal interface if spi
namespace spi {

struct Transfer {
	int csPin;
	
	intptr_t writeData;
	int writeLength;
	intptr_t readData;
	int readLength;
	
	std::function<void ()> onTransferred;
};

extern Queue<Transfer, SPI_TRANSFER_QUEUE_LENGTH> transferQueue;

void startTransfer();

} // namespace spi

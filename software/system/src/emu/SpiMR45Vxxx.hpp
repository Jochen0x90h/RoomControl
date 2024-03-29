#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "../posix/File.hpp"
#include <string>


/**
 * Emulates a MR45V064B FeRam an SPI slave device
 */
class SpiMR45Vxxx : public SpiMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	SpiMR45Vxxx(std::string const &filename, int size);

	Awaitable <Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void handle(Gui &gui) override;


	File file;
	int size;
	Waitlist<SpiMaster::Parameters> waitlist;
};

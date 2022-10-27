#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include <string>


/**
 * Virtual channel to a slave device using a dedicated CS pin
 */
class PrintSpiMaster : public SpiMaster, public Loop::Timeout {
public:
	explicit PrintSpiMaster(std::string name) : name(std::move(name)) {
	}

	Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void activate() override;

protected:
	std::string name;
	Waitlist<SpiMaster::Parameters> waitlist;
};

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include <string>


/**
 * Implementation of an SPI master that simply writes info about the transfer operations to Terminal::out
 */
class SpiMasterImpl : public SpiMaster, public loop::TimeHandler {
public:
	/**
	 * Constructor
	 * @param name name of the SPI master that appears in the printed messages
	 */
	explicit SpiMasterImpl(std::string name) : name(std::move(name)) {
	}

	[[nodiscard]] Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void activate() override;

protected:
	std::string name;
	Waitlist<SpiMaster::Parameters> waitlist;
};

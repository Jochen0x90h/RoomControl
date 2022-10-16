#include <Storage.hpp>
#include <SpiMaster.hpp>
#include <appConfig.hpp>


/**
 * Storage for counters that have a size of up to 4 bytes
 */
class FeRamCountersBase : public Storage {
public:
	FeRamCountersBase(SpiMaster &spi, int maxElementCount, uint8_t *sequenceCounters);

	[[nodiscard]] Awaitable<ReadParameters> read(int index, int &size, void *data, Status &status) override;
	[[nodiscard]] Awaitable<WriteParameters> write(int index, int size, void const *data, Status &status) override;
	[[nodiscard]] Awaitable<ClearParameters> clear(Status &status) override;

	Status readBlocking(int index, int &size, void *data) override;
	Status writeBlocking(int index, int size, void const *data) override;
	Status clearBlocking() override;

protected:
	Coroutine reader();
	Coroutine writer();

	Status readBuffer(int &size, void *data, int index, uint8_t const *buffer);

	void setSequenceCounter(int index, int counter) {
		uint8_t &sc = this->sequenceCounters[index / 4];
		int pos = index & 3;
		sc = (sc & ~(3 << pos)) | (counter << pos);
	}

	int nextSequenceCounter(int index) {
		uint8_t &sc = this->sequenceCounters[index / 4];
		int pos = index & 3;
		int counter = ((sc >> pos) & 3) + 1;
		sc = (sc & ~(3 << pos)) | (counter << pos);
		return counter;
	}

	SpiMaster &spi;
	Waitlist<ReadParameters> readWaitlist;
	Barrier<> readBarrier;

	Waitlist<WriteParameters> writeWaitlist;
	Barrier<> writeBarrier;

	Waitlist<ClearParameters> clearWaitlist;
	Barrier<> clearBarrier;

	int maxElementCount;
	uint8_t *sequenceCounters;
};


template <int FERAM_SIZE>
class FeRamCounters : public FeRamCountersBase {
public:
	static constexpr int MAX_ELEMENT_COUNT = FERAM_SIZE / 10;

	FeRamCounters(SpiMaster &spi) : FeRamCountersBase(spi, MAX_ELEMENT_COUNT, sequenceCountersBuffer) {}

protected:
	uint8_t sequenceCountersBuffer[(MAX_ELEMENT_COUNT + 3) / 4];
};

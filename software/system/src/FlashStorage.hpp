#include <Storage.hpp>
#include <Flash.hpp>


/**
 * Storage working on internal flash
 * Inspired by https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/fs/nvs/nvs.c
 */
class FlashStorage : public Storage {
public:
	/**
	 * Constructor
	 * @param flash interface to a flash memory
	 */
	FlashStorage(Flash &flash);

	[[nodiscard]] Awaitable<ReadParameters> read(int id, int &size, void *data, Status &status) override;
	[[nodiscard]] Awaitable<WriteParameters> write(int id, int size, const void *data, Status &status) override;
	[[nodiscard]] Awaitable<ClearParameters> clear(Status &status) override;

	Status readBlocking(int id, int &size, void *data) override;
	Status writeBlocking(int id, int size, const void *data) override;
	Status clearBlocking() override;

	// allocation table entry
	union Entry {
		struct {
			// id of entry
			uint16_t id;

			// size of data
			uint16_t size;

			// offset of data in sector
			uint16_t offset;

			// checksum of the entry
			uint16_t checksum;
		};
		uint32_t data[2];

		bool isEmpty() {return (this->data[0] & this->data[1]) == 0xffffffff;}
	};

protected:
	static constexpr int BUFFER_SIZE = 32;

	enum SectorState {
		EMPTY,
		OPEN,
		CLOSED
	};

	// detect the state of a sector
	SectorState detectSectorState(int sectorIndex);

	// detect the entry and data offsets for an open sector
	std::pair<int, int> detectOffsets(int sectorIndex);

	// get the offset of the last entry in a closed sector
	int getLastEntry(int sector);

	// check if allocation table entry is valid
	bool isEntryValid(int entryOffset, int dataOffset, Entry &entry) const;

	// close the current sector
	void closeSector();

	// check if closing allocation table entry is valid
	bool isCloseEntryValid(Entry &entry) const;

	// check if a sector contains a newer entry for the given entry
	bool contains(Entry &entry, int sectorIndex, int entryOffset);

	// write an entry (without data)
	void writeEntry(uint16_t id, uint16_t size);

	// copy an entry in the given sector to the current sector
	void copyEntry(int sector, Entry &entry);

	// garbage collect
	void gc(int emptySectorIndex);


	Flash &flash;
	Flash::Info info;
	int entrySize;

	// current sector
	int sectorIndex = 0;
	int sector = 0;

	// write offsets in current sector
	int entryWriteOffset;
	int dataWriteOffset;
};

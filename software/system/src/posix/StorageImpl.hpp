#include "../Storage.hpp"
#include "File.hpp"
#include <map>
#include <vector>
#include <string>


/**
 * Implementation of Storage interface using files
 */
class StorageImpl : public Storage {
public:
	/**
	 * Constructor
	 * @param filename file name where the data is stored
	 * @param maxId maximum valid id
	 * @param maxDataSize maximum size of data of an element
	 */
	StorageImpl(std::string const &filename, int maxId, int maxDataSize);

	[[nodiscard]] Awaitable<ReadParameters> read(int id, int &size, void *data, Status &status) override;
	[[nodiscard]] Awaitable<WriteParameters> write(int id, int size, void const *data, Status &status) override;
	[[nodiscard]] Awaitable<ClearParameters> clear(Status &status) override;

	Status readBlocking(int id, int &size, void *data) override;
	Status writeBlocking(int id, int size, void const *data) override;
	Status clearBlocking() override;

protected:
	void readData();
	void writeData();

	std::string filename;
	int maxId;
	int maxDataSize;
	std::map<uint16_t, std::vector<uint8_t>> elements;
};

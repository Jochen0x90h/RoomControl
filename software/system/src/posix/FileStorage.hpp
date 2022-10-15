#include "../Storage2.hpp"
#include <String.hpp>
#include <map>
#include <vector>
#include <string>


/**
 * Implementation of Storage interface using files
 */
class FileStorage : public Storage2 {
public:
	FileStorage(std::string const &filename, int maxElementCount, int maxElementSize);

	[[nodiscard]] virtual Awaitable<ReadParameters> read(int id, int &size, void *data, Status &status) override;
	[[nodiscard]] virtual Awaitable<WriteParameters> write(int id, int size, void const *data, Status &status) override;
	[[nodiscard]] virtual Awaitable<ClearParameters> clear(Status &status) override;

	virtual Status readBlocking(int id, int &size, void *data) override;
	virtual Status writeBlocking(int id, int size, void const *data) override;
	virtual Status clearBlocking() override;

protected:
	void readData();
	void writeData();

	std::string filename;
	int maxElementCount;
	int maxElementSize;
	std::map<uint16_t, std::vector<uint8_t>> elements;
};

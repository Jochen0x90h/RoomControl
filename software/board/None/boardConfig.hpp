#include <posix/PrintSpiMaster.hpp>
#include <posix/FileStorage.hpp>



// bluetooth
// ---------

constexpr int BLUETOOTH_CONTEXT_COUNT = 2;


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 1;


// storage
// -------

constexpr int STORAGE_CONTEXT_COUNT = 1;


// drivers
// -------

struct Drivers {
	PrintSpiMaster airSensor{"airSensor"};
	PrintSpiMaster display{"display"};
	FileStorage storage{"storage.bin", 65536, 1024};
};

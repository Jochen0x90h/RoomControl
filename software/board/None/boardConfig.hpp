#include <posix/SpiMasterImpl.hpp>
#include <posix/StorageImpl.hpp>
#include <posix/FlashImpl.hpp>
#include <FlashStorage.hpp>


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
	SpiMasterImpl airSensor{"airSensor"};
	SpiMasterImpl display{"display"};
	//StorageImpl storage{"storage.bin", 65535, 1024};
	FlashImpl flash{"flash.bin", 2, 65536, 4};
	FlashStorage storage{flash};
};

struct DriversFlashTest {
	FlashImpl flash{"flashTest2.bin", 2, 65536, 4};
};

struct DriversStorageTest {
	FlashImpl flash{"storageTest.bin", 2, 65536, 4};
	FlashStorage storage{flash};
};

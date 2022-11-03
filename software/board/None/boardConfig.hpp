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
	//StorageImpl storage{"storage.bin", 0xffff, 1024};
	FlashImpl flash{"flash.bin", 32, 4096, 4};
	FlashStorage storage{flash};
};

struct DriversFlashTest {
	FlashImpl flash{"flashTest.bin", 2, 4096, 4};
};

struct DriversStorageTest {
	//FlashImpl flash{"storageTest.bin", 2, 65536, 4};
	FlashImpl flash{"storageTest.bin", 4, 32768, 4};
	//FlashImpl flash{"storageTest.bin", 32, 4096, 4};
	FlashStorage storage{flash};
};

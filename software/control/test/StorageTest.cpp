#include <State.hpp>
#include <ArrayStorage.hpp>
#include <Loop.hpp>
#include <Timer.hpp>
#include <Spi.hpp>
#include <iostream>


// test storage

struct Device;

struct DeviceFlash {
	int foo;
	
	// return size of flash data in bytes
	int size() const {
		return sizeof(DeviceFlash);
	}
	
	// allocate memory
	Device *allocate() const;
};

class Device : public ArrayStorage::Element<DeviceFlash> {
public:
	Device(DeviceFlash const &flash) : ArrayStorage::Element<DeviceFlash>(flash) {
		++counter;
	}
	~Device() {
		--counter;
	}

	int bar = 0;

	static int counter;
};
int Device::counter = 0;

Device *DeviceFlash::allocate() const {
	return new Device(*this);
}

void printDevices(ArrayStorage::Array<Device> const &devices, std::string s) {
	std::cout << s << std::endl;
	for (auto &d : devices) {
		std::cout << "\tflash: " << d->foo << " ram: " << d.bar << std::endl;
	}
	std::cout << "\tDevice instance count: " << Device::counter << std::endl;
}

void testStorage() {
	Flash::init("flashTest.bin");
	
	// erase flash
	//for (int i = 0; i < FLASH_PAGE_COUNT; ++i)
	//	flash::erase(i);
	
	// array
	ArrayStorage::Array<Device> devices;
	ArrayStorage storage(0, FLASH_PAGE_COUNT, devices);
	

	// print loaded devices
	printDevices(devices, "loaded");


	// write new devices
	DeviceFlash deviceFlash{50};
	Pointer<Device> device = new Device(deviceFlash);
	devices.write(0, std::move(device));
	
	DeviceFlash deviceFlash2{10};
	devices.write(1, new Device(deviceFlash2));

	// move
	devices.move(0, 1);
	
	// print current devices
	printDevices(devices, "current");
}


// test persistent state

Coroutine testStateCoroutine() {
	PersistentStateManager stateManager;
	
	// state 1
	PersistentState<int> s1(stateManager);
	co_await s1.restore();

	// state 2
	PersistentState<uint8_t> s2(stateManager);
	co_await s2.restore();

	while (true) {
		std::cout << "s1: " << int(s1) << std::endl;
		std::cout << "s2: " << int(s2) << std::endl;

		++s1;
		--s2;

		co_await Timer::sleep(1s);
	}
}

void testState() {
	Loop::init();
	Timer::init();
	Spi::init("framTest.bin");

	testStateCoroutine();

	Loop::run();
}

int main(void) {
	testStorage();
	testState();
	return 0;
}

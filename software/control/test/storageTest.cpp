#include <State.hpp>
#include <Storage.hpp>
#include <loop.hpp>
#include <timer.hpp>
#include <spi.hpp>
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

class Device : public Storage::Element<DeviceFlash> {
public:
	Device(DeviceFlash const &flash) : Storage::Element<DeviceFlash>(flash) {
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

void printDevices(Storage::Array<Device> const &devices, std::string s) {
	std::cout << s << std::endl;
	for (auto &d : devices) {
		std::cout << "\tflash: " << d->foo << " ram: " << d.bar << std::endl;
	}
	std::cout << "\tDevice instance count: " << Device::counter << std::endl;
}

void testStorage() {
	flash::init("flashTest.bin");
	
	// erase flash
	//for (int i = 0; i < FLASH_PAGE_COUNT; ++i)
	//	flash::erase(i);
	
	// array
	Storage::Array<Device> devices;
	Storage storage(0, FLASH_PAGE_COUNT, devices);
	

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
	int o1 = stateManager.allocate<int>();
	PersistentState<int> s1(o1);
	co_await s1.restore(&stateManager);

	// state 2
	int o2 = stateManager.allocate<uint8_t>();
	PersistentState<uint8_t> s2(o2);
	co_await s2.restore(&stateManager);

	while (true) {
		std::cout << "s1: " << int(s1) << std::endl;
		std::cout << "s2: " << int(s2) << std::endl;

		++s1;
		--s2;

		co_await timer::sleep(1s);
	}
}

void testState() {
	loop::init();
	timer::init();
	spi::init("framTest.bin");

	testStateCoroutine();

	loop::run();
}

int main(void) {
	testStorage();
	testState();
	return 0;
}

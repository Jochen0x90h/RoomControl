#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <Terminal.hpp>
#include <StringOperators.hpp>
#include <boardConfig.hpp>


struct Kiss32Random {
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t c;

	// seed must be != 0
	explicit Kiss32Random(uint32_t seed = 123456789) {
		x = seed;
		y = 362436000;
		z = 521288629;
		c = 7654321;
	}

	uint32_t draw() {
		// Linear congruence generator
		x = 69069 * x + 12345;

		// Xor shift
		y ^= y << 13;
		y ^= y >> 17;
		y ^= y << 5;

		// Multiply-with-carry
		uint64_t t = 698769069ULL * z + c;
		c = t >> 32;
		z = (uint32_t) t;

		return x + y + z;
	}
};

void fail() {
	Debug::setRedLed();
	while (true) {}
}

int main() {
	Loop::init();
	Timer::init();
	Output::init(); // for debug led's
	DriversStorageTest drivers;

	Kiss32Random random;

	auto start = Timer::now();

	// table of currently stored elements
	int sizes[64];
	array::fill(sizes, 0);
	uint8_t buffer[128];

	// determine capacity
	auto info = drivers.flash.getInfo();
	unsigned int capacity = min(((info.sectorCount - 1) * (info.sectorSize - 8)) / (128 + 8), array::count(sizes)) - 1;

	// clear storage
	drivers.storage.clearBlocking();

	for (int i = 0; i < 10000; ++i) {
		// check if everything is correctly stored
		for (int index = 0; index < capacity; ++index) {
			array::fill(buffer, 0);
			int size = sizes[index];
			int id = index + 5;

			// read data
			int readSize = size;
			drivers.storage.readBlocking(id, readSize, buffer);

			// check data
			if (readSize != size)
				fail();
			for (int j = 0; j < size; ++j) {
				if (buffer[j] != uint8_t(id + j))
					fail();
			}
		}

		// random size in range [0, 128]
		int size = random.draw() % 129;

		// generate id in range [5, 68]
		int index = random.draw() % capacity;
		int id = index + 5;
		sizes[index] = size;

		// generate data
		for (int j = 0; j < size; ++j) {
			buffer[j] = id + j;
		}

		// store
		if (drivers.storage.writeBlocking(id, size, buffer) != Storage::Status::OK)
			fail();
	}

	auto end = Timer::now();

	Terminal::out << dec(int((end - start) / 1s)) << "s\n";

	Debug::setGreenLed();
	while (true) {}
}

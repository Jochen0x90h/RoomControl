#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <boardConfig.hpp>


struct Kiss32Random {
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t c;

	// seed must be != 0
	Kiss32Random(uint32_t seed = 123456789) {
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

int main(void) {
	Loop::init();
	Timer::init();
	Output::init(); // for debug led's
	DriversStorageTest drivers;

	Kiss32Random random;

	// table of currently stored elements
	int sizes[64];
	array::fill(sizes, 0);
	uint8_t buffer[256];

	drivers.storage.clearBlocking();

	for (int i = 0; i < 10000; ++i) {
		// check if everything is correctly stored
		for (int index = 0; index < 64; ++index) {
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

		// random size in range [0, 255]
		int size = random.draw() & 255;

		// generate id in range [5, 68]
		int index = random.draw() & 63;
		int id = index + 5;
		sizes[index] = size;

		// generate data
		for (int i = 0; i < size; ++i) {
			buffer[i] = id + i;
		}

		// store
		drivers.storage.writeBlocking(id, size, buffer);
	}

	Debug::setGreenLed();
	while (true) {}
}

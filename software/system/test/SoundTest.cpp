#include <Loop.hpp>
#include <Timer.hpp>
#include <Sound.hpp>
#include <Terminal.hpp>
#include <Debug.hpp>
#include <StringOperators.hpp>
#include <util.hpp>


Coroutine soundTest() {
	int index = 0;
	while (true) {
		Sound::play(index);
		do {
			co_await Timer::sleep(1s);
		} while (Sound::isPlaying(index));
		Debug::toggleBlueLed();

		index = (index + 1) % Sound::getTypes().count();
	}
}

int main() {
	Loop::init();
	Timer::init();
	Sound::init();
	Output::init(); // for debug led's

	auto types = Sound::getTypes();
	if (types.isEmpty()) {
		Terminal::out << "No Sounds available!\n";
		return 0;
	}
	for (auto type : types) {
		Terminal::out << "Type: " << dec(type) << '\n';
	}

	soundTest();

	Loop::run();
}

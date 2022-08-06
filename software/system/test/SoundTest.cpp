#include <Loop.hpp>
#include <Timer.hpp>
#include <Sound.hpp>
#include <Terminal.hpp>
#include <Debug.hpp>
#include <StringOperators.hpp>
#include <util.hpp>


Coroutine soundTest() {
	Sound::play(0);
	while (true) {
		//Audio::play(0);

		co_await Timer::sleep(5s);
		
		Debug::toggleBlueLed();
	}
}

int main(void) {
	Loop::init();
	Timer::init();
	Sound::init();
	Output::init(); // for debug led's

	Terminal::out << dec(Sound::count()) << '\n';

	soundTest();

	Loop::run();
}

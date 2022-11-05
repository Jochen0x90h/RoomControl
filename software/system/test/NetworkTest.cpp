#include <Network.hpp>
#include <Timer.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <Coroutine.hpp>
#ifdef PLATFORM_POSIX
#include <string>
#endif


/*
 * NetworkTest: Either start one instance without arguments that sends to itself on port 1337
 * or start two instances, one with arguments 1337 1338 and one with arguments 1338 1337
 */

Network::Endpoint destination;


Coroutine sender() {
	uint8_t data[] = {1, 2, 3, 4};
	while (true) {
		co_await Network::send(0, destination, array::count(data), data);
		Debug::toggleRedLed();
		co_await Timer::sleep(1s);
	}
}

Coroutine receiver() {
	Network::Endpoint source;
	uint8_t data[4];
	while (true) {
		int count = 4;
		co_await Network::receive(0, source, count, data);
		Debug::toggleGreenLed();
	}
}

// it is possible to start two instances with different ports
uint16_t localPort = 1337;
uint16_t remotePort = 1337;

#ifdef PLATFORM_POSIX
int main(int argc, char const **argv) {
	if (argc >= 3) {
		localPort = std::stoi(argv[1]);
		remotePort = std::stoi(argv[2]);
	}
#else
int main() {
#endif
	Loop::init();
	Timer::init();
	Network::init();
	Output::init(); // for debug signals on pins

	Network::open(0, localPort);
	
	destination = {Network::Address::fromString("::1"), remotePort};
	
	sender();
	receiver();

	Loop::run();
}

#include <network.hpp>
#include <timer.hpp>
#include <loop.hpp>
#include <debug.hpp>
#include <Coroutine.hpp>


network::Endpoint destination;


Coroutine sender() {
	uint8_t data[] = {1, 2, 3, 4};
	while (true) {
		co_await network::send(0, destination, array::size(data), data);
		debug::toggleRedLed();
		co_await timer::delay(1s);
	}
}

Coroutine receiver() {
	network::Endpoint source;
	uint8_t data[4];
	while (true) {
		int count = 4;
		co_await network::receive(0, source, count, data);
		debug::toggleGreenLed();
	}
}

#ifdef EMU
int main(int argc, char const **argv) {
	uint16_t localPort = 1337;
	uint16_t remotePort = 1338;
	if (argc >= 3) {
		localPort = std::stoi(argv[1]);
		remotePort = std::stoi(argv[2]);
	}
#else
int main(void) {
	uint16_t localPort = 1337;
	uint16_t remotePort = 1338;
#endif
	loop::init();
	timer::init();
	out::init();
	network::init();

	network::setLocalPort(0, localPort);
	
	destination = network::Endpoint::fromString("::1", remotePort);
	
	sender();
	receiver();

	loop::run();
}

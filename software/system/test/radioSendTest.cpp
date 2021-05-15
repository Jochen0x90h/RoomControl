#include <timer.hpp>
#include <rng.hpp>
#include <radio.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>
#include <config.hpp>
#include <Queue.hpp>

/*
	This test sends a packet every second and sets the green led on success (ack received) and red led on error
*/

uint8_t packet[] = {0, 0x61, 0x88, 0, 0x62, 0x1a, 0x00, 0x00, 0x36, 0xcb, 0x48, 0x02, 0x00, 0x00, 0x36,
	0xcb, 0x1e, 0xb8, 0x28, 0x21, 0x00, 0x00, 0x00, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12, 0x00,
	0x00, 0x45, 0xf4, 0x0b, 0x3f, 0xd4, 0xf5, 0xbf, 0x99, 0xae, 0xa9, 0x34, 0xea, 0x4a, 0x2f, 0x7f};

uint8_t timerId;
uint8_t radioId;

int channel = 15;

void send() {
	packet[0] = array::size(packet) - 1 + 2;
	packet[3]++;

	debug::setRedLed(false);
	debug::setGreenLed(false);
	debug::setBlueLed(false);

	// send over the air
	radio::send(radioId, packet, [](bool success) {
		debug::setRedLed(!success);
		debug::setGreenLed(success);
		
		// change channel
		radio::stop();
		channel ^= 1;
		radio::start(channel);
		radio::enableReceiver(true);	
	});

	timer::start(timerId, timer::now() + 1s, []() {send();});
}


int main(void) {
	loop::init();
	timer::init();
	rng::init(); // needed by radio
	radio::init();
	debug::init();
		
	// allocate a timer
	timerId = timer::allocate();
	
	// allocate a radio context
	radioId = radio::allocate();

	// send is reported as successful only when ack was received
	radio::setFlags(radioId, radio::HANDLE_ACK);

	// start radio
	radio::start(15);
	radio::enableReceiver(true);

	//timer::start(timerId, timer::now() + 1s, []() {send();});
	send();
	
	loop::run();
}

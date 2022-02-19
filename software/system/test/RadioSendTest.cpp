#include <Timer.hpp>
#include <Random.hpp>
#include <Radio.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#include <Coroutine.hpp>
#include <Queue.hpp>
#include <util.hpp>

/*
	This test sends a packet every second and sets the green led on success (ack received) and red led on error
*/

// toggle command
/*uint8_t packet[] = {0, 0x61, 0x88, 0, 0x62, 0x1a, 0x00, 0x00, 0x36, 0xcb, 0x48, 0x02, 0x00, 0x00, 0x36,
	0xcb, 0x1e, 0xb8, 0x28, 0x21, 0x00, 0x00, 0x00, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12, 0x00,
	0x00, 0x45, 0xf4, 0x0b, 0x3f, 0xd4, 0xf5, 0xbf, 0x99, 0xae, 0xa9, 0x34, 0xea, 0x4a, 0x2f, 0x7f,
	uint8_t(radio::SendFlags::NONE)};*/

// association request
uint8_t packet[] = {0, 0x23, 0xc8, 0x45, 0x62, 0x1a, 0x00, 0x00, 0xff, 0xff, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12,
	0x00, 0x01, 0x80,
	uint8_t(Radio::SendFlags::NONE)};

// data request
uint8_t dataRequest[] = {0, 0x63, 0xc8, 0x46, 0x62, 0x1a, 0x00, 0x00, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12, 0x00, 0x04,
	uint8_t(Radio::SendFlags::NONE)};


int channel = 15;

Coroutine send() {
	// subtract one length bute, one flags byte and add 2 byte for crc (not in packet)
	packet[0] = array::count(packet) - 1 - 1 + 2;
	dataRequest[0] = array::count(dataRequest) - 1 - 1 + 2;
	while (true) {

		// send over the air
		uint8_t result;
		co_await Radio::send(0, packet, result);
		packet[3]++;
		
		bool success = result != 0;
		Debug::setRedLed(!success);
		Debug::setGreenLed(success);
			
		// change channel
		/*radio::stop();
		channel ^= 1;
		radio::start(channel);
		radio::enableReceiver(true);*/


		co_await Timer::sleep(500ms);
		co_await Radio::send(0, dataRequest, result);
		packet[3]++;

		co_await Timer::sleep(1s);
	}
}


int main(void) {
	Loop::init();
	Timer::init();
	Radio::init();
	Output::init();
	
	Radio::setPan(0, 0x1a62);
	Radio::setLongAddress(UINT64_C(0x00124b002268b96b));
	
	// send is reported as successful only when ack was received
	Radio::setFlags(0, Radio::ContextFlags::PASS_DEST_LONG | Radio::ContextFlags::HANDLE_ACK);

	// start radio
	Radio::start(15);
	Radio::enableReceiver(true);

	send();
	
	Loop::run();
}
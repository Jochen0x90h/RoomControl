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

int channel = 15;

// association request (battery powered switch, request ack, receive off when idle)
uint8_t associationRequest1[] = {0, 0x23, 0xc8, 0x45, 0x62, 0x1a, 0x00, 0x00, 0xff, 0xff, 0x6b,
	0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12,0x00, 0x01, 0x80,
	uint8_t(Radio::SendFlags::NONE)};

// association request (hue lamp, request ack, receive on when idle)
uint8_t associationRequest2[] = {0, 0x23, 0xc8, 0x6e, 0x62, 0x1a, 0x00, 0x00, 0xff, 0xff, 0xfe,
	0xd0, 0xa5, 0x0b, 0x01, 0x88, 0x17, 0x00, 0x01, 0x8e,
	uint8_t(Radio::SendFlags::NONE)};

// data request
uint8_t dataRequest[] = {0, 0x63, 0xc8, 0x46, 0x62, 0x1a, 0x00, 0x00, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12, 0x00, 0x04,
	uint8_t(Radio::SendFlags::NONE)};

// toggle command
uint8_t toggleCommand[] = {0, 0x61, 0x88, 0, 0x62, 0x1a, 0x00, 0x00, 0x36, 0xcb, 0x48, 0x02, 0x00, 0x00, 0x36,
	0xcb, 0x1e, 0xb8, 0x28, 0x21, 0x00, 0x00, 0x00, 0x6b, 0xb9, 0x68, 0x22, 0x00, 0x4b, 0x12, 0x00,
	0x00, 0x45, 0xf4, 0x0b, 0x3f, 0xd4, 0xf5, 0xbf, 0x99, 0xae, 0xa9, 0x34, 0xea, 0x4a, 0x2f, 0x7f,
	uint8_t(Radio::SendFlags::NONE)};

// default response ([1]: 0x61 = with acknowledge request, 0x41 = without acknowledge request, [4-5]: pan id)
uint8_t defaultResponse[] = {0, 0x41, 0x88, 0x2f, 0x62, 0x2a, 0x00, 0x00, 0x19, 0xed, 0x48, 0x02, 0x00, 0x00, 0x19, 0xed, 0x1e,
	0x42, 0x28, 0x45, 0xf1, 0x03, 0x00, 0xfe, 0xd0, 0xa5, 0x0b, 0x01, 0x88, 0x17, 0x00, 0x00, 0x61,
	0x74, 0xcd, 0xe5, 0xdb, 0x52, 0x2e, 0x6a, 0x42, 0xe1, 0xc0, 0x03, 0x58, 0x21, 0x0a, 0x6b, 0xc0,
	uint8_t(Radio::SendFlags::NONE)};

// periodically send packets
Coroutine send() {
	// subtract one length byte, one flags byte and add 2 byte for crc (not in packet)
	auto &packet1 = defaultResponse;
	packet1[0] = array::count(packet1) - 1 - 1 + 2;
	auto &packet2 = dataRequest;
	packet2[0] = array::count(packet2) - 1 - 1 + 2;
	while (true) {
		uint8_t result;

		co_await Timer::sleep(55ms);

		// send over the air and increment mac counter
		co_await Radio::send(0, packet1, result);
		packet1[3]++;
		
		bool success = result != 0;
		Debug::setRedLed(!success);
		Debug::setGreenLed(success);
			
		// change channel
		/*radio::stop();
		channel ^= 1;
		radio::start(channel);
		radio::enableReceiver(true);*/


		//co_await Timer::sleep(1s);

		// send over the air and increment mac counter
		//co_await Radio::send(0, packet2, result);
		//packet2[3]++;

	}
}

// reply to received packets
Coroutine reply() {
	Radio::Packet receivePacket;
	auto &sendPacket = defaultResponse;
	sendPacket[0] = array::count(sendPacket) - 1 - 1 + 2;
	while (true) {
		// wait for receive packet
		Debug::setBlueLed(true);
		co_await Radio::receive(0, receivePacket);
		Debug::setBlueLed(false);

		// reply
		Debug::setRedLed(true);
		uint8_t result;
		co_await Radio::send(0, sendPacket, result);
		Debug::setRedLed(false);
	}

}


int main(void) {
	Loop::init();
	Timer::init();
	Radio::init();
	Output::init();
	
	Radio::setLongAddress(UINT64_C(0x00124b002268b96b));
	Radio::setPan(0, 0);//0x1a62);
	Radio::setShortAddress(0, 0xed19);

	// send is reported as successful only when ack was received
	Radio::setFlags(0, Radio::ContextFlags::PASS_DEST_LONG | Radio::ContextFlags::PASS_DEST_SHORT | Radio::ContextFlags::HANDLE_ACK);

	// start radio
	Radio::start(channel);
	Radio::enableReceiver(true);

	send();
	//reply();
	
	Loop::run();
}

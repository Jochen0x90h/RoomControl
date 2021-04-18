#include <radio.hpp>
#include <crypt.hpp>
#include <emu/global.hpp>
#include "Gui.hpp"
#include <config.hpp>

// 8 bit integer
#define INT8(value) uint8_t(value)

// little endian 32 bit integer
#define LE_INT32(value) uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)


// emulator implementation of radio uses virtual devices on user interface
namespace radio {

int channel = 15;
std::function<void ()> onSent;
std::function<void (uint8_t *, int)> onReceived[RADIO_RECEIVE_HANDLER_COUNT];

static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

// device key
static uint8_t const key[] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
AesKey aesKey;

// device id
uint32_t deviceId = 0x12345678;

// device security counter
uint32_t counter = 0xfffffff0;

std::chrono::steady_clock::time_point time;
int lastRocker = 0;

void doGui(Gui &gui, int &id) {
	
	int rocker = gui.doubleRocker(id++);
	if (rocker != -1) {

		// time difference
		auto now = std::chrono::steady_clock::now();
		int ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - radio::time).count();
		radio::time = now;

		if (ms > 3000 && rocker == 0) {
			// released after some time: commission
			if (lastRocker & 1)
				channel = 15;
			else if (lastRocker & 2)
				channel = 20;
			else if (lastRocker & 4)
				channel = 11;
			else if (lastRocker & 8)
				channel = 25;

			uint8_t packet[] = {
				0x01, 0x08, INT8(radio::counter), 0xff, 0xff, 0xff, 0xff, // mac header
				0x0c, // network header
				LE_INT32(radio::deviceId), // deviceId
				0xe0, 0x02, 0xc5, 0xf2, // command and flags
				0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // key
				LE_INT32(0), // mic
				LE_INT32(radio::counter)}; // counter

			// nonce
			GreenPowerNonce nonce(radio::deviceId, radio::deviceId);

			// header is deviceId
			uint8_t const *header = packet + 8;
			int headerLength = 4;
			
			// payload is key
			uint8_t *message = packet + 16;
			
			encrypt(message, nonce, header, headerLength, message, 16, 4, defaultAesKey);

			// call receive handlers
			for (auto const &h : radio::onReceived) {
				if (h)
					h(packet, array::size(packet));
			}
		} else {
			uint8_t command = rocker != 0 ? 0 : 0x04;
			int r = rocker | lastRocker;
			if (r == 1)
				command |= 0x10;
			else if (r == 2)
				command |= 0x11;
			else if (r == 4)
				command |= 0x13;
			else if (r == 8)
				command |= 0x12;

			uint8_t packet[] = {
				0x01, 0x08, INT8(radio::counter), 0xff, 0xff, 0xff, 0xff, // mac header
				0x8c, 0x30, // network header
				LE_INT32(radio::deviceId), // deviceId
				LE_INT32(radio::counter), // counter
				command,
				0x00, 0x00, 0x00, 0x00}; // mic

			// nonce
			DataBuffer<13> nonce;
			nonce.setLittleEndianInt32(0, radio::deviceId);
			nonce.setLittleEndianInt32(4, radio::deviceId);
			nonce.setLittleEndianInt32(8, radio::counter);
			nonce.setInt8(12, 0x05);

			// header is network header, deviceId, counter and command
			uint8_t const *header = packet + 7;
			int headerLength = 11;

			// message: empty payload and mic
			uint8_t *message = packet + 18;

			encrypt(message, nonce, header, headerLength, message, 0, 4, aesKey);

			// call receive handlers
			for (auto const &h : radio::onReceived) {
				if (h)
					h(packet, array::size(packet));
			}
		}
		radio::lastRocker = rocker;
	}

	gui.newLine();
}

void init() {
	setKey(aesKey, key);
}

void handle() {
}

void setChannel(int channel) {
	radio::channel = channel;
}

bool send(uint8_t const* data, std::function<void ()> const &onSent) {
	radio::onSent = onSent;

	return true;
}

uint8_t addReceiveHandler(std::function<void (uint8_t *, int)> const &onReceived) {
	assert(onReceived);
	for (int i = 0; i < RADIO_RECEIVE_HANDLER_COUNT; ++i) {
		if (!radio::onReceived[i]) {
			radio::onReceived[i] = onReceived;
			return i + 1;
		}
	}

	// error: handler array is full
	assert(false);
	return 0;
}

} // namespace radio

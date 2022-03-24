#include <Timer.hpp>
#include <Input.hpp>
#include <Output.hpp>
#include <Debug.hpp>
#include <Spi.hpp>
#include <Loop.hpp>
#include <SystemTime.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include "appConfig.hpp"


constexpr SystemDuration RELAY_TIME = 5ms;


class MessageReader : public DecryptReader {
public:
	MessageReader(int length, uint8_t *data) : DecryptReader(length, data) {}

	/**
	 * Read a value from 0 to 8 from bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 */
	uint8_t arbiter() {
		uint8_t value = u8();

		uint8_t count = 0;
		while (value > 0) {
			++count;
			value <<= 1;
		}
		return count;
	}
};

class MessageWriter : public EncryptWriter {
public:
	template <int N>
	MessageWriter(uint8_t (&message)[N]) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + N)
#endif
	{}

	MessageWriter(int length, uint8_t *message) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + length)
#endif
	{}

	/**
	 * Write a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 */
	void arbiter(uint8_t value) {
		u8(~(0xff >> value));
	}

	int getLength() {
		int length = this->current - this->begin;
#ifdef EMU
		assert(this->current <= this->end);
#endif
		return length;
	}

	uint8_t *begin;
#ifdef EMU
	uint8_t *end;
#endif
};



class Switch {
public:

	Switch() {
		// start coroutines
		checkButtons();
		//enumerate();
		//receive();
		updateRelays();
	}

	Coroutine checkButtons() {
		bool standalone = true;
		int address = 5;
		int securityCounter = 0;
		
		while (true) {
			int index;
			bool value;

			// wait until trigger detected on input
			co_await Input::trigger(0xf, 0xf, index, value);

			if (standalone) {
				// standalone mode: A and B channel each control one relay
				if (value) {
					if ((index & 2) == 0) {
						this->relays &= ~0x11;
						this->relays |= index & 1;
					} else {
						this->relays &= ~0x44;
						this->relays |= (index & 1) << 2;
					}
				}
				this->event.set();
			} /*else {
				// bus mode: report state change on bus
				uint8_t sendData[64];
				
				// write into read data of transfer()
				MessageWriter w(writeData);

				// set start of header
				w.setHeader();

				// encoded address
				w.arbiter((address & 7) + 1);
				w.arbiter(address >> 3);

				// security counter
				w.u32L(securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				uint8_t endpointIndex = (index >> 1) & 1;
				w.u8(endpointIndex);

				// state
				uint8_t state = int(value) << (index & 1);
				w.u8(state);

				// encrypt
				Nonce nonce(device.flash.address, device.securityCounter);
				w.encrypt(4, nonce, device.flash.aesKey);

				// increment security counter
				++securityCounter;

				co_await BusNode::send(w.getLength(), sendData);
			}*/
		}
	}
/*
	coroutine enumerate() {
		// commission
		while (true) {

			co_await BusNode::send();
						
			co_await Timer::sleep(1s);
		}
	}

	coroutine receive() {
		uint8_t receiveData[64];
		while (true) {

			int length = array::count(receiveData);
			co_await BusNode::receive(length, receiveData);
			
			// decode received message
			
			
		}
	}*/

	Coroutine updateRelays() {
		bool jpa = false;
		bool jpb = false;
		
		// current state of 4 relays A0, A1, B0, B1, upper 4 bit indicate "unknown state" after power-on	
		uint8_t relays = 0xf0;
		
		while (true) {
			// wait until relays change
			co_await this->event.wait();
			event.clear();
			
			// enable driver
			//Output::set(OUTPUT_DRIVER_EN);
			
			// repat until all relays have been changed
			while (this->relays != relays) {
				uint8_t change = this->relays ^ relays;

				// check if A0 or A1 need to be changed
				uint8_t ax = 0;
				uint8_t ay = 0;
				uint8_t az = 0;
				if (change & 0x33) {
					uint8_t a0 = relays & 1;
					uint8_t a1 = (relays >> 1) & 1;
		
					if (a0 == a1) {
						if (a0 == 0 || !jpa) {
							ax = (a0 ^ 1) | a0 << 1;
							ay = ax ^ 3;
							az = ax;
						}
						this->relays = (this->relays & ~0x33) | (relays & 0x03);
					} else if ((change & 0x11) != 0 && ((change & 0x22) == 0 || a0 == 0)) {
						ax = (a0 ^ 1) | a0 << 1;
						ay = ax ^ 3;
						this->relays = (this->relays & ~0x11) | (relays & 0x01);
					} else {
						az = (a1 ^ 1) | a1 << 1;
						ay = az ^ 3;
						this->relays = (this->relays & ~0x22) | (relays & 0x02);
					}
				}
			
				// check if B0 or B1 need to be changed
				uint8_t bx = 0;
				uint8_t by = 0;
				uint8_t bz = 0;
				if (change & 0xcc) {
					uint8_t b0 = (relays >> 2) & 1;
					uint8_t b1 = (relays >> 3) & 1;
		
					if (b0 == b1) {
						if (b0 == 0 || !jpb) {
							bx = (b0 ^ 1) | b0 << 1;
							by = bx ^ 3;
							bz = bx;
						}
						this->relays = (this->relays & ~0xcc) | (relays & 0x0c);
					} else if ((change & 0x44) != 0 && ((change & 0x88) == 0 || b0 == 0)) {
						bx = (b0 ^ 1) | b0 << 1;
						by = bx ^ 3;
						this->relays = (this->relays & ~0x44) | (relays & 0x04);
					} else {
						bz = (b1 ^ 1) | b1 << 1;
						by = bz ^ 3;
						this->relays = (this->relays & ~0x88) | (relays & 0x08);
					}
				}
				
				// build 16 bit word for relay driver
				uint16_t word = 0x8000 // enable
					| ax << 1
					| ay << 3
					| az << 5
					| bx << 7
					| by << 9
					| bz << 11;
				co_await Spi::transfer(SPI_RELAY_DRIVER, 1, &word, 0, nullptr);
			
				// wait some time until relay contacts react
				co_await Timer::sleep(RELAY_TIME);
			}

			// switch off all half bridges
			uint16_t word = 0;
			co_await Spi::transfer(SPI_RELAY_DRIVER, 1, &word, 0, nullptr);
			co_await Timer::sleep(RELAY_TIME);
			//Output::clear(OUTPUT_DRIVER_EN);		
		}
	}
	
	
	Event event;
	
	uint8_t relays = 0xf0;
};


int main(void) {
	Loop::init();
	Output::init(); // for debug signals on pins
	Input::init();

	Switch s;

	Loop::run();
}

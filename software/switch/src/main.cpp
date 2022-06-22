#include <Timer.hpp>
#include <Input.hpp>
#include <Output.hpp>
#include <Debug.hpp>
#include <Spi.hpp>
#include <BusNode.hpp>
#include <Loop.hpp>
#include <Terminal.hpp>
#include <SystemTime.hpp>
#include <bus.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <Queue.hpp>
#include <StringOperators.hpp>
#include "appConfig.hpp"
#include <boardConfig.hpp>


constexpr SystemDuration RELAY_TIME = 10ms;


constexpr auto ROCKER = bus::EndpointType::TERNARY_BUTTON_OUT;
constexpr auto LIGHT = bus::EndpointType::BINARY_POWER_LIGHT_IN;
constexpr auto BLIND = bus::EndpointType::TERNARY_OPENING_BLIND_IN;

enum class Mode : uint8_t {
	LIGHT = 1,
	DOUBLE_LIGHT = 2,
	BLIND = 3,
};
Mode modeA = Mode::LIGHT;
Mode modeB = Mode::LIGHT;

bus::EndpointType const endpointsLight[] = {ROCKER, LIGHT};
bus::EndpointType const endpointsDoubleLight[] = {ROCKER, LIGHT, LIGHT};
bus::EndpointType const endpointsBlind[] = {ROCKER, BLIND};
int const endpointCounts[4] = {0, 2, 3, 2};

Array<bus::EndpointType const> const endpoints[] = {
	Array<bus::EndpointType>(),
	endpointsLight,
	endpointsDoubleLight,
	endpointsBlind
};


// outputs
int outputStates[4];

AesKey const busDefaultAesKey = {{0x1337c6b3, 0xf16c7cb6, 0x2dec182d, 0x3078d618, 0xaec16bb7, 0x5fad1701, 0x72410f2c, 0x4239d934, 0xbef4739b, 0xe159649a, 0x93186bb6, 0xd121b282, 0x47c360a5, 0xa69a043f, 0x35826f89, 0xe4a3dd0b, 0x45024bcc, 0xe3984ff3, 0xd61a207a, 0x32b9fd71, 0x0356e8ef, 0xe0cea71c, 0x36d48766, 0x046d7a17, 0x1f8c181d, 0xff42bf01, 0xc9963867, 0xcdfb4270, 0x50a049a0, 0xafe2f6a1, 0x6674cec6, 0xab8f8cb6, 0xa3c407c2, 0x0c26f163, 0x6a523fa5, 0xc1ddb313, 0x79a97aba, 0x758f8bd9, 0x1fddb47c, 0xde00076f, 0x2c6cd2a7, 0x59e3597e, 0x463eed02, 0x983eea6d}};

constexpr int micLength = 4;


uint32_t deviceId = 0x12345678;
int address = -1;
AesKey aesKey;
uint32_t securityCounter = 0;


class Switch {
public:

	Switch() {
		// start coroutines
		checkButtons();
		send();
		receive();
		updateRelays();
	}

	Coroutine checkButtons() {
		while (true) {
			int index;
			bool state;

			// wait until trigger detected on input
			co_await Input::trigger(0xf, 0xf, index, state);
			//Terminal::out << "trigger " << dec(index) << ' ' << dec(int(state)) << '\n';

			// configure when at least one button was held for 3s after all were pressed
			if (this->configure && Timer::now() > this->allTime + 3s) {
				// use button state as configuration
				int a = this->buttons & 3;
				if (a != 0)
					modeA = Mode(a);
				int b = this->buttons >> 2;
				if (b != 0)
					modeB = Mode(b);
				this->configure = false;
				Terminal::out << "config " << dec(modeA) << ' ' << dec(modeB) << '\n';
			}

			// update button state (only needed for configuration and commissioning)
			uint8_t mask = 1 << index;
			this->buttons = (this->buttons & ~mask) | (uint8_t(state) << index);

			// check if all buttons are pressed
			if (this->buttons == 0x0f) {
				this->allTime = Timer::now();
				this->enumerate = true;
				this->configure = true;
				//Terminal::out << "all\n";
			}

			// enumerate when all buttons were released
			if (this->enumerate && this->buttons == 0) {
				//Terminal::out << "enumerate\n";
				this->enumerate = false;
				this->configure = false;

				// switch to standalone mode
				address = -1;

				// add enumerate to send queue
				this->sendQueue.addBack(SendElement{255, 255});
				this->sendBarrier.resumeFirst();
			}


			if (address == -1) {
				// standalone mode: A and B channel each control one relay with 0 = off and 1 = on
				if ((index & 2) == 0) {
					// A
					switch (modeA) {
						case Mode::LIGHT:
							if (state) {
								this->relays &= ~0x11;
								this->relays |= 1 - index;
							}
							break;
						case Mode::DOUBLE_LIGHT:
							this->relays &= ~0x10 << index;
							if (state)
								this->relays ^= 1 << index;
							break;
						case Mode::BLIND:
							if (state) {
								this->relays &= ~0x33;
								this->relays |= 1 << index;
							}
							break;
						default:
							break;
					}
				} else {
					// B
					index &= 1;
					switch (modeB) {
						case Mode::LIGHT:
							if (state) {
								this->relays &= ~0x44;
								this->relays |= (1 - index) << 2;
							}
							break;
						case Mode::DOUBLE_LIGHT:
							this->relays &= ~0x40 << index;
							if (state)
								this->relays ^= (1 << index) << 2;
							break;
						case Mode::BLIND:
							if (state) {
								this->relays &= ~0xcc;
								this->relays |= (1 << index) << 2;
							}
							break;
						default:
							break;
					}
				}
				this->relayBarrier.resumeFirst();
			} else {
				// bus mode: report state change on bus

				// endpoint index
				uint8_t endpointIndex = (index & 2) == 0 ? 0 : endpointCounts[int(modeA)];//(index >> 1) & 1;

				// value
				uint8_t value = int(state) << (index & 1);

				// add to send queue
				this->sendQueue.addBack(SendElement{endpointIndex, value});
				this->sendBarrier.resumeFirst();
			}
		}
	}

	Coroutine send() {
		uint8_t sendData[64];
		while (true) {
			if (this->sendQueue.isEmpty()) {
				// wait until someone fills the queue
				co_await this->sendBarrier.wait();
			}

			// get element from queue
			auto const &element = this->sendQueue[0];

			bus::MessageWriter w(sendData);
			w.setHeader();

			if (element.endpointIndex == 255) {
				// enumerate message

				// prefix with zero
				w.u8(0);

				// encoded device id
				w.id(deviceId);

				// list of endpoints of part A and B
				w.data16L(endpoints[int(modeA)]);
				w.data16L(endpoints[int(modeB)]);

				// encrypt
				w.setMessage();
				Nonce nonce(deviceId, 0);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				securityCounter = 0;
			} else {
				// data message

				// encoded address
				w.arbiter((address & 7) + 1);
				w.arbiter(address >> 3);

				// security counter
				w.u32L(securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(element.endpointIndex);

				// value
				w.u8(element.value);

				// encrypt
				Nonce nonce(address, securityCounter);
				w.encrypt(micLength, nonce, aesKey);

				// increment security counter
				++securityCounter;
			}
			this->sendQueue.removeFront();

			co_await BusNode::send(w.getLength(), sendData);
		}
	}

	Coroutine receive() {
		uint8_t receiveData[64];
		while (true) {
            // receive from bus
			int length = array::count(receiveData);
			co_await BusNode::receive(length, receiveData);

			// decode received message
			bus::MessageReader r(length, receiveData);
			r.setHeader();

			// skip command prefix (0) and arbitration byte (0)
			auto a1 = r.arbiter();
			if (a1 == 0) {
				// received command message
				auto a2 = r.u8();
				if (a2 == 0) {
					// commission

					// check device id
					auto id = r.u32L();
					if (id != deviceId)
						continue;

					// check message integrity code (mic)
					r.setMessageFromEnd(micLength);
					Nonce nonce(deviceId, 0);
					if (!r.decrypt(micLength, nonce, busDefaultAesKey))
						continue;

					// set address
					address = r.u8();
					//Terminal::out << "commissioned on address " << dec(address) << '\n';

					// set key
					setKey(aesKey, r.data8<16>());
				}
			} else {
				// received data message
				auto a2 = r.arbiter();

				// check address
				int addr = (a1 - 1) | (a2 << 3);
				if (addr != address)
					continue;

				// security counter
				uint32_t securityCounter = r.u32L();

				// todo: check security counter

				// decrypt
				r.setMessage();
				Nonce nonce(address, securityCounter);
				if (!r.decrypt(micLength, nonce, aesKey))
					break;

				uint8_t endpointIndex = r.u8();

				int endpointCountA = endpointCounts[int(modeA)];
				int endpointCountB = endpointCounts[int(modeB)];
				if (endpointIndex >= endpointCountA + endpointCountB)
					break;

				bus::EndpointType endpointType;
				int relayIndex;
				if (endpointIndex < endpointCountA) {
					endpointType = endpoints[int(modeA)][endpointIndex];
					relayIndex = endpointIndex == 2 ? 1 : 0;
				} else {
					endpointIndex -= endpointCountA;
					endpointType = endpoints[int(modeB)][endpointIndex];
					relayIndex = endpointIndex == 2 ? 3 : 2;
				}

				int &state = outputStates[relayIndex];

				switch (endpointType) {
					case LIGHT: {
						// 0: off, 1: on, 2: toggle
						uint8_t s = r.u8();
						if (s < 2)
							state = s;
						else if (s == 2)
							state ^= 1;
						this->relays &= ~(0x11 << relayIndex);
						this->relays |= state << relayIndex;
						this->relayBarrier.resumeFirst();
						break;
					}
					case BLIND: {
						// 0: inactive, 1: up, 2: down
						uint8_t s = r.u8();
						state = s <= 2 ? s : 0;
						this->relays &= ~(0x33 << relayIndex);
						this->relays |= state << relayIndex;
						this->relayBarrier.resumeFirst();
						break;
					}
					default:
						break;
				}
			}
		}
	}

	Coroutine updateRelays() {
		// current state of 4 relays A0, A1, B0, B1, upper 4 bit indicate "unknown state" after power-on
		uint8_t relays = 0xf0;
		
		while (true) {
			while (this->relays == relays) {
				// wait until relays change
				co_await this->relayBarrier.wait();
			}

			// repat until all relays have been changed
			while (this->relays != relays) {
				//Terminal::out << "relays " << hex(this->relays) << " " << hex(relays) << "\n";
				uint8_t change = this->relays ^ relays;

				// check if A0 or A1 need to be changed
				uint8_t ax = 0;
				uint8_t ay = 0;
				uint8_t az = 0;
				if (change & 0x33) {
					uint8_t a0 = this->relays & 1;
					uint8_t a1 = (this->relays >> 1) & 1;

					if ((change & 0x11) != 0 && ((change & 0x22) == 0 || a0 == 0 || a0 == a1)) {
						ax = (a0 ^ 1) | a0 << 1;
						ay = ax ^ 3;
						relays = (relays & ~0x11) | (this->relays & 0x11);
					}
					if ((change & 0x22) != 0 && ((change & 0x11) == 0 || a1 == 0 || a1 == a0)) {
						az = (a1 ^ 1) | a1 << 1;
						ay = az ^ 3;
						relays = (relays & ~0x22) | (this->relays & 0x22);
					}
				}
			
				// check if B0 or B1 need to be changed
				uint8_t bx = 0;
				uint8_t by = 0;
				uint8_t bz = 0;
				if (change & 0xcc) {
					uint8_t b0 = (this->relays >> 2) & 1;
					uint8_t b1 = (this->relays >> 3) & 1;

					if ((change & 0x44) != 0 && ((change & 0x88) == 0 || b0 == 0 || b0 == b1)) {
						bx = (b0 ^ 1) | b0 << 1;
						by = bx ^ 3;
						relays = (relays & ~0x44) | (this->relays & 0x44);
					}
					if ((change & 0x88) != 0 && ((change & 0x44) == 0 || b1 == 0 || b1 == b0)) {
						bz = (b1 ^ 1) | b1 << 1;
						by = bz ^ 3;
						relays = (relays & ~0x88) | (this->relays & 0x88);
					}
				}
				//Terminal::out << "ab " << dec(ax) << dec(ay) << dec(az) << " " << dec(bx) << dec(by) << dec(bz) << "\n";

				// build 16 bit word for relay driver
				uint16_t word = 0x8000 // enable
					| (ax << (MPQ6526_MAPPING[0] * 2 + 1))
					| (ay << (MPQ6526_MAPPING[1] * 2 + 1))
					| (az << (MPQ6526_MAPPING[2] * 2 + 1))
					| (bx << (MPQ6526_MAPPING[3] * 2 + 1))
					| (by << (MPQ6526_MAPPING[4] * 2 + 1))
					| (bz << (MPQ6526_MAPPING[5] * 2 + 1));
				//Terminal::out << "spi " << hex(word) << "\n";
				co_await Spi::transfer(SPI_RELAY_DRIVER, 1, &word, 0, nullptr);

				// wait some time until relay contacts react
				co_await Timer::sleep(RELAY_TIME);
			}

			// switch off all half bridges
			uint16_t word = 0;
			co_await Spi::transfer(SPI_RELAY_DRIVER, 1, &word, 0, nullptr);
		}
	}
	

	// commissioning and configuration
	uint8_t buttons = 0;
	SystemTime allTime;
	bool enumerate = false;
	bool configure = false;

	// sending over bus
	struct SendElement {uint8_t endpointIndex; uint8_t value;};
	Queue<SendElement, 8> sendQueue;
	Barrier<> sendBarrier;

	// relays
	uint8_t relays = 0xf0;
	Barrier<> relayBarrier;
};


int main(void) {
	Loop::init();
	Output::init(); // for debug signals on pins
	Input::init();
	Spi::init();
	Timer::init();
	BusNode::init();

	Switch s;

	Loop::run();
}

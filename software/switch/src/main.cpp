#include <Input.hpp>
#include <Output.hpp>
#include <Debug.hpp>
#include <BusNode.hpp>
#include <Loop.hpp>
#include <Terminal.hpp>
#include <SystemTime.hpp>
#include <bus.hpp>
#include <Queue.hpp>
#include <StringOperators.hpp>
#include <boardConfig.hpp>


/*
	Commission: Press all 4 buttons at the same time (emulator: press on center) and release again

	Configure: Press all 4 buttons at the same time (emulator: press on center), then release some buttons for
	the desired configuration and keep the remaining buttons held for at least 3 seconds.
	Buttons on left (A) side:
		Upper (A0): Light
		Lower (A1): Double Light
		Both (A0+A1): Blind
	Buttons on right (B) side:
		Upper (B0): Light
		Lower (B1): Double Light
		Both (B0+B1): Blind
*/

// todo: set individual device id before flashing
uint32_t deviceId = 0x12345678;

// time of relay to perform state change
constexpr SystemDuration RELAY_TIME = 10ms;

// plug types
constexpr auto ROCKER = bus::PlugType::TERNARY_ROCKER_OUT;
constexpr auto LIGHT = bus::PlugType::BINARY_POWER_LIGHT_IN;
constexpr auto BLIND = bus::PlugType::TERNARY_OPENING_BLIND_IN;

// plug lists for the different modes
const bus::PlugType plugsLight[] = {ROCKER, LIGHT};
const bus::PlugType plugsDoubleLight[] = {ROCKER, LIGHT, LIGHT};
const bus::PlugType plugsBlind[] = {ROCKER, BLIND};
const int plugCounts[4] = {0, 2, 3, 2};

const Array<bus::PlugType const> plugs[] = {
	{},
	plugsLight,
	plugsDoubleLight,
	plugsBlind
};

// encryption
constexpr int micLength = 4;


class Switch {
public:

	Switch() {
		int size;

		// load mode of channels A and B
		size = sizeof(Modes);
		this->drivers.storage.readBlocking(ID_MODES, size, &this->modes);
		if (size != sizeof(Modes) || this->modes.a < Mode::LIGHT || this->modes.a > Mode::BLIND)
			this->modes.a = Mode::LIGHT;
		if (size != sizeof(Modes) || this->modes.b < Mode::LIGHT || this->modes.b > Mode::BLIND)
			this->modes.b = Mode::LIGHT;

		// load commissioning
		size = sizeof(Commissioning);
		this->drivers.storage.readBlocking(ID_COMMISSIONING, size, &this->commissioning);
		if (size != sizeof(Commissioning))
			this->commissioning.address = -1;

		// load security counters
		size = 4;
		this->drivers.counters.readBlocking(ID_NODE_SECURITY_COUNTER, size, &this->nodeSecurityCounter);
		if (size != 4)
			this->nodeSecurityCounter = 0;
		size = 4;
		this->drivers.counters.readBlocking(ID_MASTER_SECURITY_COUNTER, size, &this->masterSecurityCounter);
		if (size != 4)
			this->masterSecurityCounter = 0;

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
			if (this->configure && loop::now() > this->allTime + 3s) {
				// use button state as configuration
				int a = this->buttons & 3;
				if (a != 0)
					this->modes.a = Mode(a);
				int b = this->buttons >> 2;
				if (b != 0)
					this->modes.b = Mode(b);
				this->configure = false;
				Terminal::out << "config " << dec(this->modes.a) << ' ' << dec(this->modes.b) << '\n';

				// write modes to flash
				this->drivers.storage.writeBlocking(ID_MODES, sizeof(Modes), &this->modes);
			}

			// update button state (only needed for configuration and commissioning)
			uint8_t mask = 1 << index;
			this->buttons = (this->buttons & ~mask) | (uint8_t(state) << index);

			// check if all buttons are pressed
			if (this->buttons == 0x0f) {
				this->allTime = loop::now();
				this->commission = true;
				this->configure = true;
				//Terminal::out << "all\n";
			}

			// commission when all buttons were released
			if (this->commission && this->buttons == 0) {
				//Terminal::out << "enumerate\n";
				this->commission = false;
				this->configure = false;

				// switch to standalone mode
				this->commissioning.address = -1;

				// store to flash (remove entry)
				this->drivers.storage.eraseBlocking(ID_COMMISSIONING);

				// add enumerate command to send queue
				this->sendQueue.addBack(SendElement{255, 255});
				this->sendBarrier.resumeFirst();
			}


			if (this->commissioning.address == -1) {
				// standalone mode: A and B channel each control one relay with 0 = off and 1 = on
				if ((index & 2) == 0) {
					// A
					switch (this->modes.a) {
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
					switch (this->modes.b) {
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

				// plug index (plugs of channel B come after plugs of channel A)
				uint8_t plugIndex = (index & 2) == 0 ? 0 : plugCounts[int(this->modes.a)];

				// value
				uint8_t value = int(state) << (index & 1);

				// add to send queue
				this->sendQueue.addBack(SendElement{plugIndex, value});
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

			if (element.plugIndex == 255) {
				if (element.value == 255) {
					// send enumerate message

					// prefix with zero
					w.u8(0);

					// encoded device id
					w.id(deviceId);

					// protocol version
					w.u8(0);

					// number of endpoints
					w.u8(1);

					// encrypt
					w.setMessage();
					Nonce nonce(deviceId, 0);
					w.encrypt(micLength, nonce, bus::defaultAesKey);

					this->nodeSecurityCounter = 0;
					this->drivers.counters.writeBlocking(ID_NODE_SECURITY_COUNTER, 4, &this->nodeSecurityCounter);
				} else {
					// send attribute
					auto attribute = bus::Attribute(element.value);

					// encoded address
					w.address(this->commissioning.address);

					// security counter
					w.u32L(this->nodeSecurityCounter);

					// set start of message
					w.setMessage();

					// "escaped" endpoint index
					w.u8(255);
					w.u8(0);

					// attribute
					w.e8(attribute);

					switch (attribute) {
					case bus::Attribute::MODEL_IDENTIFIER:
						w.string("Switch");
						break;
					case bus::Attribute::PLUG_LIST:
						// list of plugs
						// list of plugs of part A and B
						w.data16L(plugs[int(this->modes.a)]);
						w.data16L(plugs[int(this->modes.b)]);
						break;
					default:;
					}

					// encrypt
					Nonce nonce(this->commissioning.address, this->nodeSecurityCounter);
					w.encrypt(micLength, nonce, this->commissioning.aesKey);

					// increment security counter
					++this->nodeSecurityCounter;
					this->drivers.counters.writeBlocking(ID_NODE_SECURITY_COUNTER, 4, &this->nodeSecurityCounter);
				}
			} else {
				// send plug message

				// encoded address
				w.address(this->commissioning.address);

				// security counter
				w.u32L(this->nodeSecurityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(0);

				// plug index
				w.u8(element.plugIndex);

				// value
				w.u8(element.value);

				// encrypt
				Nonce nonce(this->commissioning.address, this->nodeSecurityCounter);
				w.encrypt(micLength, nonce, this->commissioning.aesKey);

				// increment security counter
				++this->nodeSecurityCounter;
				this->drivers.counters.writeBlocking(ID_NODE_SECURITY_COUNTER, 4, &this->nodeSecurityCounter);
			}
			this->sendQueue.removeFront();

			co_await this->drivers.busNode.send(w.getLength(), sendData);
		}
	}

	Coroutine receive() {
		uint8_t receiveData[64];
		while (true) {
            // receive from bus
			int length = array::count(receiveData);
			co_await this->drivers.busNode.receive(length, receiveData);

			// decode received message
			bus::MessageReader r(length, receiveData);
			r.setHeader();

			// check for command prefix (0)
			if (r.peekU8() == 0) {
				// received command message
				r.u8();
				if (r.u8() == 0) {
					// 0 0: commission

					// get device id
					auto id = r.u32L();

					// check message integrity code (mic)
					r.setMessageFromEnd(micLength);
					Nonce nonce(id, 0);
					if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
						continue;

					// get address
					auto address = r.u8();

					// check if device id is the device id of this node
					if (id == deviceId) {
						// clear master security counter
						this->masterSecurityCounter = 0;
						this->drivers.counters.writeBlocking(ID_MASTER_SECURITY_COUNTER, 4, &this->masterSecurityCounter);

						// set address
						this->commissioning.address = address;
						//Terminal::out << "commissioned on address " << dec(address) << '\n';

						// set key
						setKey(this->commissioning.aesKey, r.data8<16>());

						// store to flash
						this->drivers.storage.writeBlocking(ID_COMMISSIONING, sizeof(Commissioning), &this->commissioning);
					} else if (this->commissioning.address == address) {
						// the address of this node now gets assigned to another node: decommission
						// is probably a security hole
						/*
						// switch to standalone mode
						this->commissioning.address = -1;

						// store to flash (remove entry)
						this->drivers.storage.eraseBlocking(ID_COMMISSIONING);
						*/
					}
				}
			} else {
				// received data message: check address
				int addr = r.address();
				if (addr != this->commissioning.address)
					continue;

				// security counter
				uint32_t securityCounter = r.u32L();

				// check security counter
				if (securityCounter <= this->masterSecurityCounter) {
					Terminal::out << "M security counter error " << dec(securityCounter) << " <= " << dec(this->masterSecurityCounter) << '\n';
					//continue;
				} else {
					//Terminal::out << "M security counter ok " << dec(securityCounter) << " > " << dec(this->masterSecurityCounter) << '\n';
				}
				this->masterSecurityCounter = securityCounter;
				this->drivers.counters.writeBlocking(ID_MASTER_SECURITY_COUNTER, 4, &this->masterSecurityCounter);

				// decrypt
				r.setMessage();
				Nonce nonce(this->commissioning.address, securityCounter);
				if (!r.decrypt(micLength, nonce, this->commissioning.aesKey))
					break;

				// get endpoint index (255 for read attribute)
				uint8_t endpointIndex = r.u8();
				if (endpointIndex == 255) {
					// attribute
					endpointIndex = r.u8();
					auto attribute = r.e8<bus::Attribute>();
					if (r.atEnd() && endpointIndex == 0) {
						this->sendQueue.addBack(SendElement{255, uint8_t(attribute)});
						this->sendBarrier.resumeFirst();
					}
				} else if (endpointIndex == 0) {
					// plug
					uint8_t plugIndex = r.u8();
					int plugCountA = plugCounts[int(this->modes.a)];
					int plugCountB = plugCounts[int(this->modes.b)];
					if (plugIndex >= plugCountA + plugCountB)
						break;

					bus::PlugType plugType;
					int relayIndex;
					if (plugIndex < plugCountA) {
						plugType = plugs[int(this->modes.a)][plugIndex];
						relayIndex = plugIndex == 2 ? 1 : 0;
					} else {
						plugIndex -= plugCountA;
						plugType = plugs[int(this->modes.b)][plugIndex];
						relayIndex = plugIndex == 2 ? 3 : 2;
					}

					int &state = this->outputStates[relayIndex];

					switch (plugType) {
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
				uint16_t w = 0x8000 // enable
					| (ax << (MPQ6526_MAPPING[0] * 2 + 1))
					| (ay << (MPQ6526_MAPPING[1] * 2 + 1))
					| (az << (MPQ6526_MAPPING[2] * 2 + 1))
					| (bx << (MPQ6526_MAPPING[3] * 2 + 1))
					| (by << (MPQ6526_MAPPING[4] * 2 + 1))
					| (bz << (MPQ6526_MAPPING[5] * 2 + 1));
				//Terminal::out << "spi " << hex(w) << "\n";

				// transfer
				uint16_t r;
				co_await this->drivers.relayDriver.transfer(2, &w, 2, &r);

				// wait some time until relay contacts react
				co_await loop::sleep(RELAY_TIME);
			}

			// switch off all half bridges
			uint16_t word = 0;
			co_await this->drivers.relayDriver.transfer(1, &word, 0, nullptr);
		}
	}

	// platform dependent drivers
	SwitchDrivers drivers;

	// mode of channel A and B
	static constexpr int ID_MODES = 1;
	enum class Mode : uint8_t {
		LIGHT = 1,
		DOUBLE_LIGHT = 2,
		BLIND = 3,
	};
	struct Modes {
		Mode a;
		Mode b;
	};
	Modes modes;

	// commissioning and configuration
	static constexpr int ID_COMMISSIONING = 2;
	struct Commissioning {
		int address;
		AesKey aesKey;
	};
	Commissioning commissioning;
	uint8_t buttons = 0;
	SystemTime allTime;
	bool commission = false;
	bool configure = false;

	// security counters
	static constexpr int ID_NODE_SECURITY_COUNTER = 1;
	static constexpr int ID_MASTER_SECURITY_COUNTER = 2;
	uint32_t nodeSecurityCounter;
	uint32_t masterSecurityCounter;

	// sending over bus
	struct SendElement {uint8_t plugIndex; uint8_t value;};
	Queue<SendElement, 8> sendQueue;
	Barrier<> sendBarrier;

	// relays
	int outputStates[4] = {};
	uint8_t relays = 0xf0;
	Barrier<> relayBarrier;
};


int main() {
	loop::init();
	Output::init(); // for debug signals on pins
	Input::init();

	Switch s;

	loop::run();
}

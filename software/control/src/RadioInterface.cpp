#include "RadioInterface.hpp"
#include <radio.hpp>
#include <rng.hpp>
#include <timer.hpp>
#include <crypt.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <util.hpp>
#include <PacketReader.hpp>
#include <PacketWriter.hpp>
#include <iostream>


using namespace zb;


namespace SendFlags {
	constexpr int BEACON = 1;
	constexpr int LINK_STATUS = 2;
}


static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

RadioInterface::RadioInterface(AesKey const &networkKey, std::function<void (uint8_t, uint8_t const *, int)> const &onReceived)
	: networkKey(networkKey), onReceived(onReceived), panId(0xffff)
{
	// note: longAddress, panId and networkKey are not valid in the constructor yet

	// set short address of coordinator
	radio::setShortAddress(this->radioIndex, 0x0000);

	// filter packets with the coordinator as destination (and broadcast pan/address)
	radio::setFlags(this->radioIndex, radio::ContextFlags::PASS_DEST_SHORT | radio::ContextFlags::HANDLE_ACK);

	// start coroutines
	receive();
	sendBeacon();
	sendLinkStatus();
	
this->commissioning = true;
}

RadioInterface::~RadioInterface() {
}

void RadioInterface::setPan(uint16_t panId) {
	this->panId = panId;
	
	// set pan of coordinator
	radio::setPan(this->radioIndex, panId);
}

void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled && this->devices.size() < MAX_DEVICE_COUNT;
}

int RadioInterface::getDeviceCount() {
	return this->devices.size();
}

DeviceId RadioInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->devices.size());
	return this->devices[index].flash->deviceId;
	return 0;
}

Array<EndpointType> RadioInterface::getEndpoints(DeviceId deviceId) {
	for (auto e : this->devices) {
		Device const &device = *e.flash;
		if (device.deviceId == deviceId) {
			return Array<EndpointType>(device.endpointTypes, device.getEndpointCount());
		}
	}
	return {};
}

void RadioInterface::subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	for (auto e : this->devices) {
		Device const &device = *e.flash;
		DeviceState &state = *e.ram;
		if (device.deviceId == deviceId) {
			assert(endpointIndex >= 0 && endpointIndex < device.getEndpointCount());
			if (state.firstEndpointId == 0) {
				// allocate endpoints for this device
				state.firstEndpointId = this->nextEndpointId;
				this->nextEndpointId += device.getEndpointCount();
			}

			int newEndpointId = state.firstEndpointId + endpointIndex;
		
			if (endpointId == 0) {
				++state.referenceCounters[endpointIndex];
			} else {
				assert(endpointId == newEndpointId);
			}
			endpointId = newEndpointId;
			
			break;
		}
	}
}

void RadioInterface::unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	if (endpointId != 0) {
		for (auto e : this->devices) {
			Device const &device = *e.flash;
			DeviceState &state = *e.ram;
			if (device.deviceId == deviceId) {
				assert(endpointIndex >= 0 && endpointIndex < device.getEndpointCount());
				assert(state.referenceCounters[endpointIndex] > 0);
				
				--state.referenceCounters[endpointIndex];
				endpointId = 0;
			}

			break;
		}
	}
}

void RadioInterface::send(uint8_t endpointId, uint8_t const *data, int length) {
}


Coroutine RadioInterface::receive() {
	while (true) {
		radio::Packet packet;
		co_await radio::receive(this->radioIndex, packet);
		PacketReader d(packet);
		
		// ieee 802.15.4 mac
		uint8_t const *mac = d.current;

		// frame control
		auto frameControl = d.enum16<ieee::FrameControl>();

		// the radio filter flags ensure that a sequence number is present
		uint8_t sequenceNumber = d.int8();
		
		// the radio filter flags ensure that a destination pan id is present that is either broadcast or this->panId
		uint16_t destinationPanId = d.int16();
		bool destinationBroadcast = destinationPanId == 0xffff;
		
		// the radio filter flags ensure that a destination address is present hat is either boradcast or our address
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destAddress = d.int16();
			destinationBroadcast |= destAddress == 0xffff;
		} else {
			// long destination address is always our address
			d.skip(8);
		}

		auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) == 0) {
			// no source address
			if (frameType == ieee::FrameControl::TYPE_COMMAND) {
				auto command = d.enum8<ieee::Command>();
				if (command == ieee::Command::BEACON_REQUEST) {
					// handle beacon request by sending a beacon
					this->beaconWait.resumeAll();
				}
			} else if (frameType == ieee::FrameControl::TYPE_DATA) {
				// network layer: handle dependent on protocol version
				auto version = d.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
				if (version == gp::NwkFrameControl::VERSION_3_GP)
					handleGp(mac, d);
			}
		} else {
			// short or long source address

			// get source pan id
			uint16_t sourcePanId = destinationPanId;
			if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0) {
				sourcePanId = d.int16();
			}

			void* device = nullptr;
			if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
				// short source address
				uint16_t sourceAddress = d.int16();

				// try to lookup device by short address

			} else {
				// long source address
				uint64_t sourceAddress = d.int64();

				// try to lookup device by long address

				// check for association request if the device was not found
				if (!device && this->commissioning && frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = d.enum8<ieee::Command>();

					// check for association request with no broadcast
					if (!destinationBroadcast && command == ieee::Command::ASSOCIATION_REQUEST)
						handleAssociationRequest(sourceAddress, d);
				}
			}

			// check if the device was found
			if (device) {
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = d.enum8<ieee::Command>();

					// check for data request with no broadcast
					if (!destinationBroadcast) {
						if (command == ieee::Command::DATA_REQUEST)
							;//device->onDataRequest();
					}
				} else if (frameType == ieee::FrameControl::TYPE_DATA) {
					// network layer: handle dependent on protocol version
					auto version = d.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
					if (version == gp::NwkFrameControl::VERSION_2) {

					}
				}
			}
		}
	}
}

void RadioInterface::handleAssociationRequest(uint64_t sourceAddress, PacketReader &d) {
	auto associationRequest = d.enum8<ieee::AssociationRequest>();
	
}

void RadioInterface::handleGp(uint8_t const *mac, PacketReader &d) {
	// green power frame
	// -----------------
	// NWK header (frame control)
	// Security header (0, 1 or 4 byte counter)
	// NWK payload (optionally encrypted)
	// MIC (0, 2 or 4 byte message integrity code)
	uint8_t *frame = d.current;

	// frame control
	auto frameControl = d.enum8<gp::NwkFrameControl>();

	// extended frame conrol
	gp::NwkExtendedFrameControl extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = d.enum8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;
	
	// device id
	uint32_t deviceId = d.int32();

	if (this->commissioning && securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& d.peekEnum8<gp::Command>() == gp::Command::COMMISSIONING)
	{
		// commissioning
		handleCommission(deviceId, d);
		return;
	}

	// search device
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			Device const &device = *e.flash;
			DeviceState &state = *e.ram;
						
			if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16) {
				// security level 1: 1 byte counter, 2 byte mic

				// security counter (use mac sequence number)
				uint32_t counter = mac[2];

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at mac sequence number and includes also payload
				uint8_t const *header = mac + 2;
				int micLength = 2;
				int headerLength = d.end - micLength - header;
				if (headerLength < 1)
					return;
					
				// decrypt and check
				if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.u.key)) {
					//printf("error while decrypting message!\n");
					return;
				}
				
				// remove mic from end
				d.end -= micLength;
			} else if (securityLevel >= gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32) {
				// security levels 2 and 3: 4 byte counter, 4 byte mic
				
				// security counter
				uint32_t counter = d.int32();

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at frame (frameControl) and either includes payload (level 2) or not (level 3)
				uint8_t *header = frame;
				int micLength = 4;
				int headerAndPayloadLength = d.end - micLength - header;
				int headerLength = securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32
					? headerAndPayloadLength : d.current - header;
				if (headerLength < 1)
					return;

				uint8_t *message = header + headerLength;
				int payloadLength = headerAndPayloadLength - headerLength;

				// decrypt in-place and check
				if (!decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, device.u.key)) {
					//printf("error while decrypting message!\n");
					return;
				}

				// remove mic from end
				d.end -= micLength;
			}

			switch (device.deviceType) {
			case 0x02:
				if (d.getRemaining() >= 1) {
					int endpointIndex = -1;
					uint8_t data = 0;
					uint8_t command = d.int8();
					switch (command) {
					// A
					case 0x14:
					case 0x15:
						endpointIndex = 0;
						data = 0;
						break;
					case 0x10:
						endpointIndex = 0;
						data = 1;
						break;
					case 0x11:
						endpointIndex = 0;
						data = 2;
						break;
					// B
					case 0x17:
					case 0x16:
						endpointIndex = 1;
						data = 0;
						break;
					case 0x13:
						endpointIndex = 1;
						data = 1;
						break;
					case 0x12:
						endpointIndex = 1;
						data = 2;
						break;
					// AB
					case 0x65:
					case 0x63:
						endpointIndex = 2;
						data = 0;
						break;
					case 0x64:
						endpointIndex = 2;
						data = 1;
						break;
					case 0x62:
						endpointIndex = 2;
						data = 2;
						break;
					}
					if (endpointIndex != -1 && state.referenceCounters[endpointIndex] > 0)
						this->onReceived(state.firstEndpointId + endpointIndex, &data, 1);
				}
				break;
			case 0x07:
				if (d.getRemaining() >= 2) {
					uint8_t buttons = d.int8();
					
					uint8_t change = (buttons ^ state.state) & 0x0f;
					
					// check AB0 and AB1
					if (change == 0x5 || change == 0xa) {
						uint8_t data = buttons & 0x3;
						if (state.referenceCounters[2] > 0)
							this->onReceived(state.firstEndpointId + 2, &data, 1);
					} else {
						// check A0 and A1
						if (change & 0x3) {
							uint8_t data = buttons & 0x3;
							if (state.referenceCounters[0] > 0)
								this->onReceived(state.firstEndpointId + 0, &data, 1);
						}

						// check B0 and B1
						if (change & 0xC) {
							uint8_t data = (buttons >> 2) & 0x3;
							if (state.referenceCounters[1] > 0)
								this->onReceived(state.firstEndpointId + 1, &data, 1);
						}
					}
					
					state.state = buttons;
				}
			}
			
			break;
		}
	}
}

void RadioInterface::handleCommission(uint32_t deviceId, PacketReader& d) {
	// remove commissioning command (0xe0)
	d.int8();
	
	Device device;
	device.deviceId = deviceId;
	device.type = Device::GP;
	DeviceState deviceState = {};
	
	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	device.deviceType = d.int8();

	device.endpointTypes[0] = EndpointType(0);
	switch (device.deviceType) {
	case 0x02:
		// switch, PTM215Z ("friends of hue")
		device.endpointTypes[0] = EndpointType::ROCKER;
		device.endpointTypes[1] = EndpointType::ROCKER;
		device.endpointTypes[2] = EndpointType::ROCKER;
		device.endpointTypes[3] = EndpointType(0);
		break;
	case 0x07:
		// generic switch, PTM216Z
		device.endpointTypes[0] = EndpointType::ROCKER;
		device.endpointTypes[1] = EndpointType::ROCKER;
		device.endpointTypes[2] = EndpointType::ROCKER;
		device.endpointTypes[3] = EndpointType(0);
		break;
	default:
		// unknown device type
		return;
	}
	
	// options
	uint8_t options = d.int8();
	bool incrementalCounterFlag = options & 1;
	bool extendedFlag = options & 0x80;
	
	if (extendedFlag) {
		uint8_t extendedOptions = d.int8();

		// security level for messages
		// 0: none
		// 1: 1 byte counter and 2 byte MIC
		// 2: 4 byte counter + 4 byte MIC
		// 3: encryption + 4 byte counter + 4 byte MIC
		int securityLevel = extendedOptions & 3;

		// key type
		int keyType = (extendedOptions >> 2) & 7;
		
		// 128 bit (16 byte) key follows
		bool keyFlag = extendedOptions & 0x20;
		
		// key is encrypted and additional 4 byte MIC follows
		bool keyEncryptedFlag = extendedOptions & 0x40;
		
		// current value of security counter follows
		bool counterFlag = extendedOptions & 0x80;
	
		if (keyFlag) {
			uint8_t *key = d.current;
			if (keyEncryptedFlag) {
				// decrypt key, Green Power A.1.5.3.3.3
				
				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);

				if (!decrypt(key, nonce, header.data, 4, key, 16, 4, defaultAesKey)) {
					//printf("error while decrypting key!\n");
					return;
				}

				// skip key and MIC
				d.skip(16 + 4);
			} else {
				// skip key
				d.skip(16);
			}
			
			// set key for device
			setKey(device.u.key, key);
		}
		if (counterFlag) {
			deviceState.counter = d.int32();
			//printf("counter: %08x\n", counter);
		}
	}

	// check if we exceeded the end
	if (d.getRemaining() < 0)
		return;

	// check if device already exists
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			// yes: only update security counter
			e.ram->counter = deviceState.counter;
			return;
		}
	}

	// store device in flash
	this->devices.write(this->devices.size(), &device, &deviceState);
}

Coroutine RadioInterface::sendBeacon() {
	uint8_t packet[32];
	while (true) {
		// wait until a beacon request was received
		co_await this->beaconWait.wait();
		
		PacketWriter d(packet);

		// ieee 802.15.4 frame control
		d.enum16(ieee::FrameControl::TYPE_BEACON
			| ieee::FrameControl::SOURCE_ADDRESSING_SHORT);

		// sequence number
		d.int8(this->macSequenceNumber++);

		// source pan
		d.int16(this->panId);

		// source address
		d.int16(0x0000);

		// superframe specification
		d.int16(15 // beacon interval
			| (15 << 4) // superframe interval
			| (15 << 8) // final cap slot
			| (0 << 12) // battery extension
			| 0x4000 // pan coordinator
			| (this->commissioning ? 0x8000 : 0)); // association permit

		// gts
		d.int8(0);

		// pending addresses, short and long
		d.int8(0x00);

		// beacon

		// protocol id
		d.int8(0);

		// stack profile
		d.int16(2 // zb pro
			| (2 << 4) // protocol version
			| (0 << 10) // router capacity
			| (0 << 11) // device depth (4 bit)
			| 0x8000); // end device capacity

		// extended pan id
		d.int64(UINT64_C(0xdddddddddddddddd));

		// tx offset
		d.int16(0xffff);
		d.int8(0xff);

		// update id
		d.int8(0);
	
		// send beacon
		uint8_t result;
		co_await radio::send(this->radioIndex, d.finish(), result);
		
		// "cool down" before a new beacon can be sent
		co_await timer::delay(100ms);
	}
}

Coroutine RadioInterface::sendLinkStatus() {
	uint8_t packet[48];
	while (true) {
		// send link status every 15 seconds
		co_await timer::delay(15s);

		PacketWriter d(packet);

		// ieee 802.15.4 frame control
		d.enum16(ieee::FrameControl::TYPE_DATA
			| ieee::FrameControl::PAN_ID_COMPRESSION
			| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
			| ieee::FrameControl::SOURCE_ADDRESSING_SHORT);
		
		// sequence number
		d.int8(this->macSequenceNumber++);
		
		// destination pan
		d.int16(this->panId);

		// destination address (broadcast)
		d.int16(0xffff);

		// source address
		d.int16(0x0000);


		// header ("string a" for encryption)
		uint8_t const *header = d.current;

		// network layer frame control field
		d.enum16(NwkFrameControl::TYPE_COMMAND
			| NwkFrameControl::VERSION_2
			| NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
			| NwkFrameControl::SECURITY
			| NwkFrameControl::EXTENDED_SOURCE);

		// destination
		d.int16(0xfffc);
		
		// source
		d.int16(0x0000);
		
		// radius
		d.int8(1);
		
		// sequence number
		d.int8(this->nwkSequenceNumber++);
		
		// extended source
		d.int64(this->longAddress);

		
		// security control field
		auto securityControl = SecurityControl::LEVEL_ENC_MIC32
			| SecurityControl::KEY_NETWORK
			| SecurityControl::EXTENDED_NONCE;
		uint8_t *securityControlPtr = d.current;
		d.enum8(securityControl);

		// security counter
		uint32_t securityCounter = this->securityCounter++;
		d.int32(securityCounter);

		// extended source
		uint8_t const *extendedSource = d.current;
		d.int64(this->longAddress);

		// key sequence number
		d.int8(0);

		int headerLength = d.current - header;

		
		// message to encrypt
		uint8_t *message = d.current;

		// command frame
		d.enum8(NwkCommand::LINK_STATUS);
		d.int8(0x40 // last frame
			| 0x20 // first frame
			| 0); // link status count

		// encrypt
		int payloadLength = d.current - message;
		int micLength = 4;
		d.skip(micLength);

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);

		// encrypt in-place
		encrypt(message, nonce, header, headerLength, message, payloadLength, micLength, this->networkKey);

		//bool ok = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, this->networkKey);

		// clear security level according to 4.3.1.1 step 8.
		*securityControlPtr &= ~SecurityControl::LEVEL_MASK;

		// send link status
		uint8_t result;
		co_await radio::send(this->radioIndex, d.finish(), result);
	}
}


// RadioInterface::Device

int RadioInterface::Device::getFlashSize() const {
	int o = offsetof(Device, u);
	switch (this->type) {
	case ZB:
		return o + sizeof(u.shortAddress);
	case GP:
		return o + sizeof(u.key);
	}
	return o;
}

int RadioInterface::Device::getRamSize() const {
	return sizeof(DeviceState);
}

int RadioInterface::Device::getEndpointCount() const {
	int i = 0;
	while (i < array::size(this->endpointTypes) && this->endpointTypes[i] != EndpointType(0))
		++i;
	return i;
}

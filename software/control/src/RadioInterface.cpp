#include "RadioInterface.hpp"
#include <util.hpp>
#include <iostream>
#include <radio.hpp>
#include <rng.hpp>


static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

RadioInterface::RadioInterface(std::function<void (uint8_t, uint8_t const *, int)> const &onReceived)
	: onReceived(onReceived)
{
	// allocate a radio context
	this->radioId = radio::allocate();
	
	// check if there is a coordinator
	if (this->coordinator.empty()) {
		Coordinator coordinator;

		// generate random long address
		//coordinator.longAddress = rng::get64();
		coordinator.longAddress = UINT64_C(0x00124b00214f3c55);

		// todo: scan for free pan
		coordinator.pan = 0x1a62;
		
		this->coordinator.write(0, &coordinator);
	}
	Coordinator const &coordinator = *this->coordinator[0].flash;

	// set long address of coordinator
	radio::setLongAddress(coordinator.longAddress);

	// set pan of coordinator
	radio::setPan(this->radioId, coordinator.pan);

	// set short address of coordinator
	radio::setShortAddress(this->radioId, 0x0000);

	// filter packets with the coordinator as destination (and broadcast pan/address)
	radio::setFlags(this->radioId, radio::TYPE_BEACON | radio::DEST_SHORT | radio::HANDLE_ACK);
	
	// set receive handler
	radio::setReceiveHandler(this->radioId, [this](uint8_t *data) {
		int length = data[0] - 2; // cut off crc
		onRx(data + 1, length);
	});
}

RadioInterface::~RadioInterface() {
}

void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled && this->devices.size() < MAX_DEVICE_COUNT;
	if (this->commissioning) {
		
	}
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

void RadioInterface::onRx(uint8_t *data, int length) {
	uint8_t *d = data;
	uint8_t const *end = data + length;

	// ieee 802.15.4 frame
	uint8_t const *mac = d;
	uint16_t macFrameControl = d[0] | (d[1] << 8);
	d += 2;
	
	// check if data frame and no source address
	if (macFrameControl != 0x0801)
		return;
	
	// mac frame counter
	uint8_t macCounter = d[0];
	++d;
	
	// destination pan id (0xffff is broadcast)
	uint16_t destinationPanId = d[0] | (d[1] << 8);
	d += 2;
	
	// destination address (0xffff is broadcast)
	uint16_t destinationAddress = d[0] | (d[1] << 8);
	d += 2;
	
	// check if broadcast destination address
	if (d >= end || destinationPanId != 0xffff || destinationAddress != 0xffff)
		return;

	// green power frame
	// -----------------
	// NWK header (frame control)
	// Security header (0, 1 or 4 byte counter)
	// NWK payload (optionally encrypted)
	// MIC (0, 2 or 4 byte message integrity code)
	uint8_t *frame = d;

	uint8_t frameControl = d[0];
	++d;
	int protocolVersion = (frameControl >> 2) & 7;
	bool extendedFlag = frameControl & 0x80;

	if (protocolVersion != 3)
		return;

	int applicationId = 0;
	int securityLevel = 0;
	int keyType = 0;
	if (extendedFlag) {
		uint8_t extendedFrameControl = d[0];
		++d;
		
		// application id
		applicationId = extendedFrameControl & 3;
		
		// security level
		// 0: none
		// 1: 1 byte counter and 2 byte MIC
		// 2: 4 byte counter + 4 byte MIC
		// 3: encryption + 4 byte counter + 4 byte MIC
		securityLevel = (extendedFrameControl >> 3) & 3;
		
		// security key type
		// 0: none
		// 1: network key
		keyType = extendedFrameControl >> 5;
	}
	
	uint32_t deviceId = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
	//printf("deviceId: %08x\n", deviceId);
	d += 4;

	if (this->commissioning && securityLevel == 0 && d[0] == 0xe0) {
		// commissioning
		commission(deviceId, d + 1, end);
		return;
	}

	// search device
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			Device const &device = *e.flash;
			DeviceState &state = *e.ram;
						
			if (securityLevel == 1) {
				// security level 1: 1 byte counter, 2 byte mic

				// security counter (use mac sequence number)
				uint32_t counter = mac[2];

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at mac sequence number and includes also payload
				uint8_t const *header = mac + 2;
				int micLength = 2;
				int headerLength = end - micLength - header;
				if (headerLength < 1)
					return;
					
				// decrypt and check
				if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.u.key)) {
					//printf("error while decrypting message!\n");
					return;
				}
				
				// remove mic from end
				end -= 2;
			} else if (securityLevel >= 2) {
				// security levels 2 and 3: 4 byte counter, 4 byte mic
				
				// security counter
				uint32_t counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
				d += 4;

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at frame (frameControl) and either includes payload (level 2) or not (level 3)
				uint8_t *header = frame;
				int micLength = 4;
				int headerAndPayloadLength = end - micLength - header;
				int headerLength = securityLevel == 2 ? headerAndPayloadLength : d - header;
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
				end -= 4;
			}

			switch (device.deviceType) {
			case 0x02:
				if (d < end) {
					int endpointIndex = -1;
					uint8_t data = 0;
					uint8_t command = d[0];
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
				if (d + 2 <= end) {
					uint8_t buttons = d[1];
					
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

void RadioInterface::commission(uint32_t deviceId, uint8_t *d, uint8_t const *end) {
	Device device;
	device.deviceId = deviceId;
	device.type = Device::GREEN_POWER;
	DeviceState deviceState = {};
	
	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	device.deviceType = d[0];
	++d;

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
	uint8_t options = d[0];
	++d;
	bool incrementalCounterFlag = options & 1;
	bool extendedFlag = options & 0x80;
	
	if (extendedFlag) {
		uint8_t extendedOptions = d[0];
		++d;

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
			uint8_t *key = d;
			if (keyEncryptedFlag) {
				// decrypt key, Green Power A.1.5.3.3.3
				
				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);

				if (!decrypt(key, nonce, header.data, 4, d, 16, 4, defaultAesKey)) {
					//printf("error while decrypting key!\n");
					return;
				}				

				// skip key and MIC
				d += 16 + 4;
			} else {
				// skip key
				d += 16;
			}
			
			// set key for device
			setKey(device.u.key, key);
		}
		if (counterFlag) {
			deviceState.counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
			d += 4;
			//printf("counter: %08x\n", counter);
		}
	}

	if (d > end)
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


// RadioInterface::Coordinator

int RadioInterface::Coordinator::getFlashSize() const {
	return sizeof(Coordinator);
}

int RadioInterface::Coordinator::getRamSize() const {
	return sizeof(CoordinatorState);
};


// RadioInterface::Device

int RadioInterface::Device::getFlashSize() const {
	int o = offsetof(Device, u);
	switch (this->type) {
	case ZIGBEE:
		return o + sizeof(u.shortAddress);
	case GREEN_POWER:
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

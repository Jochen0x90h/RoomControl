#include "RadioInterface.hpp"
#include <util.hpp>
#include <iostream>
#include <radio.hpp>


static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

RadioInterface::RadioInterface(Storage &storage, std::function<void (uint8_t, uint8_t const *, int)> const &onReceived)
	: devices(storage)
	, onReceived(onReceived)
{
	// set handler for read requests by devices
	radio::addReceiveHandler([this](uint8_t *data, int length) {onRx(data, length);});
}

RadioInterface::~RadioInterface() {
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

void RadioInterface::onRx(uint8_t *data, int length) {
	uint8_t *d = data;
	uint8_t const *end = data + length;
	
	// mac header
	uint8_t *mac = d;
	d += 7;
	uint16_t macFrameControl = mac[0] | (mac[1] << 8);
	uint16_t destinationPanId = mac[3] | (mac[4] << 8);
	uint16_t destinationAddress = mac[5] | (mac[6] << 8);
	
	// check if data frame, broadcast destination and no source
	if (d >= end || macFrameControl != 0x0801 || destinationPanId != 0xffff || destinationAddress != 0xffff)
		return;
	
	// frame
	// -----
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
				GreenPowerNonce nonce(deviceId, counter);

				// header starts at mac sequence number and includes also payload
				uint8_t const *header = mac + 2;
				int micLength = 2;
				int headerLength = end - micLength - header;
				if (headerLength < 1)
					return;
					
				// decrypt and check
				if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.aesKey)) {
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
				GreenPowerNonce nonce(deviceId, counter);

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
				if (!decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, device.aesKey)) {
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
					int command = d[0];
					switch (command & ~0x04) {
					case 0x10:
						endpointIndex = 0;
						data = 1;
						break;
					case 0x11:
						endpointIndex = 0;
						data = 2;
						break;
					case 0x13:
						endpointIndex = 1;
						data = 1;
						break;
					case 0x12:
						endpointIndex = 1;
						data = 2;
						break;
					}
					if (command & 0x04)
						data = 0;
					if (endpointIndex != -1 && state.referenceCounters[endpointIndex] > 0)
						this->onReceived(state.firstEndpointId + endpointIndex, &data, 1);
				}
				break;
			}
			
			break;
		}
	}
}

void RadioInterface::commission(uint32_t deviceId, uint8_t *d, uint8_t const *end) {
	Device device;
	device.deviceId = deviceId;
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
		device.endpointTypes[2] = EndpointType(0);
		break;
	//case 0x07:
		// generic switch, PTM216Z
		
		//break;
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
				GreenPowerNonce nonce(deviceId, deviceId);

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
			setKey(device.aesKey, key);
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

int RadioInterface::Device::getFlashSize() const {
	return sizeof(Device);
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

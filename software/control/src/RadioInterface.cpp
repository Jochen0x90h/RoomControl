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
	radio::addReceiveHandler([this](uint8_t *data) {onRx(data);});
}

RadioInterface::~RadioInterface() {
}

void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled;
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

void RadioInterface::onRx(uint8_t *data) {
	// mac header
	uint8_t *mac = data + 1;
	uint16_t macFrameControl = mac[0] | (mac[1] << 8);
	uint16_t destinationPanId = mac[3] | (mac[4] << 8);
	uint16_t destinationAddress = mac[5] | (mac[6] << 8);
	
	// check if data frame, broadcast destination and no source
	if (macFrameControl != 0x0801 || destinationPanId != 0xffff || destinationAddress != 0xffff)
		return;
		
	int length = data[0] - 2 + 1;

	// frame
	// -----
	// NWK header (frame control)
	// Security header (0, 1 or 4 byte counter)
	// NWK payload (optionally encrypted)
	// MIC (0, 2 or 4 byte message integrity code)
	uint8_t *frame = data + 8;
	int frameLength = length - 8;
	uint8_t *d = frame;
	int l = frameLength;

	uint8_t frameControl = d[0];

	// protocol version
	int version = (frameControl >> 2) & 7;
	
	++d;
	--l;

	int applicationId = 0;
	int securityLevel = 0;
	int keyType = 0;
	if (frameControl & 0x80) {
		uint8_t extendedFrameControl = d[0];
		
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
		
		++d;
		--l;
	}

	// check if space for deviceId, command and mic
	if (version != 3 && l < 4 + 1 + 4)
		return;
	
	uint32_t deviceId = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
	//printf("deviceId: %08x\n", deviceId);
	d += 4;
	l -= 4;

	if (this->commissioning && securityLevel == 0 && d[0] == 0xe0 && this->devices.size() < MAX_DEVICE_COUNT) {
		// commissioning
		commission(deviceId, d + 1, l - 1);
		return;
	}

	// search device
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			Device const &device = *e.flash;
			DeviceState &state = *e.ram;
						
			if (securityLevel == 1) {
				// length must be at least 1 (counter) + 1 (command) + 2 (MIC)
				if (l < 1 + 1 + 2)
					return;

				// security counter (use mac sequence number)
				uint32_t counter = mac[2];

				// header starts at mac sequence number
				uint8_t *header = mac + 2;
				int headerLength = length - 2 - 2; // 2 for mac frame control at beginning, 2 for mic at end

				// decrypt and check
				if (!decrypt(deviceId, counter, header, headerLength, header + headerLength, 0, 2, device.aesKey)) {
					//printf("error while decrypting message!\n");
					return;
				}
				l -= 2;
			} else if (securityLevel >= 2) {
				// length must be at least 4 (counter) + 1 (command) + 4 (MIC)
				if (l < 4 + 1 + 4)
					return;

				// security counter
				uint32_t counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
				
				// header starts at frame (frameControl) and either includes payload (level 2) or not (level 3)
				uint8_t *header = frame;
				int headerLength = securityLevel == 2 ? frameLength - 4 : d + 4 - frame;

				// decrypt and check
				if (!decrypt(deviceId, counter, header, headerLength, header + headerLength,
					frameLength - 4 - headerLength, 4, device.aesKey)) {
					//printf("error while decrypting message!\n");
					return;
				}
					
				d += 4;
				l -= 8;
			}

			switch (device.deviceType) {
			case 0x02:
				{
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

void RadioInterface::commission(uint32_t deviceId, uint8_t *data, int length) {
	if (length < 27)
		return;
	
	Device device;
	device.deviceId = deviceId;
	DeviceState deviceState = {};
	
	// A.4.2.1.1 Commissioning
	
	uint8_t *d = data;
	int l = length;
	
	// device type
	// 0x02: on/off switch
	device.deviceType = d[0];
	++d;
	--l;
	device.endpointTypes[0] = EndpointType(0);
	switch (device.deviceType) {
	case 0x02:
		// switch, PTM215Z ("friends of hue")
		device.endpointTypes[0] = EndpointType::ROCKER;
		device.endpointTypes[1] = EndpointType::ROCKER;
		device.endpointTypes[2] = EndpointType(0);
		break;
	case 0x07:
		// generic switch, PTM216Z
		
		break;
	}
	
	// check if device type is known
	if (device.endpointTypes[0] == EndpointType(0))
		return;

	// options
	uint8_t options = d[0];
	++d;
	--l;
	bool incrementalCounter = options & 1;
	if (options & 0x80) {
		uint8_t extendedOptions = d[0];
		++d;
		--l;

		// security level for messages
		// 0: none
		// 1: 1 byte counter and 2 byte MIC
		// 2: 4 byte counter + 4 byte MIC
		// 3: encryption + 4 byte counter + 4 byte MIC
		int securityLevel = extendedOptions & 3;

		// key type
		int keyType = (extendedOptions >> 2) & 7;
		
		// 128 bit (16 byte) key follows
		bool keyPresent = extendedOptions & 0x20;
		
		// key is encrypted and additional 4 byte MIC follows
		bool keyEncrypted = extendedOptions & 0x40;
		
		// current value of security counter follows
		bool counterPresent = extendedOptions & 0x80;
	
		if (keyPresent) {
			uint8_t *key = d;
			d += 16;
			l -= 16;
			if (keyEncrypted) {
				// decrypt key
				//setKey(aesKey, defaultKey);

				// Green Power A.1.5.3.3.3
				uint8_t header[4];
				header[0] = deviceId;
				header[1] = deviceId >> 8;
				header[2] = deviceId >> 16;
				header[3] = deviceId >> 24;
				if (!decrypt(deviceId, deviceId, header, 4, key, 16, 4, defaultAesKey)) {
					//printf("error while decrypting key!\n");
					return;
				}
				
				// skip MIC
				d += 4;
				l -= 4;
			}
			
			// set key
			setKey(device.aesKey, key);
/*
			printf("key: ");
			for (int i = 0; i < 16; ++i) {
				printf("%02x ", key[i]);
			}
			printf("\n");
*/
		}
		if (counterPresent) {
			deviceState.counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
			d += 4;
			l -= 4;
			//printf("counter: %08x\n", counter);
		}
	}

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

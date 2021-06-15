#include "BusInterface.hpp"
#include <util.hpp>
#include <iostream>
#include <bus.hpp>


BusInterface::BusInterface(std::function<void ()> const &onSent,
	std::function<void (uint8_t, uint8_t const *, int)> const &onReceived)
	: onSent(onSent), onReceived(onReceived), referenceCounters{}
{
	// start first enumeration
	this->endpointStarts[0] = 0;
	enumerate();
	
	// set handler for read requests by devices
	bus::setRequestHandler([this](uint32_t endpointId) {onRequest(endpointId);});
}

BusInterface::~BusInterface() {
}

void BusInterface::setCommissioning(bool enabled) {
	if (enabled) {
		// enumerate auto-commissioning devices
		
	}
}

int BusInterface::getDeviceCount() {
	return this->deviceCount;
}

DeviceId BusInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->deviceCount);
	return this->deviceIds[index];
}

Array<EndpointType> BusInterface::getEndpoints(DeviceId deviceId) {
	for (int i = 0; i < this->deviceCount; ++i) {
		if (deviceId == this->deviceIds[i]) {
			int start = this->endpointStarts[i];
			int len = this->endpointStarts[i + 1] - start;
			return {reinterpret_cast<EndpointType *>(&this->endpointTypes[start]), len};
		}
	}
	return {};
}

void BusInterface::subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	for (int deviceIndex = 0; deviceIndex < this->deviceCount; ++deviceIndex) {
		if (this->deviceIds[deviceIndex] == deviceId) {
			int start = this->endpointStarts[deviceIndex];

			int newEndpointId = 2 + MAX_DEVICE_COUNT + start + endpointIndex;

			if (endpointId == 0) {
				++this->referenceCounters[newEndpointId - (2 + MAX_DEVICE_COUNT)];
				if (this->txQueue.hasSpace(4)) {
					auto tx = this->txQueue.add(false, 4);
					tx.data[0] = 2 + deviceIndex;
					tx.data[1] = endpointIndex;
					tx.data[2] = newEndpointId;
					tx.data[3] = calcChecksum(tx.data, 3);
					
					if (!this->busy) {
						this->busy = true;
						bus::transfer(tx.data, 4, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
					}
				}
			} else {
				assert(endpointId == newEndpointId);
			}
			endpointId = newEndpointId;
			
			break;
		}
	}
}

void BusInterface::unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	if (endpointId != 0) {
		for (int deviceIndex = 0; deviceIndex < this->deviceCount; ++deviceIndex) {
			if (this->deviceIds[deviceIndex] == deviceId) {
				int referenceCounterIndex = endpointId - (2 + MAX_DEVICE_COUNT);
				assert(referenceCounterIndex >= 0 && referenceCounterIndex < MAX_ENDPOINT_COUNT);
				assert(this->referenceCounters[referenceCounterIndex] > 0);
				if (--this->referenceCounters[referenceCounterIndex] == 0) {
					if (this->txQueue.hasSpace(4)) {
						auto tx = this->txQueue.add(false, 4);
						tx.data[0] = 2 + deviceIndex;
						tx.data[1] = endpointIndex;
						tx.data[2] = 0; // zero as new endpoint id means unsubscribe
						tx.data[3] = calcChecksum(tx.data, 3);

						if (!this->busy) {
							this->busy = true;
							bus::transfer(tx.data, 4, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
						}
					}
				}
				endpointId = 0;
			}
		}
	}
}

void BusInterface::send(uint8_t endpointId, uint8_t const *data, int length) {
	if (endpointId > 2 + this->deviceCount && this->txQueue.hasSpace(length + 2)) {
		auto tx = this->txQueue.add(false, length + 2);
		tx.data[0] = endpointId;
		array::copy(tx.data + 1, tx.data + 1 + length, data);
		tx.data[length + 1] = calcChecksum(tx.data, length + 1);

		if (!this->busy) {
			this->busy = true;
			bus::transfer(tx.data, tx.length, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
		}
	}
}

void BusInterface::enumerate() {
	if (this->deviceCount < MAX_DEVICE_COUNT) {
		int endpointStart = this->endpointStarts[this->deviceCount];

		// start next enumeration
		uint8_t *txData = this->txQueue.data();
		txData[0] = 0;
		int rxLength = array::size(this->endpointTypes) - endpointStart;
		bus::transfer(txData, 1, this->endpointTypes + endpointStart, rxLength, [this](int rxLength) {
			onEnumerated(rxLength);
		});
	} else {
		// maximum device count reached: Assume that we are finished
		this->onSent();
	}
}

void BusInterface::onEnumerated(int rxLength) {
	int endpointStart = this->endpointStarts[this->deviceCount];
	
	// rxData is endpoint id (0), device id (4 byte), endpoint types (N byte), checksum (1 byte)
	uint8_t *rxData = &this->endpointTypes[endpointStart];

	if (rxLength <= 1 || rxData[rxLength - 1] != calcChecksum(rxData, rxLength - 1)) {
		// error or no more devices (we don't know if the devices just didn't "hear" the enumerate request)
		if (this->retryCount >= 3) {
			// assume that we are finished
			this->retryCount = 0;
			this->onSent();
		} else {
			// try again
			++this->retryCount;
			enumerate();
		}
	} else {
		// successfully enumerated a device
		uint32_t deviceId = (rxData[4] << 24) | (rxData[3] << 16) | (rxData[2] << 8) | rxData[1];
		
		// check if device already exists
		int deviceIndex = 0;
		while (deviceIndex < this->deviceCount && deviceId != this->deviceIds[deviceIndex])
			++deviceIndex;
		
		if (deviceIndex == this->deviceCount) {
			// new device
			this->deviceIds[deviceIndex] = deviceId;
		
			// erase endpoint id and device id
			array::erase(rxData, rxData + rxLength, 5);
		
			// set endpoint start for next device
			endpointStart += rxLength - 6;
			this->endpointStarts[++this->deviceCount] = endpointStart;
		
			this->retryCount = 0;
		}
	
		// set device endpoint
		int txIndex = 0;
		uint8_t *txData = this->txQueue.data();
		txData[txIndex++] = 1;
		txData[txIndex++] = deviceId;
		txData[txIndex++] = deviceId >> 8;
		txData[txIndex++] = deviceId >> 16;
		txData[txIndex++] = deviceId >> 24;
		txData[txIndex++] = 2 + deviceIndex;
		txData[txIndex] = calcChecksum(txData, txIndex);
		++txIndex;
		bus::transfer(txData, txIndex, this->rxData, 10, [this](int rxLength) {
			enumerate();
		});
	}
}

void BusInterface::onTransferred(int rxLength) {
	auto tx = this->txQueue.get();
	
	// todo check if transfer was successful
	if (rxLength < 2 || this->rxData[rxLength - 1] != calcChecksum(this->rxData, rxLength - 1)) {
		// transfer failed
	
		// retry several times
		if (this->retryCount < 3) {
			++this->retryCount;
			bus::transfer(tx.data, tx.length, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
			return;
		} else {
			// todo: report error
		}
	} else if (tx.info) {
		// this was a read request
		uint8_t endpointId = this->rxData[0];
		this->onReceived(endpointId, &this->rxData[1], rxLength - 2);
	}
	this->retryCount = 0;
	
	// remove transferred message from queue
	this->txQueue.remove();

	// check if more to transfer
	if (!this->txQueue.isEmpty()) {
		auto tx = this->txQueue.get();
		bus::transfer(tx.data, tx.length, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
	} else {
		this->busy = false;
		this->onSent();
	}
}

void BusInterface::onRequest(uint8_t endpointId) {
	auto tx = this->txQueue.add(true, 1);
	tx.data[0] = endpointId;

	// gets called when no bus transfer is in progress
	this->busy = true;
	bus::transfer(tx.data, 1, this->rxData, 10, [this](int rxLength) {onTransferred(rxLength);});
}


static uint8_t const crc8Table[256] = {0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, 0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d, 0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd, 0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd, 0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea, 0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a, 0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a, 0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a, 0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4, 0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4, 0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44, 0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34, 0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6A, 0x6d, 0x64, 0x63, 0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8D, 0x84, 0x83, 0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3};

uint8_t BusInterface::calcChecksum(const uint8_t *data, int length) {
	uint8_t crc = 0;
	for (int i = 0 ; i < length ; ++i)
		crc = crc8Table[crc ^ data[i]];
	return crc;
}

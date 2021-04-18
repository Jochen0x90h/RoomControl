#include <map>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>
#include <util.hpp>
#include <crypt.hpp>
#include <DataBuffer.hpp>


// test for PTM215Z, is a usb host and needs radioSniffer on the device


// transfer direction
enum UsbDirection {
	USB_OUT = 0, // to device
	USB_IN = 0x80 // to host
};


// Green Power A.3.3.3.3
// 5A:69:67:42:65:65:41:6C:6C:69:61:6E:63:65:30:39
static uint8_t const zigbeeAlliance09Key[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};
AesKey defaultAesKey;

void printAesKey(AesKey aesKey) {
	printf("{{");
	for (int i = 0; i < array::size(aesKey.words); ++i) {
		if (i > 0)
			printf(", ");
		printf("0x%08x", aesKey.words[i]);
	}
	printf("}};\n");
}

struct Device {
	AesKey aesKey;
};

std::map<uint32_t, Device> devices;

void commission(uint32_t deviceId, uint8_t *d, uint8_t const *end, AesKey &aesKey) {
	Device device;

	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	uint8_t deviceType = d[0];
	++d;

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
				// Green Power A.1.5.3.3.3
				
				// nonce
				GreenPowerNonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);
				
				if (!decrypt(key, nonce, header.data, 4, d, 16, 4, defaultAesKey)) {
					printf("Error while decrypting key!\n");
					return;
				}
				
				// skip key and MIC
				d += 16 + 4;
			} else {
				// skip key
				d += 16;
			}

			// print key
			printf("Key: ");
			for (int i = 0; i < 16; ++i) {
				if (i > 0)
					printf(":");
				printf("%02x", key[i]);
			}
			printf("\n");

			// set key for device
			setKey(device.aesKey, key);
		}
		if (counterFlag) {
			uint32_t counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
			d += 4;
			printf("Counter: %08x\n", counter);
		}
	}

	if (d > end)
		return;

	switch (deviceType) {
	case 0x02:
		// hue switch
		
		break;
	case 0x07:
		// generic switch
		
		break;
	}
	
	devices[deviceId] = device;
}

int main(void) {
	setKey(defaultAesKey, zigbeeAlliance09Key);
	//printAesKey(defaultAesKey);

	libusb_device **devs;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		libusb_exit(NULL);
		return (int) cnt;
	}

	for (int i = 0; devs[i]; ++i) {
		libusb_device *dev = devs[i];
		libusb_device_descriptor desc;

		int ret = libusb_get_device_descriptor(dev, &desc);
		if (ret < 0) {
			fprintf(stderr, "Failed to get device descriptor");
			return -1;
		}
		if (desc.idVendor == 0x1915 && desc.idProduct == 0x1337) {
			libusb_device_handle *handle;
			ret = libusb_open(dev, &handle);
			if (ret == LIBUSB_SUCCESS) {

				// set configuration (reset alt_setting, reset toggles)
				libusb_set_configuration(handle, 1);

				// claim interface with bInterfaceNumber = 0
				ret = libusb_claim_interface(handle, 0);
				//printf("claim interface %d\n", ret);

				//ret = libusb_set_interface_alt_setting(handle, 0, 0);
				//printf("set alternate setting %d\n", ret);

				// loop
				printf("Waiting for commissioning message (press A0 for 7 seconds, then A1 and B0 to commission PTM215Z)\n");
				uint8_t data[128] = {};
				int length;
				AesKey aesKey;
				while (true) {
					// receive from device (we get back the same data that we sent)
					ret = libusb_bulk_transfer(handle, USB_IN | 1, data, 128, &length, 0);
					if (ret < 0)
						break;
				
					uint8_t *d = data;
					uint8_t const *end = data + length;
					
					// ieee 802.15.4 frame
					uint8_t const *mac = d;
					uint16_t macFrameControl = d[0] | (d[1] << 8);
					d += 2;
					
					// check if data frame
					if (macFrameControl != 0x0801)
						continue;
					
					// mac frame counter
					uint8_t macCounter = d[0];
					++d;
					
					// destination pan id (0xffff is broadcast)
					uint16_t destinationPanId = d[0] | (d[1] << 8);
					d += 2;
					
					// destination address (0xffff is broadcast)
					uint16_t destinationAddress = d[0] | (d[1] << 8);
					d += 2;
					
					// check if broadcast destination and no source
					if (d >= end || destinationPanId != 0xffff || destinationAddress != 0xffff)
						continue;

					// print packet
					printf("%d: ", length);
					for (int i = 0; i < length; ++i) {
						printf("%02x ", data[i]);
					}
					printf("\n");

					// green power frame
					// ----------------
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
						continue;

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
					printf("Device Id: %08x\n", deviceId);
					d += 4;

					if (securityLevel == 0 && d[0] == 0xe0) {
						// commissioning
						commission(deviceId, d + 1, end, aesKey);
						continue;
					}

					// search device
					auto it = devices.find(deviceId);
					if (it == devices.end()) {
						printf("Device not found!\n");
						continue;
					}
					Device &device = it->second;

					// security
					// --------
					// header: header that is not encrypted, payload is part of header for security levels 0 and 1
					// payload: payload that is encrypted, has zero length for security levels 0 and 1
					// mic: message integrity code, 2 or 4 bytes
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
							continue;
							
						// decrypt and check
						if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.aesKey)) {
							printf("error while decrypting message!\n");
							continue;
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
							continue;

						uint8_t *message = header + headerLength;
						int payloadLength = headerAndPayloadLength - headerLength;

						// decrypt in-place and check
						if (!decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, device.aesKey)) {
							printf("error while decrypting message!\n");
							continue;
						}

						// remove mic from end
						end -= 4;
					}

					printf("Data:");
					int l = end - d;
					for (int i = 0; i < l; ++i)
						printf(" %02x", d[i]);
					printf("\n");
				}
				printf("USB Error: %d\n", ret);
			}
		}
	}
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return 0;
}


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
static uint8_t const defaultKey[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};

void commission(uint32_t deviceId, uint8_t *data, int length, AesKey &aesKey) {
	if (length < 27)
		return;
	
	// A.4.2.1.1 Commissioning
	
	uint8_t *d = data;
	int l = length;
	
	// device type
	// 0x02: on/off switch
	uint8_t deviceType = d[0];
	++d;
	--l;

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
			uint8_t *key = d;//data + 3;
			d += 16;
			l -= 16;
			if (keyEncrypted) {
				// decrypt key
				setKey(aesKey, defaultKey);

				// Green Power A.1.5.3.3.3
				uint8_t header[4];
				header[0] = deviceId;
				header[1] = deviceId >> 8;
				header[2] = deviceId >> 16;
				header[3] = deviceId >> 24;
				if (!crypt(deviceId, deviceId, header, 4, key, 16, 4, aesKey)) {
					printf("error while decrypting key!\n");
					return;
				}
				
				// skip MIC
				d += 4;
				l -= 4;
			}
			
			// set key
			setKey(aesKey, key);

			printf("key: ");
			for (int i = 0; i < 16; ++i) {
				printf("%02x ", key[i]);
			}
			printf("\n");
		}
		if (counterPresent) {
			uint32_t counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
			d += 4;
			l -= 4;
			printf("counter: %08x\n", counter);
		}
	}


	switch (deviceType) {
	case 0x02:
		// hue switch
		
		break;
	case 0x07:
		// generic switch
		
		break;
	}
}

int main(void) {
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
			fprintf(stderr, "failed to get device descriptor");
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
				printf("waiting for commissioning message (press A0 for 7 seconds)\n");
				uint8_t data[128] = {};
				int transferred;
				AesKey aesKey;
				while (true) {
					// receive from device (we get back the same data that we sent)
					ret = libusb_bulk_transfer(handle, USB_IN | 1, data, 128, &transferred, 0);
					
					// packet
					printf("%d: ", transferred);
					for (int i = 0; i < transferred; ++i) {
						printf("%02x ", data[i]);
					}
					printf("\n");
					
					if (transferred >= 9) {
						// frame
						// -----
						// NWK header (frame control)
						// Security header (0, 1 or 4 byte counter)
						// NWK payload (optionally encrypted)
						// MIC (0, 2 or 4 byte message integrity code)
						uint8_t *frame = data + 8;
						int frameLength = transferred - 8;
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
							applicationId = d[0] & 3;
							
							// security level
							// 0: none
							// 1: 1 byte counter and 2 byte MIC
							// 2: 4 byte counter + 4 byte MIC
							// 3: encryption + 4 byte counter + 4 byte MIC
							securityLevel = (d[0] >> 3) & 3;
							
							// security key type
							// 0: none
							// 1: network key
							keyType = d[0] >> 5;
							
							++d;
							--l;
						}

						if (version == 3 && l >= 5) {
							uint32_t deviceId = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
							printf("deviceId: %08x\n", deviceId);
							d += 4;
							l -= 4;

							if (securityLevel == 1) {
								// length must be at least 1 (counter) + 1 (command) + 2 (MIC)
								if (l < 1 + 1 + 2)
									continue;

								// we start at mac sequence numter
								uint8_t *mac = data + 3;
								int macLength = transferred - 3;

								// security counter
								uint32_t counter = mac[0];

								// decrypt and check
								int headerLength = macLength - 2;
								if (!crypt(deviceId, counter, mac, headerLength, mac + headerLength, 0, 2, aesKey)) {
									printf("error while decrypting message!\n");
									continue;
								}
								l -= 2;
							} else if (securityLevel >= 2) {
								// length must be at least 4 (counter) + 1 (command) + 4 (MIC)
								if (l < 4 + 1 + 4)
									continue;

								// security counter
								uint32_t counter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
								
								// decrypt and check
								int headerLength = securityLevel == 2 ? frameLength - 4 : d + 4 - frame;
								if (!crypt(deviceId, counter, frame, headerLength, frame + headerLength,
									frameLength - 4 - headerLength, 4, aesKey)) {
									printf("error while decrypting message!\n");
									continue;
								}
									
								d += 4;
								l -= 8;
							}

							uint8_t command = d[0];
							if (command == 0xe0) {
								// commissioning
								commission(deviceId, d + 1, l - 1, aesKey);
							} else {
								printf("data:");
								for (int i = 0; i < l; ++i)
									printf(" %02x", d[i]);
								printf("\n");
							}
						}
					}
				}
			}
		}
	}


	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
}

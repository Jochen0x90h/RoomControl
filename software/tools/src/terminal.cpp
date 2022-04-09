#include <usb.hpp>
#include <stdio.h>
#include <libusb.h>
#include <util.hpp>


// prints everything that comes via usb from the device, e.g. radioDevice or bme680Test

int main(void) {
	int r = libusb_init(NULL);
	if (r < 0)
		return r;

	// get device list
	libusb_device **devs;
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		libusb_exit(NULL);
		return (int) cnt;
	}

	// iterate over devices
	for (int i = 0; devs[i]; ++i) {
		libusb_device *dev = devs[i];

		// get device descriptor
		libusb_device_descriptor desc;
		int ret = libusb_get_device_descriptor(dev, &desc);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "get device descriptor error: %d\n", ret);
			continue;
		}
		
		// check vendor/product
		if (desc.idVendor != 0x1915 && desc.idProduct != 0x1337)
			continue;

		// protocol: 0: binary, 1: text
		bool text = desc.bDeviceProtocol == 1;

		// open device
		libusb_device_handle *handle;
		ret = libusb_open(dev, &handle);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "open error: %d\n", ret);
			continue;
		}
			
		// set configuration (reset alt_setting, reset toggles)
		ret = libusb_set_configuration(handle, 1);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "set configuration error: %d\n", ret);
			continue;
		}

		// claim interface with bInterfaceNumber = 0
		ret = libusb_claim_interface(handle, 0);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "claim interface error: %d\n", ret);
			continue;
		}

		//ret = libusb_set_interface_alt_setting(handle, 0, 0);
		//printf("set alternate setting %d\n", ret);

		// loop
		printf("waiting for input from %s device ...\n", text ? "text" : "binary");
		uint8_t data[257] = {};
		int length;
		while (true) {
			// receive from device (we get back the same data that we sent)
			ret = libusb_bulk_transfer(handle, 1 | usb::IN, data, 256, &length, 0);
			if (ret != LIBUSB_SUCCESS) {
				fprintf(stderr, "transfer error: %d\n", ret);
				break;
			}
			data[length] = 0;
			
			if (text) {
				// text
				printf("%s\n", data);
			} else {
				// binary
				printf("%d:\n", length);
				for (int i = 0; i < length; ++i) {
					if ((i & 15) == 0) {
						if (i != 0)
							printf(",\n");
					} else {
						printf(", ");
					}
					printf("%02x", data[i]);
					//printf("0x%02x", data[i]);
				}
				printf("\n");
			}
		}
		break;
	}
	
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return 0;
}

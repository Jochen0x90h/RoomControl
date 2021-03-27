#include <stdio.h>
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
				// protocol: 0: binary, 1: text
				bool text = desc.bDeviceProtocol == 1;

				// set configuration (reset alt_setting, reset toggles)
				libusb_set_configuration(handle, 1);

				// claim interface with bInterfaceNumber = 0
				ret = libusb_claim_interface(handle, 0);
				//printf("claim interface %d\n", ret);

				//ret = libusb_set_interface_alt_setting(handle, 0, 0);
				//printf("set alternate setting %d\n", ret);

				// loop
				printf("waiting for binary or text input from device ...\n");
				uint8_t data[257] = {};
				int length;
				while (true) {
					// receive from device (we get back the same data that we sent)
					ret = libusb_bulk_transfer(handle, USB_IN | 1, data, 256, &length, 0);
					if (ret < 0)
						break;
					data[length] = 0;
					
					if (text) {
						// text
						printf("%s\n", data);
					} else {
						// binary
						printf("%d: ", length);
						for (int i = 0; i < length; ++i) {
							if ((i & 15) == 0) {
								if (i != 0)
									printf(",\n");
							} else {
								printf(", ");
							}
							printf("0x%02x", data[i]);
						}
						printf("\n");
					}
				}
				printf("error: %d\n", ret);
			}
		}
	}
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return 0;
}


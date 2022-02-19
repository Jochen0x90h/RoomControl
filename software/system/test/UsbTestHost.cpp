#include <UsbDefs.hpp>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>


// host for usbTest
// flash usbTest onto the device, then run usbTestHost


// https://github.com/libusb/libusb/blob/master/examples/listdevs.c
static void printDevices(libusb_device **devs) {
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8]; 

	// iterate over devices
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		printf("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			printf(" path: %d", path[0]);
			for (j = 1; j < r; j++)
				printf(".%d", path[j]);
		}
		printf("\n");
	}
}

// https://github.com/libusb/libusb/blob/master/examples/testlibusb.c
static int printDevice(libusb_device *dev) {
	libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	unsigned char string[256];
	int ret;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}

	printf("%04x:%04x\n", desc.idVendor, desc.idProduct);
	printf("\tBus: %d\n", libusb_get_bus_number(dev));
	printf("\tDevice: %d\n", libusb_get_device_address(dev));
	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		printf("\tOpen\n");

		// manufacturer
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("\t\tManufacturer: %s\n", string);
		}

		// product
		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("\t\tProduct: %s\n", string);
		}
		
	
		libusb_close(handle);
	} else {
		printf("\tOpen error: %d\n", ret);
	}

		// configurations
		for (int i = 0; i < desc.bNumConfigurations; i++) {
			libusb_config_descriptor *config;
			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS == ret) {
				printf("\tConfiguration[%d]\n", i);
				printf("\t\tTotalLength:         %d\n", config->wTotalLength);
				printf("\t\tNumInterfaces:       %d\n", config->bNumInterfaces);
				printf("\t\tConfigurationValue:  %d\n", config->bConfigurationValue);
				printf("\t\tConfiguration:       %d\n", config->iConfiguration);
				printf("\t\tAttributes:          %02xh\n", config->bmAttributes);
				printf("\t\tMaxPower:            %d\n", config->MaxPower);

			}
			
			// interfaces
			for (int j = 0; j < config->bNumInterfaces; j++) {
				libusb_interface const & interface = config->interface[j];
				
				// alternate settings
				for (int k = 0; k < interface.num_altsetting; k++) {
					libusb_interface_descriptor const & descriptor = interface.altsetting[k];

					printf("\t\tInterface[%d][%d]\n", j, k);
					//printf("\t\t\tInterfaceNumber:   %d\n", descriptor.bInterfaceNumber);
					//printf("\t\t\tAlternateSetting:  %d\n", descriptor.bAlternateSetting);
					printf("\t\t\tNumEndpoints:      %d\n", descriptor.bNumEndpoints);
					printf("\t\t\tInterfaceClass:    %d\n", descriptor.bInterfaceClass);
					printf("\t\t\tInterfaceSubClass: %d\n", descriptor.bInterfaceSubClass);
					printf("\t\t\tInterfaceProtocol: %d\n", descriptor.bInterfaceProtocol);
					printf("\t\t\tInterface:         %d\n", descriptor.iInterface);

					// endpoints
					for (int l = 0; l < descriptor.bNumEndpoints; l++) {
						libusb_endpoint_descriptor const & endpoint = descriptor.endpoint[l];
						
						printf("\t\t\tEndpoint[%d]\n", l);
						printf("\t\t\t\tEndpointAddress: %02xh\n", endpoint.bEndpointAddress);
						printf("\t\t\t\tAttributes:      %02xh\n", endpoint.bmAttributes);
						printf("\t\t\t\tMaxPacketSize:   %d\n", endpoint.wMaxPacketSize);
						printf("\t\t\t\tInterval:        %d\n", endpoint.bInterval);
						printf("\t\t\t\tRefresh:         %d\n", endpoint.bRefresh);
						printf("\t\t\t\tSynchAddress:    %d\n", endpoint.bSynchAddress);
					}
				}
			
			}
			
			libusb_free_config_descriptor(config);
		}


	return 0;
}

// vendor specific control request
enum class Request : uint8_t {
	RED = 0,
	GREEN = 1,
	BLUE = 2
};

// sent vendor specific control request to usb device
int controlOut(libusb_device_handle *handle, Request request, uint16_t wValue, uint16_t wIndex) {
	return libusb_control_transfer(handle,
		Usb::OUT | Usb::REQUEST_TYPE_VENDOR | Usb::RECIPIENT_INTERFACE,
		uint8_t(request),
		wValue,
		wIndex,
		nullptr,
		0,
		1000);
}

// print test status
void printStatus(char const* message, bool status) {
	printf("%s: %s\n", message, status ? "ok" : "error");
}

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
/*
	// print list of devices
	printDevices(devs);
	for (int i = 0; devs[i]; ++i) {
		printDevice(devs[i]);
	}
*/
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

		// print device
		//printDevice(devs[i]);

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


		// test control request
		ret = controlOut(handle, Request::RED, 1, 0);
		ret = controlOut(handle, Request::GREEN, 0, 1);
		ret = controlOut(handle, Request::RED, 0, 0);
		ret = controlOut(handle, Request::GREEN, 0, 0);
		ret = controlOut(handle, Request::BLUE, 5, 5);
		ret = controlOut(handle, Request::BLUE, 0, 256);

		
		// loop
		uint8_t data[128] = {};
		int sendLength = 0;
		int transferred;
		bool allOk = true;
		while (true) {
			printf("length: %d\n", sendLength);

			// send to device
			for (int i = 0; i < sendLength; ++i) {
				data[i] = sendLength + i;
			}
			ret = libusb_bulk_transfer(handle, 1 | Usb::OUT, data, sendLength, &transferred, 10000);
			if (ret == LIBUSB_ERROR_TIMEOUT)
				printf("send timeout!\n");
			printStatus("sent", ret == 0 && transferred == sendLength);
			allOk &= ret == 0 && transferred == sendLength;
			
			// send zero length packet to indicate that transfer is complete if length is multiple of 64
			if (sendLength > 0 && (sendLength & 63) == 0)
				libusb_bulk_transfer(handle, 1 | Usb::OUT, data, 0, &transferred, 1000);

			// receive from device (we get back the same data that we sent)
			ret = libusb_bulk_transfer(handle, 1 | Usb::IN, data, 128, &transferred, 10000);
			if (ret == LIBUSB_ERROR_TIMEOUT)
				printf("receive timeout!\n");
			printStatus("received", ret == 0 && transferred == sendLength);
			allOk &= ret == 0 && transferred == sendLength;

			// check received data
			bool ok = true;
			for (int i = 0; i < transferred; ++i) {
				if (data[i] != transferred + i)
					ok = false;
			}
			printStatus("data", ok);
			allOk &= ok;

			printStatus("all", allOk);

			usleep(1000000);
			sendLength = (sendLength + 5) % 129;
		}
		break;
	}

	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return 0;
}

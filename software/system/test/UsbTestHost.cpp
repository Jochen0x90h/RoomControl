#include <usb.hpp>
#include <Terminal.hpp>
#include <posix/libusb.hpp>
#include "StringOperators.hpp"


// Host for UsbDeviceTest
// Flash UsbDeviceTest onto the device, then run this


// https://github.com/libusb/libusb/blob/master/examples/listdevs.c
static void printDevices(libusb_device **devices) {
	libusb_device *device;
	int i = 0, j = 0;
	uint8_t path[8]; 

	// iterate over devices
	while ((device = devices[i++]) != nullptr) {
		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(device, &desc);
		if (r < 0) {
			Terminal::err << "failed to get device descriptor\n";
			return;
		}

		Terminal::out << hex(desc.idVendor) << ':' << hex(desc.idProduct)
			<< " (bus " << dec(libusb_get_bus_number(device)) << ", device " << dec(libusb_get_device_address(device)) << ")";

		r = libusb_get_port_numbers(device, path, sizeof(path));
		if (r > 0) {
			Terminal::out << " path: " << dec(path[0]);
			for (j = 1; j < r; j++)
				Terminal::out << "." << dec(path[j]);
		}
		Terminal::out << '\n';
	}
}

// https://github.com/libusb/libusb/blob/master/examples/testlibusb.c
static void printDevice(libusb_device *dev, int idVendor = 0, int idProduct = 0) {
	libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	unsigned char string[256];
	int ret;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		Terminal::err << "failed to get device descriptor\n";
		return;
	}

	if ((idVendor | idProduct) != 0 && (desc.idVendor != idVendor || desc.idProduct != idProduct))
		return;

	Terminal::out << hex(desc.idVendor) << ':' << hex(desc.idProduct) << '\n';
	Terminal::out << "\tBus: " << dec(libusb_get_bus_number(dev)) << '\n';
	Terminal::out << "\tDevice: " << dec(libusb_get_device_address(dev)) << '\n';
	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		Terminal::out << "\tOpen\n";

		// manufacturer
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				Terminal::out << "\t\tManufacturer: " << str(string) << '\n';
		}

		// product
		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				Terminal::out << "\t\tProduct: " << str(string) << '\n';
		}
		
	
		libusb_close(handle);
	} else {
		Terminal::out << "\tOpen error: " << dec(ret) << '\n';
	}

	// configurations
	for (int i = 0; i < desc.bNumConfigurations; i++) {
		libusb_config_descriptor *config;
		ret = libusb_get_config_descriptor(dev, i, &config);
		if (LIBUSB_SUCCESS == ret) {
			Terminal::out << "\tConfiguration[" << dec(i) << "]\n";
			Terminal::out << "\t\tTotalLength:         " << dec(config->wTotalLength) << '\n';
			Terminal::out << "\t\tNumInterfaces:       " << dec(config->bNumInterfaces) << '\n';
			Terminal::out << "\t\tConfigurationValue:  " << dec(config->bConfigurationValue) << '\n';
			Terminal::out << "\t\tConfiguration:       " << dec(config->iConfiguration) << '\n';
			Terminal::out << "\t\tAttributes:          " << hex(config->bmAttributes) << '\n';
			Terminal::out << "\t\tMaxPower:            " << dec(config->MaxPower) << '\n';
		}
		
		// interfaces
		for (int j = 0; j < config->bNumInterfaces; j++) {
			libusb_interface const & interface = config->interface[j];
			
			// alternate settings
			for (int k = 0; k < interface.num_altsetting; k++) {
				libusb_interface_descriptor const & descriptor = interface.altsetting[k];

				Terminal::out << "\t\tInterface[" << dec(j) << "][" << dec(k) << "]\n";
				//printf("\t\t\tInterfaceNumber:   %d\n", descriptor.bInterfaceNumber);
				//printf("\t\t\tAlternateSetting:  %d\n", descriptor.bAlternateSetting);
				Terminal::out << "\t\t\tNumEndpoints:      " << dec(descriptor.bNumEndpoints) << '\n';
				Terminal::out << "\t\t\tInterfaceClass:    " << dec(descriptor.bInterfaceClass) << '\n';
				Terminal::out << "\t\t\tInterfaceSubClass: " << dec(descriptor.bInterfaceSubClass) << '\n';
				Terminal::out << "\t\t\tInterfaceProtocol: " << dec(descriptor.bInterfaceProtocol) << '\n';
				Terminal::out << "\t\t\tInterface:         " << dec(descriptor.iInterface) << '\n';

				// endpoints
				for (int l = 0; l < descriptor.bNumEndpoints; l++) {
					libusb_endpoint_descriptor const & endpoint = descriptor.endpoint[l];
					
					Terminal::out << "\t\t\tEndpoint[" << dec(l) << "]\n";
					Terminal::out << "\t\t\t\tEndpointAddress: " << hex(endpoint.bEndpointAddress) << '\n';
					Terminal::out << "\t\t\t\tAttributes:      " << hex(endpoint.bmAttributes) << '\n';
					Terminal::out << "\t\t\t\tMaxPacketSize:   " << dec(endpoint.wMaxPacketSize) << '\n';
					Terminal::out << "\t\t\t\tInterval:        " << dec(endpoint.bInterval) << '\n';
					Terminal::out << "\t\t\t\tRefresh:         " << dec(endpoint.bRefresh) << '\n';
					Terminal::out << "\t\t\t\tSynchAddress:    " << dec(endpoint.bSynchAddress) << '\n';
				}
			}
		
		}
		
		libusb_free_config_descriptor(config);
	}
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
		uint8_t(usb::Request::VENDOR_DEVICE_OUT),
		uint8_t(request),
		wValue,
		wIndex,
		nullptr,
		0,
		1000);
}

// print test status
void printStatus(String message, bool status) {
	Terminal::out << message << ": " << (status ? "ok" : "error") << '\n';
}

int main() {
	int r = libusb_init(nullptr);
	if (r < 0)
		return r;

	// get device list
	libusb_device **devices;
	ssize_t cnt = libusb_get_device_list(nullptr, &devices);
	if (cnt < 0) {
		libusb_exit(nullptr);
		return (int) cnt;
	}

	// print list of devices
	printDevices(devices);
	for (int i = 0; devices[i]; ++i) {
		printDevice(devices[i], 0x1915, 0x1337);
	}

	// iterate over devices
	for (int deviceIndex = 0; devices[deviceIndex]; ++deviceIndex) {
		libusb_device *dev = devices[deviceIndex];

		// get device descriptor
		libusb_device_descriptor desc;
		int ret = libusb_get_device_descriptor(dev, &desc);
		if (ret != LIBUSB_SUCCESS) {
			Terminal::err << "get device descriptor error: " << dec(ret) << '\n';
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
			Terminal::err << "open error: " << dec(ret) << '\n';
			continue;
		}

		if (libusb_kernel_driver_active(handle, 0) == 1) {
			Terminal::out << "detach active kernel driver\n";
			ret = libusb_detach_kernel_driver(handle, 0);
			if (ret != LIBUSB_SUCCESS) {
				Terminal::out << "detach kernel driver error: " << dec(ret) << '\n';
				continue;
			}
		}
			
		// set configuration (reset alt_setting, reset toggles)
		ret = libusb_set_configuration(handle, 1);
		if (ret != LIBUSB_SUCCESS) {
			Terminal::err << "set configuration error: " << dec(ret) << '\n';
			continue;
		}

		// claim interface with bInterfaceNumber = 0
		ret = libusb_claim_interface(handle, 0);
		if (ret != LIBUSB_SUCCESS) {
			Terminal::err << "claim interface error: " << dec(ret) << '\n';
			continue;
		}

		//ret = libusb_set_interface_alt_setting(handle, 0, 0);
		//Terminal::out << "set alternate setting " << dec(ret) << '\n';


		// test control request
		ret = controlOut(handle, Request::RED, 1, 0); // set on if wValue != 0
		ret = controlOut(handle, Request::RED, 0, 0);
		ret = controlOut(handle, Request::GREEN, 0, 1); // set on if wIndex != 0
		ret = controlOut(handle, Request::GREEN, 0, 0);
		ret = controlOut(handle, Request::BLUE, 5, 5); // set on if wValue == wIndex
		ret = controlOut(handle, Request::BLUE, 0, 256);

		
		uint8_t buffer[129] = {};
		int transferred;

		// flush out data from last run
		for (int i = 0; i < 4; ++i)
			ret = libusb_bulk_transfer(handle, 1 | usb::IN, buffer, 129, &transferred, 100);

		// echo loop: send data to device and check if we get back the same data
		int sendLength = 128;
		bool allOk = true;
		for (int iter = 0; iter < 10000000; ++iter) {
			Terminal::out << "length: " << dec(sendLength) << '\n';

			// send to device
			for (int i = 0; i < sendLength; ++i) {
				buffer[i] = sendLength + i;
			}
			ret = libusb_bulk_transfer(handle, 1 | usb::OUT, buffer, sendLength, &transferred, 10000);
			if (ret == LIBUSB_ERROR_TIMEOUT)
				Terminal::out << "send timeout!\n";
			printStatus("sent", ret == 0 && transferred == sendLength);
			allOk &= ret == 0 && transferred == sendLength;
			
			// send zero length packet to indicate that transfer is complete if length is multiple of 64
			if (sendLength > 0 && (sendLength & 63) == 0)
				libusb_bulk_transfer(handle, 1 | usb::OUT, buffer, 0, &transferred, 1000);

			// debug: check if one packet of maximum 64 bytes can be written while the device is not waiting in UsbDevice::receive()
			for (int i = 0; i < 45; ++i) buffer[i] = 45 + i;
			ret = libusb_bulk_transfer(handle, 1 | usb::OUT, buffer, 45, &transferred, 10000);
			if (ret == LIBUSB_ERROR_TIMEOUT)
				Terminal::out << "debug send timeout!\n";

			// receive from device (we get back the same data that we sent)
			// note: libusb does not wait for the zero length packet if device sends 128 bytes, therefore use 129 instead of 128
			ret = libusb_bulk_transfer(handle, 1 | usb::IN, buffer, 129, &transferred, 10000);
			if (ret == LIBUSB_ERROR_TIMEOUT)
				Terminal::out << "receive timeout!\n";
			//printStatus("received", ret == 0 && transferred == sendLength);
			Terminal::out << "received: ";
			if (ret != 0) {
				Terminal::out << "error " << dec(ret) << '\n';
			} else if (transferred != sendLength) {
				Terminal::out << dec(transferred) << " != " << dec(sendLength) << '\n';
			} else {
				Terminal::out << "ok\n";
			}
			allOk &= ret == 0 && transferred == sendLength;


			// check received data
			bool ok = true;
			for (int i = 0; i < transferred; ++i) {
				if (buffer[i] != transferred + i)
					ok = false;
			}
			printStatus("data", ok);
			allOk &= ok;

			printStatus("all", allOk);

			// debug: remove additional packet
			ret = libusb_bulk_transfer(handle, 1 | usb::IN, buffer, 45, &transferred, 10000);
			for (int i = 0; i < 45; ++i) allOk &= buffer[i] == 45 + i;

			// wait
			//usleep(1000000);

			// modify the send length
			sendLength = (sendLength + 5) % 129;
		}
		libusb_close(handle);
		break;
	}

	libusb_free_device_list(devices, 1);
	libusb_exit(nullptr);
	return 0;
}

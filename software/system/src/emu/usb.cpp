#include "../usb.hpp"
#include <stdio.h>


namespace usb {

bool text;

void init(std::function<Array<uint8_t> (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration)
{
	// get device descriptor
	auto deviceDescriptor = reinterpret_cast<DeviceDescriptor const *>(getDescriptor(DESCRIPTOR_DEVICE).data);
	usb::text = deviceDescriptor->bDeviceProtocol == 1;
	
	// set configuration
	onSetConfiguration(1);
}

void enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
}

bool send(int endpoint, void const *data, int length, std::function<void ()> const &onSent) {
	auto d = reinterpret_cast<uint8_t const *>(data);
	if (usb::text) {
		printf("%.*s", length, d);
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
			printf("0x%02x", d[i]);
		}
		printf("\n");
	}
}

bool receive(int endpoint, void *data, int maxLength, std::function<void (int)> const &onReceived) {
}

} // namespace usb

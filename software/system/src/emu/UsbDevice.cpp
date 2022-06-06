#include "../UsbDevice.hpp"
#include "Loop.hpp"
#include <stdio.h>


namespace UsbDevice {

bool text;

// endpoints 1 - 7
struct Endpoint {
	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};

Endpoint endpoints[1];

std::function<void (uint8_t)> onSetConfiguration;

// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// call onSetConfiguration once
	if (UsbDevice::onSetConfiguration != nullptr) {
		UsbDevice::onSetConfiguration(1);
		UsbDevice::onSetConfiguration = nullptr;
	}

	for (auto &endpoint : UsbDevice::endpoints) {

		endpoint.sendWaitlist.resumeFirst([](SendParameters p) {
			if (UsbDevice::text) {
				printf("%.*s", p.length, reinterpret_cast<char const *>(p.data));
			} else {
				// binary
				printf("%d: ", p.length);
				for (int i = 0; i < p.length; ++i) {
					if ((i & 15) == 0) {
						if (i != 0)
							printf(",\n");
					} else {
						printf(", ");
					}
					printf("0x%02x", reinterpret_cast<uint8_t const *>(p.data)[i]);
				}
				printf("\n");
			}
			return true;
		});
	}
	
	// call next handler in chain
	UsbDevice::nextHandler(gui);
}

void init(
	std::function<Data (usb::DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration,
	std::function<bool (uint8_t, uint16_t, uint16_t)> const &onRequest)
{
	// get device descriptor
	auto &deviceDescriptor = getDescriptor(usb::DescriptorType::DEVICE).cast<usb::DeviceDescriptor>();
	UsbDevice::text = deviceDescriptor.bDeviceProtocol == 1;
	
	// set configuration
	UsbDevice::onSetConfiguration = onSetConfiguration;
	//Loop::context.post([onSetConfiguration]() {onSetConfiguration(1);});

	// add to event loop handler chain
	UsbDevice::nextHandler = Loop::addHandler(handle);
}

void enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
}

Awaitable<ReceiveParameters> receive(int index, int &length, void *data) {
	assert(index == 1);
	auto &endpoint = UsbDevice::endpoints[index - 1];

	return {endpoint.receiveWaitlist, length, data};
}

Awaitable<SendParameters> send(int index, int length, void const *data) {
	assert(index == 1);
	auto &endpoint = UsbDevice::endpoints[index - 1];

	return {endpoint.sendWaitlist, length, data};
}

} // namespace Usb

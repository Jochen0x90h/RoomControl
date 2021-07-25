#include "../usb.hpp"
#include "loop.hpp"
#include <stdio.h>


namespace usb {

bool text;

// endpoints 1 - 7
struct Endpoint {
	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};

Endpoint endpoints[1];

// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	for (auto &endpoint : usb::endpoints) {

		endpoint.sendWaitlist.resumeFirst([](SendParameters p) {
			if (usb::text) {
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
	usb::nextHandler(gui);
}

void init(
	std::function<Data (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration,
	std::function<bool (uint8_t, uint16_t, uint16_t)> const &onRequest)
{
	// get device descriptor
	auto &deviceDescriptor = getDescriptor(DescriptorType::DEVICE).cast<DeviceDescriptor>();
	usb::text = deviceDescriptor.bDeviceProtocol == 1;
	
	// set configuration
	loop::context.post([onSetConfiguration]() {onSetConfiguration(1);});

	// add to event loop handler chain
	usb::nextHandler = loop::addHandler(handle);
}

void enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
}

Awaitable<ReceiveParameters> receive(int index, int length, int& receivedLength, void *data) {
	assert(index == 1);
	auto &endpoint = usb::endpoints[index - 1];

	return {endpoint.receiveWaitlist, length, receivedLength, data};
}

Awaitable<SendParameters> send(int index, int length, void const *data) {
	assert(index == 1);
	auto &endpoint = usb::endpoints[index - 1];

	return {endpoint.sendWaitlist, length, data};
}

} // namespace usb

#include "UsbDeviceImpl.hpp"
#include "../Terminal.hpp"
#include <StringOperators.hpp>


UsbDeviceImpl::UsbDeviceImpl(
	std::function<ConstData (usb::DescriptorType)> const &getDescriptor,
	std::function<void (UsbDevice &usb, uint8_t bConfigurationValue)> const &onSetConfiguration,
	std::function<bool (uint8_t bRequest, uint16_t wValue, uint16_t wIndex)> const &onRequest)
{
	// get device descriptor
	auto *deviceDescriptor = getDescriptor(usb::DescriptorType::DEVICE).cast<usb::DeviceDescriptor>();
	this->text = deviceDescriptor->bDeviceProtocol == 1;
	
	// set configuration
	this->onSetConfiguration = onSetConfiguration;
	//Loop::context.post([onSetConfiguration]() {onSetConfiguration(1);});

	// call onSetConfiguration from event loop
	this->time = Loop::now();
	Loop::timeHandlers.add(*this);
}

void UsbDeviceImpl::enableEndpoints(uint8_t inFlags, uint8_t outFlags) {	
	Terminal::out << "enable in";
	for (int i = 0; i < 8; ++i) {
		if (inFlags & (1 << i))
			Terminal::out << ' ' << dec(i);
	}
	Terminal::out << " out";
	for (int i = 0; i < 8; ++i) {
		if (outFlags & (1 << i))
			Terminal::out << ' ' << dec(i);
	}
	Terminal::out << '\n';
}

Awaitable<UsbDevice::ReceiveParameters> UsbDeviceImpl::receive(int index, int &length, void *data) {
	assert(index == 1);
	auto &endpoint = this->endpoints[index - 1];
	if (!isInList()) {
		this->time = Loop::now() + 10ms; // emulate 10ms transfer time
		Loop::timeHandlers.add(*this);
	}
	return {endpoint.receiveWaitlist, length, data};
}

Awaitable<UsbDevice::SendParameters> UsbDeviceImpl::send(int index, int length, void const *data) {
	assert(index == 1);
	auto &endpoint = this->endpoints[index - 1];
	if (!isInList()) {
		this->time = Loop::now() + 10ms; // emulate 10ms transfer time
		Loop::timeHandlers.add(*this);
	}
	return {endpoint.sendWaitlist, length, data};
}

void UsbDeviceImpl::activate() {
	this->remove();

	// call onSetConfiguration once
	if (this->onSetConfiguration != nullptr) {
		this->onSetConfiguration(*this, 1);
		this->onSetConfiguration = nullptr;
	}

	for (auto &endpoint : this->endpoints) {

		endpoint.sendWaitlist.resumeFirst([this](SendParameters p) {
			if (this->text) {
				Terminal::out << String(p.length, reinterpret_cast<char const *>(p.data));
			} else {
				// binary
				Terminal::out << dec(p.length) << ": ";
				for (int i = 0; i < p.length; ++i) {
					if ((i & 15) == 0) {
						if (i != 0)
							Terminal::out << ",\n";
					} else {
						Terminal::out << ", ";
					}
					Terminal::out << "0x" << hex(reinterpret_cast<uint8_t const *>(p.data)[i]);
				}
				Terminal::out << '\n';
			}
			return true;
		});
	}
}

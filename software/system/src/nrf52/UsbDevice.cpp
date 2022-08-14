#include "../UsbDevice.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml

	Dependencies:

	Config:

	Resources:
		NRF_USBD
*/
namespace UsbDevice {

std::function<ConstData (usb::DescriptorType)> getDescriptor;
std::function<void (uint8_t)> onSetConfiguration;
std::function<bool (uint8_t, uint16_t, uint16_t)> onRequest;

// endpoint 0
uint8_t ep0Buffer[64] __attribute__((aligned(4)));
uint8_t const *ep0Data;
int ep0SendLength = 0;

void ep0Send(void const *data, int length) {
	auto d = reinterpret_cast<uint8_t const *>(data);
	int l = min(length, 64);
	array::copy(ep0Buffer, ep0Buffer + l, d);
	NRF_USBD->EPIN[0].PTR = intptr_t(ep0Buffer);
	NRF_USBD->EPIN[0].MAXCNT = l;
	ep0Data = d;
	ep0SendLength = length;

	NRF_USBD->TASKS_STARTEPIN[0] = TRIGGER;
}

// endpoints 1 - 7
struct Endpoint {
	// receive (OUT)
	enum State {
		IDLE,
		BUFFER_READY,
		BUSY,
	};
	State receiveState = IDLE;
	int maxReceiveLength;
	int receiveLength = 0;
	Waitlist<ReceiveParameters> receiveWaitlist;

	// send (IN)
	bool sendBusy = false;
	int sendLength = 0;
	Waitlist<SendParameters> sendWaitlist;
};

Endpoint endpoints[USB_ENDPOINT_COUNT - 1];


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (isInterruptPending(USBD_IRQn)) {
		if (NRF_USBD->EVENTS_USBEVENT) {
			// clear pending interrupt flag at peripheral
			NRF_USBD->EVENTS_USBEVENT = 0;

			// check cause
			if (NRF_USBD->EVENTCAUSE & N(USBD_EVENTCAUSE_READY, Ready)) {
				// usb is ready
				NRF_USBD->EVENTCAUSE = N(USBD_EVENTCAUSE_READY, Ready);

				// enable pullup
				NRF_USBD->USBPULLUP = N(USBD_USBPULLUP_CONNECT, Enabled);
			}
		}
		if (NRF_USBD->EVENTS_EP0SETUP) {
			// clear pending interrupt flag at peripheral
			NRF_USBD->EVENTS_EP0SETUP = 0;

			// setup request
			auto bmRequestType = usb::Request(NRF_USBD->BMREQUESTTYPE);
			uint8_t bRequest = NRF_USBD->BREQUEST;

			switch (bmRequestType) {
			case usb::Request::STANDARD_DEVICE_OUT:
				// write request to standard device
				if (bRequest == 0x05) {
					// set address, handled by hardware
				} else if (bRequest == 0x09) {
					// set configuration
					uint8_t bConfigurationValue = NRF_USBD->WVALUEL;
					UsbDevice::onSetConfiguration(bConfigurationValue);

					// enter status stage
					NRF_USBD->TASKS_EP0STATUS = TRIGGER;
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = TRIGGER;
				}
				break;
			case usb::Request::STANDARD_DEVICE_IN:
				// read request to standard device
				if (bRequest == 0x06) {
					// get descriptor from user code by using the callback
					auto descriptorType = usb::DescriptorType(NRF_USBD->WVALUEH);
					ConstData descriptor = UsbDevice::getDescriptor(descriptorType);
					if (descriptor.size() > 0) {
						// send descriptor
						int wLength = (NRF_USBD->WLENGTHH << 8) | NRF_USBD->WLENGTHL;
						int size = min(descriptor.size(), wLength);
						ep0Send(descriptor.data(), size);
					} else {
						// unsupported descriptor type: stall
						NRF_USBD->TASKS_EP0STALL = TRIGGER;
					}
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = TRIGGER;
				}
				break;
			case usb::Request::STANDARD_INTERFACE_OUT:
				// write request to standard interface
				if (bRequest == 0x11) {
					// set interface
					//uint8_t interfaceIndex = NRF_USBD->WINDEXL;
					//uint8_t alternateSettingIndex = NRF_USBD->WVALUEL;

					// enter status stage
					NRF_USBD->TASKS_EP0STATUS = TRIGGER;
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = TRIGGER;
				}
				break;
			case usb::Request::VENDOR_INTERFACE_OUT:
				{
					int wValue = (NRF_USBD->WVALUEH << 8) | NRF_USBD->WVALUEL;
					int wIndex = (NRF_USBD->WINDEXH << 8) | NRF_USBD->WINDEXL;

					// let user code handle the request
					bool result = UsbDevice::onRequest(bRequest, wValue, wIndex);
					if (result) {
						// enter status stage
						NRF_USBD->TASKS_EP0STATUS = TRIGGER;
					} else {
						// unsupported request: stall
						NRF_USBD->TASKS_EP0STALL = TRIGGER;
					}
				}
				break;
			default:
				// unsupported request type: stall
				NRF_USBD->TASKS_EP0STALL = TRIGGER;
			}
		}
		if (NRF_USBD->EVENTS_EP0DATADONE) {
			// clear pending interrupt flag at peripheral
			NRF_USBD->EVENTS_EP0DATADONE = 0;

			// check if more to send
			int sentCount = NRF_USBD->EPIN[0].AMOUNT;
			int length = ep0SendLength - sentCount;
			ep0SendLength = length;
			if (sentCount == 64) {
				// more to send
				ep0Data += sentCount;

				int l = min(length, 64);
				array::copy(ep0Buffer, ep0Buffer + l, ep0Data);
				NRF_USBD->EPIN[0].MAXCNT = l;
				NRF_USBD->TASKS_STARTEPIN[0] = TRIGGER;
			} else {
				// finished: enter status stage
				NRF_USBD->TASKS_EP0STATUS = TRIGGER;
			}
		}

		// capture EPDATASTATUS here to be sure that we see the previous ENDEPOUT event
		uint32_t EPDATASTATUS = NRF_USBD->EPDATASTATUS;

		// handle end of receive (OUT) DMA transfer from internal buffer to memory
		for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
			if (NRF_USBD->EVENTS_ENDEPOUT[index]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPOUT[index] = 0;

				auto& ep = UsbDevice::endpoints[index - 1];

				int receivedCount = NRF_USBD->EPOUT[index].AMOUNT;
				ep.receiveLength -= receivedCount;
				if (receivedCount == 64) {
					// more to receive
					NRF_USBD->EPOUT[index].PTR = NRF_USBD->EPOUT[index].PTR + 64;
				} else {
					// finished: calculate length of received data
					int length = ep.maxReceiveLength - ep.receiveLength;

					// resume first waiting coroutine
					ep.receiveWaitlist.resumeFirst([length](ReceiveParameters p){
						p.length = length;
						return true;
					});

					// check if there are more queued coroutines waiting for receive
					bool busy = ep.receiveWaitlist.resumeFirst([&ep, index](ReceiveParameters p) {
						// set receive data
						ep.maxReceiveLength = p.length;
						ep.receiveLength = p.length;
						NRF_USBD->EPOUT[index].PTR = intptr_t(p.data);

						// don't resume yet
						return false;
					});
					ep.receiveState = busy ? Endpoint::BUSY : Endpoint::IDLE;
				}
			}
		}

		// handle end of transfer between host and internal buffer (both IN and OUT)
		if (EPDATASTATUS) {
			// clear pending interrupt flags at peripheral
			NRF_USBD->EVENTS_EPDATA = 0;

			// clear flags of acknowledged data transfers (IN and OUT)
			NRF_USBD->EPDATASTATUS = EPDATASTATUS;

			for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
				int inFlag = 1 << index;
				int outFlag = inFlag << 16;

				if (EPDATASTATUS & inFlag) {
					// finished send to host (IN)
					auto &ep = UsbDevice::endpoints[index - 1];

					// check if more to send
					int sentCount = NRF_USBD->EPIN[index].AMOUNT;
					int length = ep.sendLength - sentCount;
					ep.sendLength = length;
					if (sentCount == 64) {
						// more to send: Start DMA transfer from memory to internal buffer
						NRF_USBD->EPIN[index].PTR = NRF_USBD->EPIN[index].PTR + 64;
						NRF_USBD->EPIN[index].MAXCNT = min(length, 64);
						NRF_USBD->TASKS_STARTEPIN[index] = TRIGGER;
					} else {
						// finished: resume waiting coroutine
						ep.sendWaitlist.resumeFirst([](SendParameters p) {
							return true;
						});

						// check if there are more queued coroutines waiting for send
						ep.sendBusy = ep.sendWaitlist.resumeFirst([index, &ep](SendParameters p) {
							// set send data
							ep.sendLength = p.length;
							NRF_USBD->EPIN[index].PTR = intptr_t(p.data);
							NRF_USBD->EPIN[index].MAXCNT = min(p.length, 64);

							// start send
							NRF_USBD->TASKS_STARTEPIN[index] = TRIGGER;

							// don't resume yet
							return false;
						});
					}
				}

				if (EPDATASTATUS & outFlag) {
					// finished receive from host (OUT)
					auto& ep = UsbDevice::endpoints[index - 1];

					if (ep.receiveState == Endpoint::BUSY) {
						// get length of data in internal buffer
						int bufferLength = NRF_USBD->SIZE.EPOUT[index];

						// start DMA transfer of received data from internal buffer to memory
						NRF_USBD->EPOUT[index].MAXCNT = min(ep.receiveLength, bufferLength);
						NRF_USBD->TASKS_STARTEPOUT[index] = TRIGGER;
					} else {
						// no receive is in progress, therefore memoize that data for next receive is ready
						ep.receiveState = Endpoint::BUFFER_READY;
					}
				}
			}
		}
		if (NRF_USBD->EVENTS_USBRESET) {
			// clear pending interrupt flags at peripheral
			NRF_USBD->EVENTS_USBRESET = 0;

		}

		// clear pending interrupt flag at NVIC
		clearInterrupt(USBD_IRQn);
	}

	// call next handler in chain
	UsbDevice::nextHandler();
}

void init(
	std::function<ConstData (usb::DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration,
	std::function<bool (uint8_t, uint16_t, uint16_t)> const &onRequest)
{
	// check if already initialized
	if (UsbDevice::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	UsbDevice::nextHandler = Loop::addHandler(handle);

	UsbDevice::getDescriptor = getDescriptor;
	UsbDevice::onSetConfiguration = onSetConfiguration;
	UsbDevice::onRequest = onRequest;

	NRF_USBD->INTENSET = N(USBD_INTENSET_USBEVENT, Set)
		| N(USBD_INTENSET_EP0SETUP, Set)
		| N(USBD_INTENSET_EP0DATADONE, Set)
		| N(USBD_INTENSET_EPDATA, Set)
		| (1 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT1, Set) : 0)
		| (2 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT2, Set) : 0)
		| (3 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT3, Set) : 0)
		| (4 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT4, Set) : 0)
		| (5 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT5, Set) : 0)
		| (6 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT6, Set) : 0)
		| (7 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPOUT7, Set) : 0)
		| N(USBD_INTENSET_USBRESET, Set);

	NRF_USBD->ENABLE = N(USBD_ENABLE_ENABLE, Enabled);
}

void enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
	NRF_USBD->EPINEN = inFlags;
	NRF_USBD->EPOUTEN = outFlags;

	for (int index = 1; index < 8; ++index) {
		if (inFlags & (1 << index)) {
			NRF_USBD->DTOGGLE = index | N(USBD_DTOGGLE_IO, In) | N(USBD_DTOGGLE_VALUE, Data0);
		}
		if (outFlags & (1 << index)) {
			NRF_USBD->DTOGGLE = index | N(USBD_DTOGGLE_IO, Out) | N(USBD_DTOGGLE_VALUE, Data0);

			// write any value to start receiving OUT transfers into intermediate buffer
			NRF_USBD->SIZE.EPOUT[index] = 0;
		}
	}
}

Awaitable<ReceiveParameters> receive(int index, int &length, void *data) {
	assert(index >= 1 && endpoint < USB_ENDPOINT_COUNT);
	auto& ep = UsbDevice::endpoints[index - 1];

	// check if usb receiver is idle
	if (ep.receiveState != Endpoint::BUSY) {
		// set receive data
		ep.maxReceiveLength = length;
		ep.receiveLength = length;
		NRF_USBD->EPOUT[index].PTR = intptr_t(data);

		// check if data is already in the internal buffer
		if (ep.receiveState == Endpoint::BUFFER_READY) {
			// get length of data in internal buffer
			int bufferLength = NRF_USBD->SIZE.EPOUT[index];

			// start DMA transfer of received data from internal buffer to memory
			NRF_USBD->EPOUT[index].MAXCNT = min(length, bufferLength);
			NRF_USBD->TASKS_STARTEPOUT[index] = TRIGGER;
		}
		ep.receiveState = Endpoint::BUSY;
	}

	return {ep.receiveWaitlist, length, data};
}

Awaitable<SendParameters> send(int index, int length, void const *data) {
	assert(index >= 1 && index < USB_ENDPOINT_COUNT);
	auto& ep = UsbDevice::endpoints[index - 1];

	// check if usb sender is idle
	if (!ep.sendBusy) {
		// set send data
		ep.sendBusy = true;
		ep.sendLength = length;
		NRF_USBD->EPIN[index].PTR = intptr_t(data);
		NRF_USBD->EPIN[index].MAXCNT = min(length, 64);

		// start send
		NRF_USBD->TASKS_STARTEPIN[index] = TRIGGER;
	}

	return {ep.sendWaitlist, length, data};
}

} // namespace UsbDevice

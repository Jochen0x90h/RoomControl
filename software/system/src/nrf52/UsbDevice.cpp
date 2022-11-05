#include "../UsbDevice.hpp"
#include "../Debug.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml

	Dependencies:

	Config:

	Resources:
		NRF_USBD

	Bugs:
 		Workaround for Errata 199 needs to be added when using more than one endpoint: https://infocenter.nordicsemi.com/topic/errata_nRF52840_Rev2/ERR/nRF52840/Rev2/latest/anomaly_840_199.html
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
	enum State : uint8_t {
		IDLE,

		BUFFER,

		// data is transferred via usb from host to internal buffer
		USB,

		// data is transferred via dma from internal buffer to memory
		DMA,

		// wait for dma channel to be available (there is only one per endpoint)
		WAIT
	};

	// receive (OUT)
	State receiveState = IDLE;
	int maxReceiveLength;
	int receiveLength = 0;
	Waitlist<ReceiveParameters> receiveWaitlist;
	void prepareReceive(int index, intptr_t data) {
		// set pointer and wait for USB transfer into internal buffer
		NRF_USBD->EPOUT[index].PTR = data;
		this->receiveState = USB;
	}
	void startReceive(int index) {
		// length of data in internal buffer
		int bufferLength = NRF_USBD->SIZE.EPOUT[index];
		NRF_USBD->EPOUT[index].MAXCNT = min(this->receiveLength, bufferLength);
		if (this->sendState == DMA) {
			// DMA is currently in use: wait until DMA is free
			this->receiveState = WAIT;
		} else {
			triggerReceive(index);
		}
	}
	void triggerReceive(int index) {
		// start DMA
		NRF_USBD->TASKS_STARTEPOUT[index] = TRIGGER;
		this->receiveState = DMA;
	}

	// send (IN)
	State sendState = IDLE;
	int sendLength = 0;
	Waitlist<SendParameters> sendWaitlist;
	void startSend(int index, intptr_t data) {
		NRF_USBD->EPIN[index].PTR = data;
		NRF_USBD->EPIN[index].MAXCNT = min(this->sendLength, 64);
		if (this->receiveState == Endpoint::DMA) {
			// DMA is currently in use: wait until DMA is free
			this->sendState = Endpoint::WAIT;
		} else {
			triggerSend(index);
		}
	}
	void triggerSend(int index) {
		// start DMA
		NRF_USBD->TASKS_STARTEPIN[index] = TRIGGER;
		this->sendState = Endpoint::DMA;
	}
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

		// get flags of acknowledged USB transfers here so that they don't "overtake" DMA events
		NRF_USBD->EVENTS_EPDATA = 0;
		uint32_t EPDATASTATUS = NRF_USBD->EPDATASTATUS;

		// handle end of DMA transfers
		for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
			// check if we sent via DMA to IN endpoint buffer
			if (NRF_USBD->EVENTS_ENDEPIN[index]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPIN[index] = 0;

				auto& ep = UsbDevice::endpoints[index - 1];

				// check if receiver waits for DMA
				if (ep.receiveState == Endpoint::WAIT)
					ep.triggerReceive(index); // -> ENDEPOUT[index]

				// now wait until sending via USB to the host has finished
				ep.sendState = Endpoint::USB;
				// -> EPDATA IN
			}

			// check if we received via DMA from OUT endpoint buffer
			if (NRF_USBD->EVENTS_ENDEPOUT[index]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPOUT[index] = 0;

				auto& ep = UsbDevice::endpoints[index - 1];

				// check if sender waits for DMA
				if (ep.sendState == Endpoint::WAIT)
					ep.triggerSend(index); // -> ENDEPIN[index]

				int receivedCount = NRF_USBD->EPOUT[index].AMOUNT;
				ep.receiveLength -= receivedCount;
				if (receivedCount == 64) {
					// more to receive
					ep.prepareReceive(index, NRF_USBD->EPOUT[index].PTR + 64);
				} else {
					// finished: calculate length of received data
					int length = ep.maxReceiveLength - ep.receiveLength;

					// resume first waiting coroutine
					ep.receiveState = Endpoint::USB; // receiveState must not be DMA any more
					ep.receiveWaitlist.resumeFirst([length](ReceiveParameters &p){
						p.length = length;
						return true;
					});

					ep.receiveState = Endpoint::IDLE;

					// check if there are more queued coroutines waiting for receive
					ep.receiveWaitlist.visitFirst([&ep, index](ReceiveParameters &p) {
						// set receive data
						ep.maxReceiveLength = p.length;
						ep.receiveLength = p.length;
						ep.prepareReceive(index, intptr_t(p.data));
					});
					// -> EPDATA OUT
				}
			}
		}

		// handle end of transfer between host and internal buffer (both IN and OUT)
		if (EPDATASTATUS != 0) {
			// clear flags
			NRF_USBD->EPDATASTATUS = EPDATASTATUS;
			for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
				int inFlag = 1 << index;
				int outFlag = inFlag << 16;

				// EPDATA IN
				if (EPDATASTATUS & inFlag) {
					// finished send to host (IN)
					auto &ep = UsbDevice::endpoints[index - 1];

					// check if more to send
					int sentCount = NRF_USBD->EPIN[index].AMOUNT;
					int length = ep.sendLength - sentCount;
					ep.sendLength = length;
					if (sentCount == 64) {
						// more to send: Start next DMA transfer from memory to internal buffer (may be zero length)
						ep.startSend(index, NRF_USBD->EPIN[index].PTR + 64); // -> ENDEPIN[index]
					} else {
						// finished: resume waiting coroutine
						ep.sendWaitlist.resumeFirst([](SendParameters &p) {
							return true;
						});

						ep.sendState = Endpoint::IDLE;

						// check if there are more queued coroutines waiting for send
						ep.sendWaitlist.visitFirst([index, &ep](SendParameters &p) {
							// set send data
							ep.sendLength = p.length;

							// start first DMA transfer from memory to internal buffer
							ep.startSend(index, intptr_t(p.data)); // -> ENDEPIN[index]
						});
					}
				}

				// EPDATA OUT
				if (EPDATASTATUS & outFlag) {
					// finished receive from host (OUT)
					auto& ep = UsbDevice::endpoints[index - 1];

					if (ep.receiveState <= Endpoint::BUFFER) {
						// idle: mark that data is already waiting in the buffer
						ep.receiveState = Endpoint::BUFFER;
					} else {
						// ep.receiveState must be Endpoint::USB

						// start receiving via DMA
						ep.startReceive(index); // -> ENDEPOUT[index]
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
		| (1 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN1, Set) : 0)
		| (2 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN2, Set) : 0)
		| (3 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN3, Set) : 0)
		| (4 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN4, Set) : 0)
		| (5 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN5, Set) : 0)
		| (6 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN6, Set) : 0)
		| (7 < USB_ENDPOINT_COUNT ? N(USBD_INTENSET_ENDEPIN7, Set) : 0)
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
	assert(index >= 1 && index < USB_ENDPOINT_COUNT);
	auto& ep = UsbDevice::endpoints[index - 1];

	// check if usb receiver is idle or a buffer is ready
	if (ep.receiveState <= Endpoint::BUFFER) {
		// set receive data
		ep.maxReceiveLength = length;
		ep.receiveLength = length;
		bool buffer = ep.receiveState == Endpoint::BUFFER;
		ep.prepareReceive(index, intptr_t(data));

		// we can immediately start receiving via DMA if data is already in the internal buffer
		if (buffer)
			ep.startReceive(index); // -> ENDEPOUT[index]
	}

	return {ep.receiveWaitlist, length, data};
}

Awaitable<SendParameters> send(int index, int length, void const *data) {
	assert(index >= 1 && index < USB_ENDPOINT_COUNT);
	auto& ep = UsbDevice::endpoints[index - 1];

	// check if usb sender is idle
	if (ep.sendState == Endpoint::IDLE) {
		// set send data
		ep.sendLength = length;

		// start first DMA transfer from memory to internal buffer
		ep.startSend(index, intptr_t(data)); // -> ENDEPIN[index]
	}

	return {ep.sendWaitlist, length, data};
}

} // namespace UsbDevice

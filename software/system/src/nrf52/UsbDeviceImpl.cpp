#include "UsbDeviceImpl.hpp"
#include "../Debug.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"


/*
	Dependencies:

	Config:

	Resources:
		NRF_USBD

	Bugs:
 		Workaround for Errata 199 needs to be added when using more than one endpoint: https://infocenter.nordicsemi.com/topic/errata_nRF52840_Rev2/ERR/nRF52840/Rev2/latest/anomaly_840_199.html
*/

UsbDeviceImpl::UsbDeviceImpl(
	std::function<ConstData (usb::DescriptorType)> const &getDescriptor,
	std::function<void (UsbDevice &usb, uint8_t bConfigurationValue)> const &onSetConfiguration,
	std::function<bool (uint8_t bRequest, uint16_t wValue, uint16_t wIndex)> const &onRequest)
	: getDescriptor(getDescriptor), onSetConfiguration(onSetConfiguration), onRequest(onRequest)
{
	NRF_USBD->INTENSET = N(USBD_INTENSET_USBEVENT, Set)
		| N(USBD_INTENSET_EP0SETUP, Set)
		| N(USBD_INTENSET_EP0DATADONE, Set)
		| N(USBD_INTENSET_EPDATA, Set)
		| 0xef << USBD_INTENSET_ENDEPOUT1_Pos // OUT endpoint 1-7
		| 0xef << USBD_INTENSET_ENDEPIN1_Pos // IN endpoint 1-7
		| N(USBD_INTENSET_USBRESET, Set);

	NRF_USBD->ENABLE = N(USBD_ENABLE_ENABLE, Enabled);

	// add to list of handlers
	loop::handlers.add(*this);
}

void UsbDeviceImpl::enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
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

Awaitable<UsbDevice::ReceiveParameters> UsbDeviceImpl::receive(int index, int &length, void *data) {
	assert(index >= 1 && index <= ENDPOINT_COUNT);
	auto& ep = this->endpoints[index - 1];

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

Awaitable<UsbDevice::SendParameters> UsbDeviceImpl::send(int index, int length, void const *data) {
	assert(index >= 1 && index <= ENDPOINT_COUNT);
	auto& ep = this->endpoints[index - 1];

	// check if usb sender is idle
	if (ep.sendState == Endpoint::IDLE) {
		// set send data
		ep.sendLength = length;

		// start first DMA transfer from memory to internal buffer
		ep.startSend(index, intptr_t(data)); // -> ENDEPIN[index]
	}

	return {ep.sendWaitlist, length, data};
}

void UsbDeviceImpl::handle() {
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
					this->onSetConfiguration(*this, bConfigurationValue);

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
					ConstData descriptor = this->getDescriptor(descriptorType);
					if (descriptor.size() > 0) {
						// send descriptor
						int wLength = (NRF_USBD->WLENGTHH << 8) | NRF_USBD->WLENGTHL;
						int size = std::min(descriptor.size(), wLength);
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
			case usb::Request::VENDOR_DEVICE_OUT:
				{
					int wValue = (NRF_USBD->WVALUEH << 8) | NRF_USBD->WVALUEL;
					int wIndex = (NRF_USBD->WINDEXH << 8) | NRF_USBD->WINDEXL;

					// let user code handle the request
					bool result = this->onRequest(bRequest, wValue, wIndex);
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

				int l = std::min(length, 64);
				//array::copy(ep0Buffer, ep0Buffer + l, ep0Data);
				std::copy(ep0Data, ep0Data + l, ep0Buffer);
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
		for (int index = 1; index <= ENDPOINT_COUNT; ++index) {
			// check if we sent via DMA to IN endpoint buffer
			if (NRF_USBD->EVENTS_ENDEPIN[index]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPIN[index] = 0;

				auto& ep = this->endpoints[index - 1];

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

				auto& ep = this->endpoints[index - 1];

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
			for (int index = 1; index <= ENDPOINT_COUNT; ++index) {
				int inFlag = 1 << index;
				int outFlag = inFlag << 16;

				// EPDATA IN
				if (EPDATASTATUS & inFlag) {
					// finished send to host (IN)
					auto &ep = this->endpoints[index - 1];

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
					auto& ep = this->endpoints[index - 1];

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
}

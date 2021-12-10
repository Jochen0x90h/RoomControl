#include "../usb.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <util.hpp>


// usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml
namespace usb {

std::function<Data (DescriptorType)> getDescriptor;
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

	NRF_USBD->TASKS_STARTEPIN[0] = Trigger;
}

// endpoints 1 - 7
struct Endpoint {
	// receive
	bool receiveBusy;
	int maxReceiveLength;
	int receiveLength = 0;
	Waitlist<ReceiveParameters> receiveWaitlist;

	// send
	bool sendBusy;
	int sendLength = 0;
	Waitlist<SendParameters> sendWaitlist;
};

Endpoint endpoints[USB_ENDPOINT_COUNT - 1];


// event loop handler chain
loop::Handler nextHandler = nullptr;
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
			uint8_t bmRequestType = NRF_USBD->BMREQUESTTYPE;
			uint8_t bRequest = NRF_USBD->BREQUEST;
	
			switch (bmRequestType) {
			case OUT | REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE:
				// write request to standard device
				if (bRequest == 0x05) {
					// set address, handled by hardware
				} else if (bRequest == 0x09) {
					// set configuration
					uint8_t bConfigurationValue = NRF_USBD->WVALUEL;
					usb::onSetConfiguration(bConfigurationValue);
		
					// enter status stage
					NRF_USBD->TASKS_EP0STATUS = Trigger;
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = Trigger;
				}
				break;
			case IN | REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE:
				// read request to standard device
				if (bRequest == 0x06) {
					// get descriptor from user code by using the callback
					auto descriptorType = DescriptorType(NRF_USBD->WVALUEH);
					Data descriptor = usb::getDescriptor(descriptorType);
					if (descriptor.size > 0) {
						// send descriptor
						int wLength = (NRF_USBD->WLENGTHH << 8) | NRF_USBD->WLENGTHL;
						int size = min(descriptor.size, wLength);
						ep0Send(descriptor.data, size);
					} else {
						// unsupported descriptor type: stall
						NRF_USBD->TASKS_EP0STALL = Trigger;
					}
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = Trigger;
				}
				break;
			case OUT | REQUEST_TYPE_STANDARD | RECIPIENT_INTERFACE:
				// write request to standard interface
				if (bRequest == 0x11) {
					// set interface
					//uint8_t interfaceIndex = NRF_USBD->WINDEXL;
					//uint8_t alternateSettingIndex = NRF_USBD->WVALUEL;
	
					// enter status stage
					NRF_USBD->TASKS_EP0STATUS = Trigger;
				} else {
					// unsupported request: stall
					NRF_USBD->TASKS_EP0STALL = Trigger;
				}
				break;
			case OUT | REQUEST_TYPE_VENDOR | RECIPIENT_INTERFACE:
				{
					int wValue = (NRF_USBD->WVALUEH << 8) | NRF_USBD->WVALUEL;
					int wIndex = (NRF_USBD->WINDEXH << 8) | NRF_USBD->WINDEXL;
					
					// let user code handle the request
					bool result = usb::onRequest(bRequest, wValue, wIndex);
					if (result) {
						// enter status stage
						NRF_USBD->TASKS_EP0STATUS = Trigger;
					} else {
						// unsupported request: stall
						NRF_USBD->TASKS_EP0STALL = Trigger;
					}
				}
				break;
			default:
				// unsupported request type: stall
				NRF_USBD->TASKS_EP0STALL = Trigger;
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
				NRF_USBD->TASKS_STARTEPIN[0] = Trigger;
			} else {
				// finished: enter status stage
				NRF_USBD->TASKS_EP0STATUS = Trigger;
			}
		}
		
		// capture EPDATASTATUS here to be sure that we see the previous ENDEPOUT event
		uint32_t EPDATASTATUS = NRF_USBD->EPDATASTATUS;
		
		// handle end of receive (OUT) DMA transfer
		for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
			if (NRF_USBD->EVENTS_ENDEPOUT[index]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPOUT[index] = 0;
	
				auto& ep = usb::endpoints[index - 1];
				
				int receivedCount = NRF_USBD->EPOUT[index].AMOUNT;
				ep.receiveLength -= receivedCount;
				if (receivedCount == 64) {
					// more to receive
					NRF_USBD->EPOUT[index].PTR += 64;
				} else {
					// finished: calculate length of received data
					int length = ep.maxReceiveLength - ep.receiveLength;
					
					// resume first waiting coroutine
					ep.receiveWaitlist.resumeFirst([length](ReceiveParameters p){
						p.length = length;
						return true;
					});
					
					// check if there are more queued coroutines waiting for receive
					ep.receiveBusy = ep.receiveWaitlist.resumeFirst([&ep, index](ReceiveParameters p) {
						// set receive data
						ep.maxReceiveLength = p.length;
						ep.receiveLength = p.length;
						NRF_USBD->EPOUT[index].PTR = intptr_t(p.data);
					
						// write to start receiving into intermediate buffer
						NRF_USBD->SIZE.EPOUT[index] = 0;						

						// don't resume yet
						return false;						
					});
				}
			}
		}
		
		// handle end of transfer between host and internal buffer (both IN and OUT)
		if (EPDATASTATUS) {
			// clear pending interrupt flags at peripheral
			NRF_USBD->EVENTS_EPDATA = 0;
			NRF_USBD->EPDATASTATUS = EPDATASTATUS;
			
			for (int index = 1; index < USB_ENDPOINT_COUNT; ++index) {
				int inFlag = 1 << index;
				int outFlag = inFlag << 16;
				
				if (EPDATASTATUS & inFlag) {
					// finished send to host (IN)
					auto& ep = usb::endpoints[index - 1];
	
					// check if more to send
					int sentCount = NRF_USBD->EPIN[index].AMOUNT;
					int length = ep.sendLength - sentCount;
					ep.sendLength = length;
					if (sentCount == 64) {
						// more to send: Start DMA transfer from memory to internal buffer
						NRF_USBD->EPIN[index].PTR += 64;
						NRF_USBD->EPIN[index].MAXCNT = min(length, 64);
						NRF_USBD->TASKS_STARTEPIN[index] = Trigger;
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
							NRF_USBD->TASKS_STARTEPIN[index] = Trigger;
							
							// don't resume yet
							return false;
						});
					}
				}
	
				if (EPDATASTATUS & outFlag) {
					// finished receive from host (OUT)
					auto& ep = usb::endpoints[index - 1];
	
					// get length of data in internal buffer
					int length = NRF_USBD->SIZE.EPOUT[index];
	
					// start DMA transfer of received data from internal buffer to memory
					NRF_USBD->EPOUT[index].MAXCNT = min(ep.receiveLength, length);
					NRF_USBD->TASKS_STARTEPOUT[index] = Trigger;
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
	usb::nextHandler();
}

void init(
	std::function<Data (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration,
	std::function<bool (uint8_t, uint16_t, uint16_t)> const &onRequest)
{
	usb::getDescriptor = getDescriptor;
	usb::onSetConfiguration = onSetConfiguration;
	usb::onRequest = onRequest;
	
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

	// add to event loop handler chain
	usb::nextHandler = loop::addHandler(handle);
}

void enableEndpoints(uint8_t inFlags, uint8_t outFlags) {
	NRF_USBD->EPINEN = inFlags;
	NRF_USBD->EPOUTEN = outFlags;
}

Awaitable<ReceiveParameters> receive(int index, int &length, void *data) {
	assert(index >= 1 && endpoint < USB_ENDPOINT_COUNT);
	auto& ep = usb::endpoints[index - 1];
	
	// check if usb receiver is idle
	if (!ep.receiveBusy) {
		// set receive data
		ep.receiveBusy = true;
		ep.maxReceiveLength = length;
		ep.receiveLength = length;
		NRF_USBD->EPOUT[index].PTR = intptr_t(data);
	
		// write to start receiving into intermediate buffer
		NRF_USBD->SIZE.EPOUT[index] = 0;
	}

	return {ep.receiveWaitlist, length, data};
}

Awaitable<SendParameters> send(int index, int length, void const *data) {
	assert(index >= 1 && index < USB_ENDPOINT_COUNT);
	auto& ep = usb::endpoints[index - 1];
	
	// check if usb sender is idle
	if (!ep.sendBusy) {
		// set send data
		ep.sendBusy = true;
		ep.sendLength = length;
		NRF_USBD->EPIN[index].PTR = intptr_t(data);
		NRF_USBD->EPIN[index].MAXCNT = min(length, 64);
	
		// start send
		NRF_USBD->TASKS_STARTEPIN[index] = Trigger;
	}

	return {ep.sendWaitlist, length, data};
}

} // namespace usb

#include "../usb.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <util.hpp>


// usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml
namespace usb {

std::function<Array<uint8_t> (DescriptorType)> getDescriptor;
std::function<void (uint8_t)> onSetConfiguration;

// endpoint 0
uint8_t ep0Buffer[64] __attribute__((aligned(4)));
uint8_t const *ep0Data;
int ep0SendLength = 0;

void ep0Send(uint8_t const *data, int length) {
	int l = min(length, 64);
	array::copy(ep0Buffer, ep0Buffer + l, data);
	NRF_USBD->EPIN[0].PTR = intptr_t(ep0Buffer);
	NRF_USBD->EPIN[0].MAXCNT = l;
	NRF_USBD->TASKS_STARTEPIN[0] = Trigger;

	ep0Data = data;
	ep0SendLength = length;
}

// endpoints 1 - 7
struct Endpoint {
	int sendLength = 0;
	std::function<void ()> onSent;

	int maxReceiveLength;
	int receiveLength = 0;
	std::function<void (int)> onReceived;
};

Endpoint endpoints[USB_ENDPOINT_COUNT - 1];


// event loop handler chain
loop::Handler nextHandler;
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
					// get descriptor
					auto descriptorType = DescriptorType(NRF_USBD->WVALUEH);
					
					Array<uint8_t> descriptor = usb::getDescriptor(descriptorType);
					if (descriptor.length > 0) {
						// send descriptor
						int wLength = (NRF_USBD->WLENGTHH << 8) | NRF_USBD->WLENGTHL;
						int size = min(descriptor.length, wLength);
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
		for (int endpoint = 1; endpoint < USB_ENDPOINT_COUNT; ++endpoint) {
			if (NRF_USBD->EVENTS_ENDEPOUT[endpoint]) {
				// clear pending interrupt flag at peripheral
				NRF_USBD->EVENTS_ENDEPOUT[endpoint] = 0;
	
				Endpoint& ep = usb::endpoints[endpoint - 1];
				
				int receivedCount = NRF_USBD->EPOUT[endpoint].AMOUNT;
				ep.receiveLength -= receivedCount;
				if (receivedCount == 64) {
					// more to receive
					NRF_USBD->EPOUT[endpoint].PTR += 64;
				} else {
					// finished
					int length = ep.maxReceiveLength - ep.receiveLength;
					ep.receiveLength = 0;
					ep.onReceived(length);
				}
			}
		}
		
		// handle end of transfer between host and internal buffer (both IN and OUT)
		if (EPDATASTATUS) {
			// clear pending interrupt flags at peripheral
			NRF_USBD->EVENTS_EPDATA = 0;
			NRF_USBD->EPDATASTATUS = EPDATASTATUS;
			
			for (int endpoint = 1; endpoint < USB_ENDPOINT_COUNT; ++endpoint) {
				int inFlag = 1 << endpoint;
				int outFlag = inFlag << 16;
				
				if (EPDATASTATUS & inFlag) {
					// finished send to host (IN)
					Endpoint& ep = usb::endpoints[endpoint - 1];
	
					// check if more to send
					int sentCount = NRF_USBD->EPIN[endpoint].AMOUNT;
					int length = ep.sendLength - sentCount;
					ep.sendLength = length;
					if (sentCount == 64) {
						// more to send: Start DMA transfer from memory to internal buffer
						NRF_USBD->EPIN[endpoint].PTR += 64;
						NRF_USBD->EPIN[endpoint].MAXCNT = min(length, 64);
						NRF_USBD->TASKS_STARTEPIN[endpoint] = Trigger;
					} else {
						// finished
						ep.onSent();
					}
				}
	
				if (EPDATASTATUS & outFlag) {
					// finished receive from host (OUT)
					Endpoint& ep = usb::endpoints[endpoint - 1];
	
					// get length of data in internal buffer
					int length = NRF_USBD->SIZE.EPOUT[endpoint];
	
					// start DMA transfer of received data from internal buffer to memory
					NRF_USBD->EPOUT[endpoint].MAXCNT = min(ep.receiveLength, length);
					NRF_USBD->TASKS_STARTEPOUT[endpoint] = Trigger;
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

void init(std::function<Array<uint8_t> (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration)
{
	usb::getDescriptor = getDescriptor;
	usb::onSetConfiguration = onSetConfiguration;
	
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

bool send(int endpoint, void const *data, int length, std::function<void ()> const &onSent) {
	assert(endpoint >= 1 && endpoint < USB_ENDPOINT_COUNT);
	Endpoint& ep = usb::endpoints[endpoint - 1];
	if (ep.sendLength != 0)
		return false;
	ep.onSent = onSent;
	
	NRF_USBD->EPIN[endpoint].PTR = intptr_t(data);
	NRF_USBD->EPIN[endpoint].MAXCNT = min(length, 64);
	NRF_USBD->TASKS_STARTEPIN[endpoint] = Trigger;
	
	ep.sendLength = length;
	return true;
}

bool receive(int endpoint, void *data, int maxLength, std::function<void (int)> const &onReceived) {
	assert(endpoint >= 1 && endpoint < USB_ENDPOINT_COUNT);
	Endpoint& ep = usb::endpoints[endpoint - 1];
	if (ep.receiveLength != 0)
		return false;
	ep.onReceived = onReceived;

	NRF_USBD->EPOUT[endpoint].PTR = intptr_t(data);

	// write to start receiving into intermediate buffer
	NRF_USBD->SIZE.EPOUT[endpoint] = 0;

	ep.maxReceiveLength = maxLength;
	ep.receiveLength = maxLength;
	return true;
}

} // namespace usb

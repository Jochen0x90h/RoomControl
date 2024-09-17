#include "../UsbDevice.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"


/**
 * Implementation of an SPI master that simply writes info about the transfer operations to Terminal::out
 */
class UsbDeviceImpl : public UsbDevice, public loop::Handler2 {
public:
	// number of endpoints without endpoint 0
	static constexpr int ENDPOINT_COUNT = 7;

	/**
	 * Constructor
	 * @param getDescriptor callback for obtaining descriptors
	 * @param onSetConfiguration callback for setting the configuration (libusb_set_configuration() on host), always called from event loop
	 * @param onRequest callback for vendor specific request
	 */
	UsbDeviceImpl(
		std::function<ConstData (usb::DescriptorType)> const &getDescriptor,
		std::function<void (UsbDevice &usb, uint8_t bConfigurationValue)> const &onSetConfiguration,
		std::function<bool (uint8_t bRequest, uint16_t wValue, uint16_t wIndex)> const &onRequest);

	void enableEndpoints(uint8_t inFlags, uint8_t outFlags) override;
	[[nodiscard]] Awaitable<ReceiveParameters> receive(int index, int &length, void *data) override;
	[[nodiscard]] Awaitable<SendParameters> send(int index, int length, void const *data) override;

	void handle() override;

protected:

	std::function<ConstData (usb::DescriptorType)> getDescriptor;
	std::function<void (UsbDevice &, uint8_t)> onSetConfiguration;
	std::function<bool (uint8_t, uint16_t, uint16_t)> onRequest;


	// endpoint 0
	uint8_t ep0Buffer[64] __attribute__((aligned(4)));
	uint8_t const *ep0Data;
	int ep0SendLength = 0;
	
	void ep0Send(void const *data, int length)  {
		auto d = reinterpret_cast<uint8_t const *>(data);
		int l = std::min(length, 64);
		//array::copy(ep0Buffer, ep0Buffer + l, d);
		std::copy(d, d + l, ep0Buffer);
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
			NRF_USBD->EPOUT[index].MAXCNT = std::min(this->receiveLength, bufferLength);
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
			NRF_USBD->EPIN[index].MAXCNT = std::min(this->sendLength, 64);
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
	Endpoint endpoints[ENDPOINT_COUNT];
};

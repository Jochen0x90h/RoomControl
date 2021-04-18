#pragma once

#include <Data.hpp>
#include <cstdint>
#include <functional>


namespace usb {

// transfer direction
enum Direction {
	OUT = 0, // to device
	IN = 0x80 // to host
};

// descriptor type
enum DescriptorType {
	DESCRIPTOR_DEVICE = 0x01,
	DESCRIPTOR_CONFIGURATION = 0x02,
	DESCRIPTOR_INTERFACE = 0x04,
	DESCRIPTOR_ENDPOINT = 0x05
};

// endpoint type
enum EndpointType {
	ENDPOINT_CONTROL = 0,
	ENDPOINT_ISOCHRONOUS = 1,
	ENDPOINT_BULK = 2,
	ENDPOINT_INTERRUPT = 3
};

// control request type
enum RequestType {
	REQUEST_TYPE_MASK = (0x03 << 5),
	REQUEST_TYPE_STANDARD = (0x00 << 5),
	REQUEST_TYPE_CLASS = (0x01 << 5),
	REQUEST_TYPE_VENDOR = (0x02 << 5),
	REQUEST_TYPE_RESERVED = (0x03 << 5)
};

// control request recipient
enum RequestRecipient {
	RECIPIENT_MASK = 0x1f,
	RECIPIENT_DEVICE = 0x00,
	RECIPIENT_INTERFACE = 0x01,
	RECIPIENT_ENDPOINT = 0x02,
	RECIPIENT_OTHER = 0x03
};


// device descriptor
struct DeviceDescriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
} __attribute__((packed));

// configuration descriptor
struct ConfigDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__((packed));

// interface descriptor
struct InterfaceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__((packed));

// endpoint descriptor
struct EndpointDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__((packed));




/**
 * Initialize USB
 */
void init(std::function<Data (DescriptorType)> const &getDescriptor,
	std::function<void (uint8_t)> const &onSetConfiguration);

/**
 * Enable endpoints. Can be done in onSetConfiguration. Endpoint 0 should stay enabled
 */
void enableEndpoints(uint8_t inFlags, uint8_t outFlags);

/**
 * Send data over an endpoint (IN transfer)
 * @param endpont endpoint index (1-7)
 * @param data data to send, must be in ram and 32 bit aligned
 * @param length data length
 * @param onSent called when finished sending
 * @return true on success, false if busy with previous send
 */
bool send(int endpoint, void const *data, int length, std::function<void ()> const &onSent);

/**
 * Receive data over an endpoint (OUT transfer)
 * @param endpont endpoint index (1-7)
 * @param data data to receive, must be in ram and 32 bit aligned
 * @param maxLength maximum length of data to receive
 * @param onReceived called when data was received
 */
bool receive(int endpoint, void *data, int maxLength, std::function<void (int)> const &onReceived);

} // namespace usb

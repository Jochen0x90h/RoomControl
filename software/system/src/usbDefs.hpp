#pragma once

#include <cstdint>


namespace usb {

// descriptor type
enum class DescriptorType : uint8_t {
	DEVICE = 1,
	CONFIGURATION = 2,
	INTERFACE = 4,
	ENDPOINT = 5
};

// endpoint type
namespace EndpointType {
	constexpr uint8_t CONTROL = 0;
	constexpr uint8_t ISOCHRONOUS = 1;
	constexpr uint8_t BULK = 2;
	constexpr uint8_t INTERRUPT = 3;
}

// transfer direction
enum Direction {
	OUT = 0, // to device
	IN = 0x80 // to host
};

// control request type
enum RequestType {
	REQUEST_TYPE_MASK = 0x3 << 5,
	REQUEST_TYPE_STANDARD = 0x0 << 5,
	REQUEST_TYPE_CLASS = 0x1 << 5,
	REQUEST_TYPE_VENDOR = 0x2 << 5,
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
	DescriptorType  bDescriptorType;
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
	DescriptorType bDescriptorType;
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
	DescriptorType bDescriptorType;
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
	DescriptorType bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__((packed));

} // namespace usb

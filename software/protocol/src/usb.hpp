#pragma once

#include <enum.hpp>
#include <cstdint>

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#else
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif


namespace usb {

// descriptor type
enum class DescriptorType : uint8_t {
	DEVICE = 1,
	CONFIGURATION = 2,
	INTERFACE = 4,
	ENDPOINT = 5
};

// endpoint type
enum class EndpointType : uint8_t {
	CONTROL = 0,
	ISOCHRONOUS = 1,
	BULK = 2,
	INTERRUPT = 3
};

// endpoint transfer direction
enum Direction {
	OUT = 0, // to device
	IN = 0x80 // to host
};

// control request
enum class Request : uint8_t {
	// request type
	TYPE_MASK = 0x3 << 5,
	TYPE_STANDARD = 0x0 << 5,
	TYPE_CLASS = 0x1 << 5,
	TYPE_VENDOR = 0x2 << 5,

	// request recipient
	RECIPIENT_MASK = 0x1f,
	RECIPIENT_DEVICE = 0x00,
	RECIPIENT_INTERFACE = 0x01,
	RECIPIENT_ENDPOINT = 0x02,
	RECIPIENT_OTHER = 0x03,

	// request direction
	OUT = 0, // to device
	IN = 0x80, // to host

	// combinations
	STANDARD_DEVICE_OUT = TYPE_STANDARD | RECIPIENT_DEVICE | OUT,
	STANDARD_DEVICE_IN = TYPE_STANDARD | RECIPIENT_DEVICE | IN,
	STANDARD_INTERFACE_OUT = TYPE_STANDARD | RECIPIENT_INTERFACE | OUT,
	STANDARD_INTERFACE_IN = TYPE_STANDARD | RECIPIENT_INTERFACE | IN,
	VENDOR_DEVICE_OUT = TYPE_VENDOR | RECIPIENT_DEVICE | OUT,
	VENDOR_INTERFACE_OUT = TYPE_VENDOR | RECIPIENT_INTERFACE | OUT,
};
FLAGS_ENUM(Request)

// device descriptor
PACK(struct DeviceDescriptor {
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
});

// configuration descriptor
PACK(struct ConfigDescriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
});

// interface descriptor
PACK(struct InterfaceDescriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
});

// endpoint descriptor
PACK(struct EndpointDescriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint8_t bEndpointAddress;
	EndpointType bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
});

} // namespace usb

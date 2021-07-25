#pragma once

#include "Device.hpp"
#include <crypt.hpp>


/**
 * Global configuration
 */
struct Configuration {
	// name of this room control
	char name[16];

	// ieee 802.15.4 long address
	DeviceId longAddress;
	
	// pan id for radio devices
	uint16_t zbPanId;

	// network key
	DataBuffer<16> networkKey;

	// network key prepared for aes encryption
	AesKey networkAesKey;

	/**
	 * Returns the size in bytes needed to store the configuration in flash
	 * @return size in bytes
	 */
	int getFlashSize() const {return sizeof(Configuration);}

	/**
	 * Returns the size in bytes needed to store the contol state in ram
	 * @return size in bytes
	 */
	int getRamSize() const {return 0;}
};

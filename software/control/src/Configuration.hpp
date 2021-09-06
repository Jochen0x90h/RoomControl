#pragma once

#include "Device.hpp"
#include "Storage2.hpp"
#include <crypt.hpp>
#include <iostream>


class Configuration;

/**
 * Global configuration
 */
struct ConfigurationFlash {
	
	// name of this room control
	char name[16];

	// ieee 802.15.4 long address
	DeviceId longAddress;
	
	// pan id for RadioInterface
	uint16_t zbPanId;

	// network key
	DataBuffer<16> networkKey;

	// network key prepared for aes encryption
	AesKey networkAesKey;


	// state offsets for interfaces
	uint16_t busSecurityCounterOffset;
	uint16_t radioSecurityCounterOffset;


	/**
	 * Returns the size in bytes needed to store the configuration in flash
	 * @return size in bytes
	 */
	int size() const {return sizeof(ConfigurationFlash);}
	
	/**
	 * Allocate configuration
	 */
	Configuration *allocate() const;
};

class Configuration : public Storage2::Element<ConfigurationFlash> {
public:

	Configuration(ConfigurationFlash const &flash) : Storage2::Element<ConfigurationFlash>(flash) {}

};

inline Configuration *ConfigurationFlash::allocate() const {return new Configuration(*this);}

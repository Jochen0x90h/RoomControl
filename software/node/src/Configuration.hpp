#pragma once

#include "Storage.hpp"
#include <Network.hpp>
#include <crypt.hpp>
#include <String.hpp>


class Configuration;

/**
 * Global configuration
 */
struct ConfigurationFlash {
	
	// name of this node
	char name[16];

// encryption

	// same key used for all interfaces
	DataBuffer<16> key;

	// key prepared for aes encryption
	AesKey aesKey;


// bus

	// state offsets for bus interface
	uint16_t busSecurityCounterOffset;


// radio

	// ieee 802.15.4 long address
	uint64_t ieeeLongAddress;
	
	// pan id for RadioInterface
	uint16_t zbeePanId;

	// state offsets for radio interface
	uint16_t zbeeSecurityCounterOffset;


// network

	// local port and endpoint for connection to mqtt-sn gateway
	uint16_t mqttLocalPort;
	Network::Endpoint mqttGateway;



	/**
	 * Returns the size in bytes needed to store the configuration in flash
	 * @return size in bytes
	 */
	int size() const {return sizeof(ConfigurationFlash);}
	
	String getName() const {return String(this->name);}
	
	/**
	 * Allocate configuration
	 */
	Configuration *allocate() const;
};

class Configuration : public Storage::Element<ConfigurationFlash> {
public:

	Configuration(ConfigurationFlash const &flash) : Storage::Element<ConfigurationFlash>(flash) {}

};

inline Configuration *ConfigurationFlash::allocate() const {return new Configuration(*this);}

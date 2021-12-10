#pragma once

#include "Storage2.hpp"
#include <network.hpp>
#include <crypt.hpp>
#include <String.hpp>


class Configuration;

/**
 * Global configuration
 */
struct ConfigurationFlash {
	
	// name of this node
	char name[16];

	// encryption key
	DataBuffer<16> key;

	// encryption key prepared for aes encryption
	AesKey aesKey;


// bus interface

	// state offsets for bus interface
	uint16_t busSecurityCounterOffset;


// radio interface

	// ieee 802.15.4 long address
	uint64_t radioLongAddress;
	
	// pan id for RadioInterface
	uint16_t radioPanId;

	// state offsets for radio interface
	uint16_t radioSecurityCounterOffset;


// network

	network::Endpoint networkGateway;
	uint16_t networkLocalPort;



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

class Configuration : public Storage2::Element<ConfigurationFlash> {
public:

	Configuration(ConfigurationFlash const &flash) : Storage2::Element<ConfigurationFlash>(flash) {}

};

inline Configuration *ConfigurationFlash::allocate() const {return new Configuration(*this);}

#pragma once

#include "DataBuffer.hpp"
#include "zb.hpp"


class Nonce : public DataBuffer<13> {
public:
	/**
	 * Constructor
	 */
	Nonce(uint8_t const *sourceAddress, uint32_t securityCounter, zb::SecurityControl securityControl) {
		setData(0, sourceAddress, 8);
		setLittleEndianInt32(8, securityCounter);
		setInt8(12, uint8_t(securityControl));
	}

	/**
	 *	Constructor
	 */
	Nonce(uint64_t sourceAddress, uint32_t securityCounter, zb::SecurityControl securityControl) {
		setLittleEndianInt64(0, sourceAddress);
		setLittleEndianInt32(8, securityCounter);
		setInt8(12, uint8_t(securityControl));
	}

	/**
	 * Constructor for self powered devices
	 */
	Nonce(uint32_t deviceId, uint32_t securityCounter) {
		setLittleEndianInt32(0, deviceId);
		setLittleEndianInt32(4, deviceId);
		setLittleEndianInt32(8, securityCounter);
		setInt8(12, 0x05);
	}
};

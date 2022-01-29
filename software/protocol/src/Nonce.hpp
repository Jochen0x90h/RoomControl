#pragma once

#include "DataBuffer.hpp"
#include "zb.hpp"


class Nonce : public DataBuffer<13> {
public:
	/**
	 * Constructor
	 */
	Nonce(uint8_t const *sourceAddress, uint32_t securityCounter, zb::SecurityControl securityControl) {
		setData(0, 8, sourceAddress);
		setU32L(8, securityCounter);
		setU8(12, uint8_t(securityControl));
	}

	/**
	 *	Constructor
	 */
	Nonce(uint64_t sourceAddress, uint32_t securityCounter, zb::SecurityControl securityControl) {
		setU64L(0, sourceAddress);
		setU32L(8, securityCounter);
		setU8(12, uint8_t(securityControl));
	}

	/**
	 * Constructor for self powered devices
	 */
	Nonce(uint32_t deviceId, uint32_t securityCounter) {
		setU32L(0, deviceId);
		setU32L(4, deviceId);
		setU32L(8, securityCounter);
		setU8(12, 0x05);
	}
};

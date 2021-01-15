#pragma once

#include "global.hpp"
#include "Array.hpp"
#include "Device.hpp"
#include "Gui.hpp"


/**
 * Bus for connecting devices using wires
 * Emulator implementation uses virtual devices on user interface
 */
class Bus {
public:

	/**
	 * Constructor
	 * @param parameters platform dependent parameters
	 */
	Bus();

	virtual ~Bus();

	/**
	 * Notify that the bus is ready
	 */
	virtual void onBusReady() = 0;
	
	/**
	 * Get list of devices connected to the bus. Is not valid before onBusReady() gets called
	 * @param deviceIndex index of device
	 * @return list of device ids
	 */
	Array<DeviceId> getBusDevices();
	
	/**
	 * Get list of endpoints for a device
	 * @param deviceIndex index of device
	 * @return list of endpoints
	 */
	Array<EndpointType> getBusDeviceEndpoints(DeviceId deviceId);

	/**
	 * Subscribe to an endpoint on the bus
	 * @param deviceId device id
	 * @param endpointIndex endpoint index of device
	 * @return endpoint id
	 */
	//uint8_t subscribeBus(uint32_t deviceId, uint8_t endpointIndex);

	/**
	 * Subscribe to an endpoint on the bus
	 * @param receives the endpoint id. A new reference is counted if it was zero, otherwise it must be the same id
	 * @param deviceId device id
	 * @param endpointIndex endpoint index of device
	 */
	void subscribeBus(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex);

	/**
	 * Unsubscribe from an endpoint on the bus
	 * @param deviceId device id
	 * @param endpointIndex endpoint index of device
	 * @return endpoint id
	 */
	void unsubscribeBus(uint8_t &endpointId);

	/**
	 * Called when a message was received
	 * @param endpointId sender of the message
	 * @param data message data
	 * @param length message length
	 */
	virtual void onBusReceived(uint8_t endpointId, uint8_t const *data, int length) = 0;

	/**
	 * Send a message to a lin device
	 * @param endpointId receiver of the message
	 * @param data message data
	 * @param length message length, maximum is 8
	 */
	void busSend(uint8_t endpointId, uint8_t const *data, int length);
	
	/**
	 * Called when send operation has finished
	 */
	virtual void onBusSent() = 0;

	/**
	 * Returns true when sending is in progress
	 */
	bool isBusSendBusy() {return this->sendBusy;}

	// only for emulator
	void doGui(int &id);
	
private:
	
	//uint8_t nextEndpointId = 1;
	
	struct State {
		uint8_t endpointId;
		uint8_t relays;
		int blind;
	};
	State states[32];

	bool sendBusy = false;

	uint8_t receiveData[8];
	
	std::chrono::steady_clock::time_point time;
};

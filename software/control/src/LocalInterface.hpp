#pragma once

#include "Interface.hpp"
#include "BME680.hpp"
#include <Coroutine.hpp>


/**
 * Interface to locally connected devices
 */
class LocalInterface : public Interface {
public:
	//static constexpr int TIMER_INDEX = LOCAL_DEVICES_TIMER_INDEX;

	LocalInterface(std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~LocalInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	//Array<DeviceId> getDevices() override;
	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;
	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;
	
protected:

	Coroutine measure();
/*
	void onAirSensorInitialized();

	void measure();

	void readAirSensor();
	void airSensorGetValues();
*/
	
	// timer index
	static int const timerIndex = TIMER_LOCAL_INTERFACE;

	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	BME680 airSensor;

	uint8_t subscriptions[10];
};

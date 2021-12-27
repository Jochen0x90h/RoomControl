#pragma once

#include "Interface.hpp"
#include "BME680.hpp"
#include <Coroutine.hpp>


class LocalInterface : public Interface {
public:
	// device id's
	enum DeviceIds {
		BME680_ID = 1,
		BRIGHTNESS_SENSOR_ID = 2,
		MOTION_DETECTOR_ID = 3,
		IN_ID = 4, // generic binary input
		OUT_ID = 5, // generic binary output
		DEVICE_COUNT = 5
	};

	LocalInterface();

	~LocalInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	Array<EndpointType const> getEndpoints(DeviceId deviceId) override;
	
	void addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) override;

	void addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) override;

protected:

	struct Device {
		uint8_t id;
		
		SubscriberList subscribers;
		PublisherList publishers;
	};

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor();
	
	Coroutine publish();

	uint8_t deviceCount;
	Device devices[DEVICE_COUNT];
	
	Event publishEvent;
};

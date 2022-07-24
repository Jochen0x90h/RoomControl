#pragma once

#include "Interface.hpp"
#include "BME680.hpp"


class LocalInterface : public Interface {
public:
	// device id's
	enum DeviceIds {
		BME680_ID = 1, // air sensor
		HEATING_ID = 2, // heating
		BRIGHTNESS_SENSOR_ID = 3,
		MOTION_DETECTOR_ID = 4,
		IN_ID = 5, // generic binary input
		OUT_ID = 6, // generic binary output
		DEVICE_COUNT = 6
	};

	LocalInterface();

	~LocalInterface() override;

	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	//Device *getDevice(uint8_t id) override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	void subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) override;
	PublishInfo getPublishInfo(uint8_t id, uint8_t plugIndex) override;
	void erase(uint8_t id) override;

protected:

	struct LocalDevice {
		// subscribers and publishers
		SubscriberList subscribers;
	};

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor();
	
	Coroutine publish();

	uint8_t deviceCount;
	uint8_t deviceIds[DEVICE_COUNT];
	LocalDevice devices[DEVICE_COUNT];

	PublishInfo::Barrier publishBarrier;
};

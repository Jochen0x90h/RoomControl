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

	int getDeviceCount() override;
	Device &getDeviceByIndex(int index) override;
	Device *getDeviceById(uint8_t id) override;

protected:

	class LocalDevice : public Device {
	public:
		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<EndpointType const> getEndpoints() const override;
		void addPublisher(uint8_t endpointIndex, Publisher &publisher) override;
		void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) override;

		// back pointer to interface
		LocalInterface *interface;

		// interface id
		uint8_t interfaceId;

		// subscribers and publishers
		SubscriberList subscribers;
		PublisherList publishers;
	};

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor();
	
	Coroutine publish();

	uint8_t deviceCount;
	LocalDevice devices[DEVICE_COUNT];
	
	Event publishEvent;
};

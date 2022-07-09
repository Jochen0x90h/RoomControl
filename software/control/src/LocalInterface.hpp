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
	Device *getDevice(uint8_t id) override;
	void eraseDevice(uint8_t id) override;

protected:

	class LocalDevice : public Device {
	public:
		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<MessageType const> getPlugs() const override;
		void subscribe(uint8_t endpointIndex, Subscriber &subscriber) override;
		PublishInfo getPublishInfo(uint8_t endpointIndex) override;

		void init(LocalInterface *interface, uint8_t interfaceId, Array<MessageType const> endpoints) {
			this->interface = interface;
			this->interfaceId = interfaceId;
			this->endpoints = endpoints;
		}

		// back pointer to interface
		LocalInterface *interface;

		// interface id
		uint8_t interfaceId;

		// endpoints
		Array<MessageType const> endpoints;

		// subscribers and publishers
		SubscriberList subscribers;
	};

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor();
	
	Coroutine publish();

	uint8_t deviceCount;
	LocalDevice devices[DEVICE_COUNT];

	PublishInfo::Barrier publishBarrier;
};

#pragma once

#include "Interface.hpp"
#include "BME680.hpp"


class LocalInterface : public Interface {
public:
	// device id's
	enum DeviceIds {
		WHEEL_ID = 1, // wheel (rotary encoder)
		BME680_ID = 2, // air sensor
		IN_ID = 3, // generic binary input
		OUT_ID = 4, // generic binary output
		HEATING_ID = 5, // heating
		SOUND_ID = 6, // audio via built-in (or bluetooth) speaker
		BRIGHTNESS_SENSOR_ID = 7,
		MOTION_DETECTOR_ID = 8,
		DEVICE_COUNT = 8
	};

	LocalInterface(uint8_t interfaceId, SpiMaster &airSensor);

	~LocalInterface() override;

	String getName() override;
	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getElementIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	SubscriberTarget getSubscriberTarget(uint8_t id, uint8_t plugIndex) override;
	void subscribe(Subscriber &subscriber) override;
	void listen(Listener &listener) override;
	void erase(uint8_t id) override;

	// set number of plugs for the wheel device
	void setWheelPlugCount(uint8_t plugCount) {this->wheelPlugCount = plugCount;}

	// publish increment of digital potentiometer
	void publishWheel(uint8_t plugIndex, int8_t delta);

protected:

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor(SpiMaster &spi);

	// handles messages published to devices in this interface
	Coroutine publish();


	uint8_t deviceCount;
	uint8_t deviceIds[DEVICE_COUNT];
	Element devices[DEVICE_COUNT];
	uint8_t wheelPlugCount = 1;

	// sounds
	int soundCount;
	MessageType soundPlugs[32];

	SubscriberBarrier publishBarrier;

	// listeners that listen on all messages of the interface (as opposed to subscribers that subscribe to one plug)
	ListenerList listeners;
};

#pragma once

#include "Interface.hpp"
#include "BME680.hpp"


class LocalInterface : public Interface {
public:
	// device id's
	enum DeviceIds {
		BME680_ID = 1, // air sensor
		IN_ID = 2, // generic binary input
		OUT_ID = 3, // generic binary output
		HEATING_ID = 4, // heating
		SOUND_ID = 5, // audio via built-in (or bluetooth) speaker
		BRIGHTNESS_SENSOR_ID = 6,
		MOTION_DETECTOR_ID = 7,
		DEVICE_COUNT = 7
	};

	LocalInterface();

	~LocalInterface() override;

	String getName() override;
	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	SubscriberInfo getSubscriberInfo(uint8_t id, uint8_t plugIndex) override;
	void subscribe(Subscriber &subscriber) override;
	void listen(Listener &listener) override;
	void erase(uint8_t id) override;

protected:

	// reads the air sensor every minute and publishes the values to the subscribers
	Coroutine readAirSensor();

	// handles messages published to devices in this interface
	Coroutine publish();


	uint8_t deviceCount;
	uint8_t deviceIds[DEVICE_COUNT];
	Device devices[DEVICE_COUNT];

	// sounds
	int soundCount;
	MessageType soundPlugs[32];

	MessageBarrier publishBarrier;

	// listeners that listen on all messages of the interface (as opposed to subscribers that subscribe to one plug)
	ListenerList listeners;
};

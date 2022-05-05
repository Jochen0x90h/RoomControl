#pragma once

#include "Interface.hpp"
#include <Storage.hpp>
#include <ClockTime.hpp>
#include <Coroutine.hpp>


class AlarmInterface : public Interface {
public:
	AlarmInterface();
	~AlarmInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	Device &getDeviceByIndex(int index) override;
	Device *getDeviceById(uint8_t id) override;

protected:
	class Alarm;

public:
	struct AlarmFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 4;

		AlarmTime time;

		// interface id
		uint8_t interfaceId;

		// endpoints that send messages when the alarm goes off
		uint8_t endpointCount = 0;
		EndpointType endpoints[MAX_ENDPOINT_COUNT];
		MessageType messageTypes[MAX_ENDPOINT_COUNT];
		Message messages[MAX_ENDPOINT_COUNT];

		// note: messages must be the last member because of variable size allocation

		AlarmFlash() = default;
		AlarmFlash(AlarmFlash const &flash);

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const;

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		Alarm *allocate() const;
	};

	AlarmFlash const &getAlarm(int index) const;
	void setAlarm(int index, AlarmFlash &flash);
	void eraseAlarm(int index) {this->alarms.erase(index);}

protected:
	class Alarm : public Storage::Element<AlarmFlash>, public Device {
	public:
		Alarm(AlarmFlash const &flash) : Storage::Element<AlarmFlash>(flash) {}

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<EndpointType const> getEndpoints() const override;
		void addPublisher(uint8_t endpointIndex, Publisher &publisher) override;
		void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) override;

		// back pointer to interface
		AlarmInterface *interface;

		SubscriberList subscribers;
	};

public:
	// list of alarms
	Storage::Array<Alarm> alarms;

protected:

	Coroutine tick();
};

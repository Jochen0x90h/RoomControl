#pragma once

#include "Interface.hpp"
#include <ClockTime.hpp>
#include <Coroutine.hpp>


class AlarmInterface : public Interface {
public:
	// maximum number of alarms
	static constexpr int MAX_ALARM_COUNT = 64;

	AlarmInterface();
	~AlarmInterface() override;

	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	void subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) override;
	PublishInfo getPublishInfo(uint8_t id, uint8_t plugIndex) override;
	void erase(uint8_t id) override;

protected:
	class Alarm;

public:
	// alarm data that is stored in flash
	struct AlarmData {
		static constexpr int MAX_PLUG_COUNT = 8;

		uint8_t id;

		// device name, zero-terminated if shorter than maximum length
		char name[MAX_NAME_LENGTH];

		// endpoints that send messages when the alarm goes off
		uint8_t plugCount = 1;

		AlarmTime time;
	};

	/**
	 * Get the data of an alarm
	 * @param id id of alarm
	 * @return configuration data of alarm
	 */
	AlarmData const *get(uint8_t id) const;

	/**
	 * Set an alarm
	 * @param id id of alarm, new alarm with new id gets created if not found (use 0 for new alarms)
	 * @param data configuration data of alarm
	 */
	void set(uint8_t id, AlarmData &data);

	/**
	 * Get number of subscribers of an alarm
	 * @param id id of alarm
	 * @param plugCount override number of active plugs
	 * @param command command: 0: deactivate, 1: activate
	 * @return number of subscribers
	 */
	int getSubscriberCount(uint8_t id, int plugCount, uint8_t command);

	/**
	 * Test an alarm by publishing its messages using given configuration
	 * @param id id of alarm
	 * @param endpointCount override number of active endpoints
	 * @param command command: 0: deactivate, 1: activate
	 */
	void test(uint8_t id, int endpointCount, uint8_t command);

protected:

	class Alarm {
	public:
		Alarm(AlarmInterface *interface, AlarmData const &data)
			: next(interface->alarms), data(data)
		{
			interface->alarms = this;
		}

		// next alarm
		Alarm *next;

		// zbee device data that is stored in flash
		AlarmData data;

		// list of subscribers
		SubscriberList subscribers;

		// true when alarm is active
		bool active = false;
	};

	Alarm *getAlarm(uint8_t id) const;

	int alarmCount = 0;
	uint8_t alarmIds[MAX_ALARM_COUNT];
	Alarm *alarms = nullptr;

	uint8_t allocateId();

	Coroutine tick();
};

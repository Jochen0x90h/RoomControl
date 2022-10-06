#pragma once

#include "Interface.hpp"
#include <ClockTime.hpp>
#include <Coroutine.hpp>


class AlarmInterface : public Interface {
public:
	// maximum number of alarms
	static constexpr int MAX_ALARM_COUNT = 64;

	AlarmInterface(uint8_t interfaceId);
	~AlarmInterface() override;

	String getName() override;
	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	SubscriberTarget getSubscriberTarget(uint8_t id, uint8_t plugIndex) override;
	void subscribe(Subscriber &subscriber) override;
	void listen(Listener &listener) override;
	void erase(uint8_t id) override;


	// alarm data that is stored in flash
	struct Data {
		uint8_t id;

		// device name, zero-terminated if shorter than maximum length
		char name[MAX_NAME_LENGTH];

		uint8_t reserved;

		AlarmTime time;
	};

	/**
	 * Get the data of an alarm
	 * @param id id of alarm
	 * @return configuration data of alarm
	 */
	Data const *get(uint8_t id) const;

	/**
	 * Set an alarm
	 * @param id id of alarm, new alarm with new id gets created if not found (use 0 for new alarms)
	 * @param data configuration data of alarm
	 */
	void set(uint8_t id, Data &data);

	/**
	 * Get number of subscribers of an alarm
	 * @param id id of alarm
	 * @param plugCount override number of active plugs
	 * @param command command: 0: deactivate, 1: activate
	 * @return number of subscribers
	 */
	int getSubscriberCount(uint8_t id, uint8_t command);

	/**
	 * Test an alarm by publishing its messages using given configuration
	 * @param id id of alarm
	 * @param endpointCount override number of active endpoints
	 * @param command command: 0: deactivate, 1: activate
	 */
	void test(uint8_t id, uint8_t command);

protected:

	class Alarm : public Device {
	public:
		// Constructor, add alarm to linked list
		Alarm(AlarmInterface *interface, Data const &data)
			: Device(data.id, interface->listeners), next(interface->alarms), data(data)
		{
			interface->alarms = this;
		}

		// next alarm
		Alarm *next;

		// alarm data that is stored in flash
		Data data;

		// true when alarm is active
		bool active = false;
	};

	int alarmCount = 0;
	uint8_t alarmIds[MAX_ALARM_COUNT];
	Alarm *alarms = nullptr;

	// find alarm by id
	Alarm *findAlarm(uint8_t id) const;

	uint8_t allocateId();

	Coroutine tick();

	// listeners that listen on all messages of the interface (as opposed to subscribers that subscribe to one plug)
	ListenerList listeners;
};

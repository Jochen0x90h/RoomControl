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
		MessageType endpoints[MAX_ENDPOINT_COUNT];
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

	/**
	 * Get an alarm
	 * @param index index of alarm
	 * @return flash configuration of alarm
	 */
	AlarmFlash const &get(int index) const;

	/**
	 * Set an alarm
	 * @param index index of alarm
	 * @param flash flash configuration of alarm
	 */
	void set(int index, AlarmFlash &flash);

	/**
	 * Erase an alarm
	 * @param index index of alarm
	 */
	void erase(int index) {this->alarms.erase(index);}

	/**
	 * Get number of subscribers of an alarm
	 * @param index index of alarm
	 * @return number of subscribers
	 */
	int getSubscriberCount(int index);

	/**
	 * Test an alarm by publishing its messages using given configuration
	 * @param index index of alarm
	 * @param flash flash configuration of alarm
	 */
	void test(int index, AlarmFlash const &flash);

protected:
	class Alarm : public Storage::Element<AlarmFlash>, public Device {
	public:
		explicit Alarm(AlarmFlash const &flash) : Storage::Element<AlarmFlash>(flash) {}
		~Alarm() override;

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<MessageType const> getEndpoints() const override;
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

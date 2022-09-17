#include "AlarmInterface.hpp"
#include <Timer.hpp>
#include <Calendar.hpp>
#include <Storage.hpp>
#include <util.hpp>
#include <appConfig.hpp>


static MessageType const plugs[1] = {MessageType::BINARY_ALARM_OUT};


AlarmInterface::AlarmInterface() {
	// load list of device ids
	int deviceCount = Storage::read(STORAGE_CONFIG, STORAGE_ID_ALARM, sizeof(this->alarmIds), this->alarmIds);

	// load devices
	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		uint8_t id = this->alarmIds[i];

		// read data
		Data data;
		if (Storage::read(STORAGE_CONFIG, STORAGE_ID_ALARM | id, sizeof(data), &data) != sizeof(data))
			continue;

		// check id
		if (data.id != id)
			continue;

		// create alarm
		new Alarm(this, data);

		// device was correctly loaded
		this->alarmIds[j] = id;
		++j;
	}
	this->alarmCount = j;

	// start coroutines
	tick();
}

AlarmInterface::~AlarmInterface() {
}

String AlarmInterface::getName() {
	return "Alarm";
}

void AlarmInterface::setCommissioning(bool enabled) {
}

Array<uint8_t const> AlarmInterface::getDeviceIds() {
	return {this->alarmCount, this->alarmIds};
}

String AlarmInterface::getName(uint8_t id) const {
	auto alarm = findAlarm(id);
	if (alarm != nullptr)
		return String(alarm->data.name);
	return {};
}

void AlarmInterface::setName(uint8_t id, String name) {
	auto alarm = findAlarm(id);
	if (alarm != nullptr) {
		assign(alarm->data.name, name);

		// write to flash
		Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | alarm->data.id, sizeof(alarm->data), &alarm->data);
	}
}

Array<MessageType const> AlarmInterface::getPlugs(uint8_t id) const {
	auto alarm = findAlarm(id);
	if (alarm != nullptr)
		//return {alarm->data.plugCount, plugs};
		return plugs;
	return {};
}

void AlarmInterface::subscribe(Subscriber &subscriber) {
	subscriber.remove();
	auto id = subscriber.data->source.deviceId;
	auto alarm = findAlarm(id);
	if (alarm != nullptr)
		alarm->subscribers.add(subscriber);
}

SubscriberInfo AlarmInterface::getSubscriberInfo(uint8_t id, uint8_t plugIndex) {
	return {};
}

void AlarmInterface::erase(uint8_t id) {
	auto p = &this->alarms;
	while (*p != nullptr) {
		auto alarm = *p;
		if (alarm->data.id == id) {
			// remove alarm from linked list
			*p = alarm->next;

			// erase from flash
			Storage::erase(STORAGE_CONFIG, STORAGE_ID_ALARM | id);

			// delete alarm
			delete alarm;

			goto list;
		}
		p = &alarm->next;
	}

list:

	// erase from list of device id's
	this->alarmCount = eraseId(this->alarmCount, this->alarmIds, id);
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM, this->alarmCount, this->alarmIds);
}

AlarmInterface::Data const *AlarmInterface::get(uint8_t id) const {
	auto alarm = findAlarm(id);
	if (alarm != nullptr) {
		return &alarm->data;
	}
	return nullptr;
}

void AlarmInterface::set(uint8_t id, Data &data) {
	auto alarm = findAlarm(id);
	if (alarm != nullptr) {
		// set data
		alarm->data = data;

		// store alarm to flash
		Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | id, sizeof(alarm->data), &alarm->data);
		return;
	}

	data.id = allocateId();

	// add to list of alarms
	this->alarmIds[this->alarmCount++] = data.id;
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM, this->alarmCount, this->alarmIds);

	// create alarm
	alarm = new Alarm(this, data);

	// store data to flash
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | data.id, sizeof(alarm->data), &alarm->data);
}

int AlarmInterface::getSubscriberCount(uint8_t id, uint8_t command) {
	auto alarm = findAlarm(id);
	if (alarm != nullptr) {
		int count = 0;
		for (auto &subscriber : alarm->subscribers) {
			if (subscriber.data->source.plugIndex != 0)
				continue;

			// check if conversion for subscriber succeeds (command is not discarded by convertOptions)
			Message dst;
			if (convertSwitch(subscriber.info.type, dst, command, subscriber.data->convertOptions))
				++count;
		}
		return count;
	}
	return 0;
}

void AlarmInterface::test(uint8_t id, uint8_t command) {
	auto alarm = findAlarm(id);
	if (alarm != nullptr)
		alarm->subscribers.publishSwitch(0, command);
}

// protected:

AlarmInterface::Alarm *AlarmInterface::findAlarm(uint8_t id) const {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id)
			return alarm;
		alarm = alarm->next;
	}
	return nullptr;
}

uint8_t AlarmInterface::allocateId() {
	// find a free id
	int id;
	for (id = 1; id < 256; ++id) {
		auto alarm = this->alarms;
		while (alarm != nullptr) {
			if (alarm->data.id == id)
				goto found;
			alarm = alarm->next;
		}
		break;
	found:;
	}
	return id;
}

Coroutine AlarmInterface::tick() {
	while (true) {
		// measure
		co_await Calendar::secondTick();

		// check if an alarm goes off
		auto time = Calendar::now();
		if (time.getSeconds() != 0)
			continue;
		auto alarm = this->alarms;
		while (alarm != nullptr) {
			if (alarm->data.time.matches(time)) {
				// alarm goes off: publish to subscribers of this alarm
				alarm->subscribers.publishSwitch(0, 1);

				// set to active state
				alarm->active = true;
			} else if (alarm->active) {
				// todo: check if alarm is deactivated again
			}
			alarm = alarm->next;
		}
	}
}

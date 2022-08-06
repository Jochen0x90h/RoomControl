#include "AlarmInterface.hpp"
#include <Timer.hpp>
#include <Calendar.hpp>
#include <Output.hpp>
#include <Storage.hpp>
#include <util.hpp>
#include <appConfig.hpp>


static MessageType const plugs[8] = {
	MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT,
	MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT
};
static_assert(array::count(plugs) >= AlarmInterface::AlarmData::MAX_PLUG_COUNT);


AlarmInterface::AlarmInterface() {
	// load list of device ids
	int deviceCount = Storage::read(STORAGE_CONFIG, STORAGE_ID_ALARM, sizeof(this->alarmIds), this->alarmIds);

	// load devices
	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		uint8_t id = this->alarmIds[i];

		// load data
		AlarmData data;
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

void AlarmInterface::setCommissioning(bool enabled) {
}

Array<uint8_t const> AlarmInterface::getDeviceIds() {
	return {this->alarmCount, this->alarmIds};
}

String AlarmInterface::getName(uint8_t id) const {
	auto alarm = getAlarm(id);
	if (alarm != nullptr)
		return String(alarm->data.name);
	return {};
}

void AlarmInterface::setName(uint8_t id, String name) {
	auto alarm = getAlarm(id);
	if (alarm != nullptr) {
		assign(alarm->data.name, name);

		// write to flash
		Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | alarm->data.id, sizeof(alarm->data), &alarm->data);
	}
}

Array<MessageType const> AlarmInterface::getPlugs(uint8_t id) const {
	auto alarm = getAlarm(id);
	if (alarm != nullptr)
		return {alarm->data.plugCount, plugs};
	return {};
}

void AlarmInterface::subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	auto alarm = getAlarm(id);
	if (alarm != nullptr) {
		subscriber.source.device = {id, plugIndex};
		alarm->subscribers.add(subscriber);
	}
}

PublishInfo AlarmInterface::getPublishInfo(uint8_t id, uint8_t plugIndex) {
	return {};
}

void AlarmInterface::erase(uint8_t id) {
	auto a = &this->alarms;
	while (*a != nullptr) {
		auto alarm = *a;
		if (alarm->data.id == id) {
			// remove alarm from linked list
			*a = alarm->next;

			// erase from flash
			Storage::erase(STORAGE_CONFIG, STORAGE_ID_ALARM | id);

			// delete device
			delete alarm;

			goto list;
		}
		a = &alarm->next;
	}

list:

	// erase from list of device id's
	this->alarmCount = eraseId(this->alarmCount, this->alarmIds, id);
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM, this->alarmCount, this->alarmIds);
}

AlarmInterface::AlarmData const *AlarmInterface::get(uint8_t id) const {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id)
			return &alarm->data;
		alarm = alarm->next;
	}
	return nullptr;
}

void AlarmInterface::set(uint8_t id, AlarmData &data) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id) {
			// set data
			alarm->data = data;

			// store alarm to flash
			Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | id, sizeof(alarm->data), &alarm->data);
			return;
		}
	}

	data.id = allocateId();

	// add alarm to list of alarms
	this->alarmIds[this->alarmCount++] = data.id;
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM, this->alarmCount, this->alarmIds);

	// create alarm
	alarm = new Alarm(this, data);

	// store alarm to flash
	Storage::write(STORAGE_CONFIG, STORAGE_ID_ALARM | data.id, sizeof(alarm->data), &alarm->data);
}

int AlarmInterface::getSubscriberCount(uint8_t id, int endpointCount, uint8_t command) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id != id) {
			alarm = alarm->next;
			continue;
		}

		int count = 0;
		for (auto &subscriber : alarm->subscribers) {
			if (subscriber.source.device.plugIndex >= endpointCount)
				continue;

			Message dst;
			if (convertSwitch(subscriber.destination.type, dst, command, subscriber.convertOptions))
				++count;
		}
		return count;
	}
	return 0;
}

void AlarmInterface::test(uint8_t id, int plugCount, uint8_t command) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id != id) {
			alarm = alarm->next;
			continue;
		}

		for (auto &subscriber: alarm->subscribers) {
			if (subscriber.source.device.plugIndex >= plugCount)
				continue;

			// publish to subscriber
			subscriber.barrier->resumeFirst([&subscriber, command] (PublishInfo::Parameters &p) {
				p.info = subscriber.destination;

				// convert to target unit and type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(subscriber.destination.type, dst, command, subscriber.convertOptions);
			});
		}
		return;
	}
}

// protected:

AlarmInterface::Alarm *AlarmInterface::getAlarm(uint8_t id) const {
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
				// alarm goes off: publish to subscribers of alarm
				for (auto &subscriber: alarm->subscribers) {
					if (subscriber.source.device.plugIndex >= alarm->data.plugCount)
						break;

					// publish to subscriber
					subscriber.barrier->resumeFirst([&subscriber] (PublishInfo::Parameters &p) {
						p.info = subscriber.destination;

						// convert to target unit and type and resume coroutine if conversion was successful
						auto &dst = *reinterpret_cast<Message *>(p.message);
						uint8_t src = 1;
						return convertSwitch(subscriber.destination.type, dst, src, subscriber.convertOptions);
					});
				}
				alarm->active = true;
			} else if (alarm->active) {
				// todo: check if alarm is deactivated again
			}
			alarm = alarm->next;
		}
	}
}

#include "AlarmInterface.hpp"
#include <Timer.hpp>
#include <Calendar.hpp>
#include <Output.hpp>
#include <Storage2.hpp>
#include <util.hpp>
#include <appConfig.hpp>


static MessageType const endpoints[8] = {
	MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT,
	MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT
};
static_assert(array::count(endpoints) >= AlarmInterface::AlarmData::MAX_ENDPOINT_COUNT);


AlarmInterface::AlarmInterface() {
	// load list of device ids
	int deviceCount = Storage2::read(STORAGE_CONFIG, STORAGE_ID_ALARM, sizeof(this->alarmIds), this->alarmIds);

	// load devices
	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		uint8_t id = this->alarmIds[i];

		// load data
		AlarmData data;
		if (Storage2::read(STORAGE_CONFIG, STORAGE_ID_ALARM | id, sizeof(data), &data) != sizeof(data))
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

Interface::Device *AlarmInterface::getDevice(uint8_t id) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id)
			return alarm;
		alarm = alarm->next;
	}
	return nullptr;
}

void AlarmInterface::eraseDevice(uint8_t id) {
	auto d = &this->alarms;
	while (*d != nullptr) {
		auto device = *d;
		if (device->data.id == id) {
			// remove device from linked list
			*d = device->next;

			// erase from flash
			Storage2::erase(STORAGE_CONFIG, STORAGE_ID_ALARM | id);

			// delete device
			delete device;

			goto list;
		}
		d = &device->next;
	}

list:

	// erase from list of device id's
	int j = 0;
	for (int i = 0; i < this->alarmCount; ++i) {
		uint8_t id2 = this->alarmIds[i];
		if (id2 != id) {
			this->alarmIds[j] = id;
			++j;
		}
	}
	this->alarmCount = j;
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_ALARM, j, this->alarmIds);
}

AlarmInterface::AlarmData const *AlarmInterface::get(uint8_t id) const {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id)
			return &alarm->data;
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
			Storage2::write(STORAGE_CONFIG, STORAGE_ID_ALARM | id, sizeof(alarm->data), &alarm->data);
			return;
		}
	}

	data.id = allocateId();

	// add alarm to list of alarms
	this->alarmIds[this->alarmCount++] = data.id;
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_ALARM, this->alarmCount, this->alarmIds);

	// create alarm
	alarm = new Alarm(this, data);

	// store alarm to flash
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_ALARM | data.id, sizeof(alarm->data), &alarm->data);
}

int AlarmInterface::getSubscriberCount(uint8_t id, int endpointCount, uint8_t command) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id) {
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
	}
	return 0;
}

void AlarmInterface::test(uint8_t id, int plugCount, uint8_t command) {
	auto alarm = this->alarms;
	while (alarm != nullptr) {
		if (alarm->data.id == id) {
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
		}
	}
}

// AlarmInterface::Alarm

AlarmInterface::Alarm::~Alarm() {
}

uint8_t AlarmInterface::Alarm::getId() const {
	return this->data.id;
}

String AlarmInterface::Alarm::getName() const {
	return String(this->data.name);
}

void AlarmInterface::Alarm::setName(String name) {
	assign(this->data.name, name);

	// write to flash
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_ALARM | this->data.id, sizeof(this->data), &this->data);
}

Array<MessageType const> AlarmInterface::Alarm::getPlugs() const {
	return {this->data.plugCount, endpoints};
}

void AlarmInterface::Alarm::subscribe(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = endpointIndex;
	this->subscribers.add(subscriber);
}

PublishInfo AlarmInterface::Alarm::getPublishInfo(uint8_t endpointIndex) {
	return {};
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

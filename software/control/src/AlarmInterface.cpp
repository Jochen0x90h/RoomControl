#include "AlarmInterface.hpp"
#include <Timer.hpp>
#include <Calendar.hpp>
#include <Output.hpp>
#include <util.hpp>


static MessageType const endpoints[4] = {
	MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT, MessageType::BINARY_ALARM_OUT
};
static_assert(array::count(endpoints) >= AlarmInterface::AlarmFlash::MAX_ENDPOINT_COUNT);

/*
static bool convertMessage(MessageType dstType, void *dstMessage, MessageType srcType, Message const &src,
	ConvertOptions const &convertOptions)
{
	Message &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (srcType) {
	case MessageType::OFF_ON_IN:
	case MessageType::OFF_ON_TOGGLE_IN:
	case MessageType::TRIGGER_IN:
	case MessageType::UP_DOWN_IN:
	case MessageType::OPEN_CLOSE_IN:
		return convertCommandIn(dstType, dst, src.command, convertOptions);
	case MessageType::SET_LEVEL_IN:
	case MessageType::SET_AIR_TEMPERATURE_IN:
	case MessageType::SET_AIR_HUMIDITY_IN:
		return convertSetFloatValueIn(dstType, dst, srcType, src, convertOptions);
	default:
		// conversion failed
		return false;
	}
}
*/

AlarmInterface::AlarmInterface() {
	// set backpointers
	for (auto &alarm : this->alarms)
		alarm.interface = this;

	// start coroutines
	tick();
}

AlarmInterface::~AlarmInterface() {
}

void AlarmInterface::setCommissioning(bool enabled) {
}

int AlarmInterface::getDeviceCount() {
	return this->alarms.count();
}

Interface::Device &AlarmInterface::getDeviceByIndex(int index) {
	assert(index >= 0 && index < this->alarms.count());
	return this->alarms[index];
}

Interface::Device *AlarmInterface::getDevice(uint8_t id) {
	for (auto &alarm : this->alarms) {
		if (alarm->interfaceId == id)
			return &alarm;
	}
	return nullptr;
}

void AlarmInterface::eraseDevice(uint8_t id) {
}

AlarmInterface::AlarmFlash const &AlarmInterface::get(int index) const {
	assert(index >= 0 && index < this->alarms.count());
	return *this->alarms[index];
}

void AlarmInterface::set(int index, AlarmFlash &flash) {
	assert(index >= 0 && index <= this->alarms.count());
	if (index < this->alarms.count()) {
		this->alarms.write(index, flash);
	} else {
		Alarm* alarm = new Alarm(flash);
		alarm->interface = this;

		// find a free id
		flash.interfaceId = allocateInterfaceId(this->alarms);

		this->alarms.write(index, alarm);
	}
}

int AlarmInterface::getSubscriberCount(int index, int endpointCount, uint8_t command) {
	assert(index >= 0 && index < this->alarms.count());
	int count = 0;
	for (auto &subscriber : this->alarms[index].subscribers) {
		if (subscriber.source.device.plugIndex >= endpointCount)
			continue;

		Message dst;
		if (convertSwitch(subscriber.destination.type, dst, command, subscriber.convertOptions))
			++count;
	}
	return count;
}

void AlarmInterface::test(int index, int endpointCount, uint8_t command) {
	assert(index >= 0 && index < this->alarms.count());
	auto &alarm = this->alarms[index];

	for (auto &subscriber: alarm.subscribers) {
		if (subscriber.source.device.plugIndex >= endpointCount)
			continue;

		// publish to subscriber
		subscriber.barrier->resumeFirst([&subscriber, command] (PublishInfo::Parameters &p) {
			p.info = subscriber.destination;

			// convert to target unit and type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			return convertSwitch(subscriber.destination.type, dst, command, subscriber.convertOptions);
			//MessageType srcType = flash.endpoints[subscriber.index];
			//auto &src = flash.messages[subscriber.index];
			//return convertMessage(subscriber.messageType, p.message, srcType, src, subscriber.convertOptions);
		});
	}
}

Coroutine AlarmInterface::tick() {
	while (true) {
		// measure
		co_await Calendar::secondTick();

		// check if an alarm goes off
		auto time = Calendar::now();
		if (time.getSeconds() != 0)
			continue;
		for (Alarm &alarm : this->alarms) {
			if (alarm->time.matches(time)) {
				// alarm goes off: publish to subscribers of alarm
				for (auto &subscriber: alarm.subscribers) {
					auto const &flash = *alarm;
					if (subscriber.source.device.plugIndex >= flash.endpointCount)
						break;

					// publish to subscriber
					subscriber.barrier->resumeFirst([&subscriber, &flash] (PublishInfo::Parameters &p) {
						p.info = subscriber.destination;

						// convert to target unit and type and resume coroutine if conversion was successful
						auto &dst = *reinterpret_cast<Message *>(p.message);
						uint8_t src = 1;
						return convertSwitch(subscriber.destination.type, dst, src, subscriber.convertOptions);
						//MessageType type = flash.endpoints[subscriber.index];
						//auto &message = flash.messages[subscriber.index];
						//return convertMessage(subscriber.messageType, p.message, type, message,
						//	subscriber.convertOptions);
					});
				}
			}
		}
	}
}

// AlarmInterface::DeviceFlash

AlarmInterface::AlarmFlash::AlarmFlash(AlarmFlash const &flash) :
	interfaceId(flash.interfaceId), time(flash.time), endpointCount(flash.endpointCount)
{
}

int AlarmInterface::AlarmFlash::size() const {
	return sizeof(AlarmFlash);
	//return getOffset(AlarmFlash, messages[this->endpointCount]);
}

AlarmInterface::Alarm *AlarmInterface::AlarmFlash::allocate() const {
	return new Alarm(*this);
}


// AlarmInterface::Alarm

AlarmInterface::Alarm::~Alarm() {
}

uint8_t AlarmInterface::Alarm::getId() const {
	auto &flash = **this;
	return flash.interfaceId;
}

String AlarmInterface::Alarm::getName() const {
	return "x";
}

void AlarmInterface::Alarm::setName(String name) {

}

Array<MessageType const> AlarmInterface::Alarm::getPlugs() const {
	auto &flash = **this;
	return {flash.endpointCount, endpoints};
}

void AlarmInterface::Alarm::subscribe(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = endpointIndex;
	this->subscribers.add(subscriber);
}

PublishInfo AlarmInterface::Alarm::getPublishInfo(uint8_t endpointIndex) {
	return {};
}

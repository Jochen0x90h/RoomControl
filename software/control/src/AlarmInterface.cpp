#include "AlarmInterface.hpp"
#include <Timer.hpp>
#include <Calendar.hpp>
#include <Output.hpp>
#include <util.hpp>


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
			return convertCommandIn(dstType, dst, srcType, src.command, convertOptions);
		case MessageType::TEMPERATURE_IN:
			return convertTemperatureIn(dstType, dst, src.temperature, convertOptions);
		default:
			// conversion failed
			return false;
	}
}


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

Interface::Device *AlarmInterface::getDeviceById(uint8_t id) {
	for (auto &alarm : this->alarms) {
		if (alarm->interfaceId == id)
			return &alarm;
	}
	return nullptr;
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

int AlarmInterface::getSubscriberCount(int index) {
	assert(index >= 0 && index < this->alarms.count());
	return this->alarms[index].subscribers.count();
}

void AlarmInterface::test(int index, AlarmFlash const &flash) {
	assert(index >= 0 && index < this->alarms.count());
	auto &alarm = this->alarms[index];

	for (auto &subscriber: alarm.subscribers) {
		if (subscriber.index >= flash.endpointCount)
			continue;

		// publish to subscriber
		subscriber.barrier->resumeFirst([&subscriber, &flash] (Subscriber::Parameters &p) {
			p.subscriptionIndex = subscriber.subscriptionIndex;

			// convert to target unit and type and resume coroutine if conversion was successful
			MessageType srcType = flash.endpoints[subscriber.index];
			auto &message = flash.messages[subscriber.index];
			return convertMessage(subscriber.messageType, p.message, srcType, message, subscriber.convertOptions);
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
					if (subscriber.index >= flash.endpointCount)
						break;

					// publish to subscriber
					subscriber.barrier->resumeFirst([&subscriber, &flash] (Subscriber::Parameters &p) {
						p.subscriptionIndex = subscriber.subscriptionIndex;

						// convert to target unit and type and resume coroutine if conversion was successful
						//MessageType type = flash.messageTypes[subscriber.index];
						MessageType type = flash.endpoints[subscriber.index];
						auto &message = flash.messages[subscriber.index];
						return convertMessage(subscriber.messageType, p.message, type, message,
							subscriber.convertOptions);
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
	array::copy(this->endpoints, flash.endpoints);
	for (int i = 0; i < flash.endpointCount; ++i) {
		array::copy(this->messages[i].data, flash.messages[i].data);
	}
}

int AlarmInterface::AlarmFlash::size() const {
	return getOffset(AlarmFlash, messages[this->endpointCount]);
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

Array<MessageType const> AlarmInterface::Alarm::getEndpoints() const {
	auto &flash = **this;
	return {flash.endpointCount, flash.endpoints};
}

void AlarmInterface::Alarm::addPublisher(uint8_t endpointIndex, Publisher &publisher) {
}

void AlarmInterface::Alarm::addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.index = endpointIndex;
	this->subscribers.add(subscriber);
}

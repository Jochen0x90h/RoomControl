#include "LocalInterface.hpp"
#include <Timer.hpp>
#include <Output.hpp>
#include <util.hpp>


// device names
constexpr String deviceNames[] = {
	"Air Sensor",
	"Heating",
	"Brightness Sensor",
	"Motion Detector",
	"Digital Inputs",
	"Digital Outputs"
};

// air sensor endpoints
constexpr int BME680_TEMPERATURE_ENDPOINT = 0;
constexpr int BME680_HUMIDITY_ENDPOINT = 1;
constexpr int BME680_PRESSURE_ENDPOINT = 2;
constexpr int BME680_VOC_ENDPOINT = 3;

// endpoint type arrays for getEndpoints()
constexpr MessageType bme680Endpoints[] = {
	MessageType::AIR_TEMPERATURE_IN,
	MessageType::AIR_HUMIDITY_IN,
	MessageType::AIR_PRESSURE_IN,
	MessageType::AIR_VOC_IN
};

constexpr MessageType heatingEndpoints[] = {
	MessageType::OFF_ON_TOGGLE_OUT
};

constexpr MessageType brightnessSensorEndpoints[] = {
	MessageType::ILLUMINANCE_IN,
};

constexpr MessageType motionDetectorEndpoints[] = {
	MessageType::TRIGGER_IN,
};

constexpr MessageType inEndpoints[] = {
	MessageType::OFF_ON_IN, MessageType::OFF_ON_IN, MessageType::OFF_ON_IN, MessageType::OFF_ON_IN
};

constexpr MessageType outEndpoints[] = {
	MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT
};



LocalInterface::LocalInterface() {
	int i = 0;
	this->devices[i++].interfaceId = BME680_ID;
	this->devices[i++].interfaceId = HEATING_ID;
	this->devices[i++].interfaceId = BRIGHTNESS_SENSOR_ID;
	this->devices[i++].interfaceId = MOTION_DETECTOR_ID;
	if (INPUT_EXT_COUNT > 0)
		this->devices[i++].interfaceId = IN_ID;
	if (OUTPUT_EXT_COUNT > 0)
		this->devices[i++].interfaceId = OUT_ID;
	this->deviceCount = i;

	// set backpointers
	for (auto &device : this->devices)
		device.interface = this;

	// start coroutines
	readAirSensor();
	publish();
}

LocalInterface::~LocalInterface() {
}

void LocalInterface::setCommissioning(bool enabled) {
}

int LocalInterface::getDeviceCount() {
	return this->deviceCount;
}

Interface::Device &LocalInterface::getDeviceByIndex(int index) {
	assert(index >= 0 && index < this->deviceCount);
	return this->devices[index];
}

Interface::Device *LocalInterface::getDeviceById(uint8_t id) {
	for (auto &device : Array<LocalDevice>(this->deviceCount, this->devices)) {
		if (device.interfaceId == id)
			return &device;
	}
	return nullptr;
}

Coroutine LocalInterface::readAirSensor() {
	BME680 airSensor;

	// initialize the air sensor
	co_await airSensor.init();
	co_await airSensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100); // heater temperature (celsius) and duration (ms)

	// regularly read the air sensor
	auto &device = this->devices[BME680_ID - 1];
	while (true) {
		// measure
		co_await airSensor.measure();

		// publish to subscribers of air sensor
		for (auto &subscriber : device.subscribers) {
			if (subscriber.index >= array::count(bme680Endpoints))
				break;

			// get value
			float value;
			switch (subscriber.index) {
			case BME680_TEMPERATURE_ENDPOINT:
				// get temperature in celsius
				value = airSensor.getTemperature() + 273.15f;
				break;
			case BME680_HUMIDITY_ENDPOINT:
				value = airSensor.getHumidity();
				break;
			case BME680_PRESSURE_ENDPOINT:
				// get pressure in hectopascal
				value = airSensor.getPressure();
				break;
			case BME680_VOC_ENDPOINT:
				// get gas resistance in ohm
				value = airSensor.getGasResistance();
				break;
			}

			// publish to subscriber
			subscriber.barrier->resumeFirst([&subscriber, value] (Subscriber::Parameters &p) {
				p.source = subscriber.source;

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				MessageType srcType = bme680Endpoints[subscriber.index];
				return convertFloatValueIn(subscriber.messageType, dst, srcType, value, subscriber.convertOptions);
			});
		}
		
		// wait
		#ifdef DEBUG
			co_await Timer::sleep(10s);
		#else
			co_await timer::sleep(60s);
		#endif
	}
}

Coroutine LocalInterface::publish() {
	while (true) {
		// wait until a publisher wants to publish a message to an endpoint of a local device
		co_await this->publishEvent.wait();
		this->publishEvent.clear();
		
		// iterate over devices
		for (int i = 0; i < this->deviceCount; ++i) {
			auto &device = this->devices[i];
		
			// iterate over publishers
			for (auto &publisher : device.publishers) {
				// check if publisher wants to publish
				if (publisher.dirty) {
					publisher.dirty = false;
					uint8_t endpointIndex = publisher.index;
					auto &src = *reinterpret_cast<Message const *>(publisher.message);

					// set to device
					switch (device.interfaceId) {
					case HEATING_ID:
						{
							// convert to on/off
							uint8_t message;
							if (convertCommandOut(message, publisher.messageType, src, publisher.convertOptions)) {
								// set output
								if (message <= 1)
									Output::set(OUTPUT_HEATING, message);
								else
									Output::toggle(OUTPUT_HEATING);
							}
						}
						break;
					case OUT_ID:
						{
							// convert to on/off
							uint8_t message;
							if (convertCommandOut(message, publisher.messageType, src, publisher.convertOptions)) {
								// set output
								if (message <= 1)
									Output::set(OUTPUT_EXT_INDEX + endpointIndex, message);
								else
									Output::toggle(OUTPUT_EXT_INDEX + endpointIndex);
							}
						}
						break;
					}
/*
					// forward to subscribers
					for (auto &subscriber : device.subscribers) {
						if (subscriber.index == endpointIndex) {
							subscriber.barrier->resumeAll([&subscriber, &publisher] (Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;
								
								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}*/
				}
			}
		}
	}
}


// LocalInterface::LocalDevice

uint8_t LocalInterface::LocalDevice::getId() const {
	return this->interfaceId;
}

String LocalInterface::LocalDevice::getName() const {
	int i = int(this->interfaceId) - 1;
	return deviceNames[i];
}

void LocalInterface::LocalDevice::setName(String name) {

}

Array<MessageType const> LocalInterface::LocalDevice::getEndpoints() const {
	switch (this->interfaceId) {
		case BME680_ID:
			return bme680Endpoints;
		case HEATING_ID:
			return heatingEndpoints;
		case BRIGHTNESS_SENSOR_ID:
			return brightnessSensorEndpoints;
		case MOTION_DETECTOR_ID:
			return motionDetectorEndpoints;
		case IN_ID:
			return {INPUT_EXT_COUNT, inEndpoints};
		case OUT_ID:
			return {OUTPUT_EXT_COUNT, outEndpoints};
	}
	return {};
}

void LocalInterface::LocalDevice::addPublisher(uint8_t endpointIndex, Publisher &publisher) {
	publisher.remove();
	publisher.index = endpointIndex;
	publisher.event = &this->interface->publishEvent;
	this->publishers.add(publisher);
}

void LocalInterface::LocalDevice::addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.index = endpointIndex;
	this->subscribers.add(subscriber);
}

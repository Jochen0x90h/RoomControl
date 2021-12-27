#include "LocalInterface.hpp"
#include <timer.hpp>
#include <out.hpp>
#include <util.hpp>



// air sensor endpoints
constexpr int BME680_TEMPERATURE_ENDPOINT = 0;
constexpr int BME680_PRESSURE_ENDPOINT = 1;
constexpr int BME680_HUMIDITY_ENDPOINT = 2;
constexpr int BME680_VOC_ENDPOINT = 3;

// endpoint type arrays for getEndpoints()
constexpr EndpointType bme680Endpoints[] = {
	EndpointType::TEMPERATURE_IN,
	EndpointType::AIR_PRESSURE_IN,
	EndpointType::AIR_HUMIDITY_IN,
	EndpointType::AIR_VOC_IN
};
constexpr MessageType bme680MessageTypes[] = {
	MessageType::CELSIUS, MessageType::HECTOPASCAL, MessageType::HECTOPASCAL, MessageType::OHM
};

constexpr EndpointType brightnessSensorEndpoints[] = {
	EndpointType::ILLUMINANCE_IN,
};

constexpr EndpointType motionDetectorEndpoints[] = {
	EndpointType::TRIGGER_IN,
};

constexpr EndpointType inEndpoints[] = {
	EndpointType::ON_OFF_IN, EndpointType::ON_OFF_IN, EndpointType::ON_OFF_IN, EndpointType::ON_OFF_IN
};

constexpr EndpointType outEndpoints[] = {
	EndpointType::ON_OFF_OUT, EndpointType::ON_OFF_OUT, EndpointType::ON_OFF_OUT, EndpointType::ON_OFF_OUT
};



LocalInterface::LocalInterface() {
	uint8_t i = 0;
	this->devices[i++].id = BME680_ID;
	this->devices[i++].id = BRIGHTNESS_SENSOR_ID;
	this->devices[i++].id = MOTION_DETECTOR_ID;
	if (IN_COUNT > 0)
		this->devices[i++].id = IN_ID;
	if (OUT_COUNT > 0)
		this->devices[i++].id = OUT_ID;
	this->deviceCount = i;

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

DeviceId LocalInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->deviceCount);
	return this->devices[index].id;
}

Array<EndpointType const> LocalInterface::getEndpoints(DeviceId deviceId) {
	switch (deviceId) {
	case BME680_ID:
		return bme680Endpoints;
	case BRIGHTNESS_SENSOR_ID:
		return brightnessSensorEndpoints;
	case MOTION_DETECTOR_ID:
		return motionDetectorEndpoints;
	case IN_ID:
		return Array(IN_COUNT, inEndpoints);
	case OUT_ID:
		return Array(OUT_COUNT, outEndpoints);
	}
	return {};
}

void LocalInterface::addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) {
	for (int i = 0; i < this->deviceCount; ++i) {
		auto &device = this->devices[i];
		if (device.id == deviceId && endpointIndex < getEndpoints(deviceId).length) {
			publisher.remove();
			publisher.index = endpointIndex;
			publisher.event = &this->publishEvent;
			device.publishers.add(publisher);
		}
	}
}

void LocalInterface::addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) {
	for (int i = 0; i < this->deviceCount; ++i) {
		auto &device = this->devices[i];
		if (device.id == deviceId && endpointIndex < getEndpoints(deviceId).length) {
			subscriber.remove();
			subscriber.index = endpointIndex;
			device.subscribers.add(subscriber);
		}
	}
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
	Device &device = this->devices[BME680_ID - 1];
	while (true) {
		// measure
		co_await airSensor.measure();

		// publish to subscribers of air sensor
		//for (auto &subscription : device.subscriptions) {
		for (auto &subscriber : device.subscribers) {
			// get value
			FloatWithFlag value;
			switch (subscriber.index) {
			case BME680_TEMPERATURE_ENDPOINT:
				// get temperature in celsius
				value = airSensor.getTemperature();
				break;
			case BME680_PRESSURE_ENDPOINT:
				// get pressure in hectopascal
				value = airSensor.getPressure();
				break;
			case BME680_HUMIDITY_ENDPOINT:
				value = airSensor.getHumidity();
				break;
			case BME680_VOC_ENDPOINT:
				// get gas resistance in ohm
				value = airSensor.getGasResistance();
				break;
			}
			
			// publish to subscriber
			subscriber.barrier->resumeFirst([&subscriber, value] (Subscriber::Parameters &p) {
				p.subscriptionIndex = subscriber.subscriptionIndex;
				
				// convert to target unit and type and resume coroutine if conversion was successful
				MessageType type = bme680MessageTypes[subscriber.index];
				return convert(subscriber.messageType, p.message, type, &value);
			});
		}
		
		// wait
		#ifdef DEBUG
			co_await timer::sleep(10s);
		#else
			co_await timer::sleep(60s);
		#endif
	}
}

Coroutine LocalInterface::publish() {
	while (true) {
		// wait until something was published
		co_await this->publishEvent.wait();

		// clear immediately as we have only one instance of this coroutine
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

					// set to device
					switch (device.id) {
					case OUT_ID:
						{
							// convert to on/off
							uint8_t message;
							if (convert(MessageType::ON_OFF, &message, publisher.messageType, publisher.message)) {
								// set output
								if (message <= 1)
									out::set(endpointIndex, message);
								else
									out::toggle(endpointIndex);
							}
						}
					}

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
					}
				}
			}
		}
	}
}

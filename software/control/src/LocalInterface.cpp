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
constexpr MessageType2 bme680Endpoints[] = {
	MessageType2::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT,//AIR_TEMPERATURE_OUT,
	MessageType2::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR_OUT, //AIR_HUMIDITY_OUT,
	MessageType2::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE_OUT,//AIR_PRESSURE_OUT,
	MessageType2::CONCENTRATION_VOC_OUT,//AIR_VOC_OUT
};

constexpr MessageType2 heatingEndpoints[] = {
	MessageType2::SWITCH_BINARY_ON_OFF_CMD_OUT//OFF_ON_TOGGLE_IN
};

constexpr MessageType2 brightnessSensorEndpoints[] = {
	MessageType2::PHYSICAL_ILLUMINATION_OUT//ILLUMINANCE_OUT,
};

constexpr MessageType2 motionDetectorEndpoints[] = {
	MessageType2::SWITCH_BINARY_OCCUPANCY_OUT//TRIGGER_OUT,
};

// binary inputs (have out endpoints)
constexpr MessageType2 inEndpoints[] = {
	//MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT, MessageType::OFF_ON_OUT
	MessageType2::SWITCH_BINARY_OUT, MessageType2::SWITCH_BINARY_OUT, MessageType2::SWITCH_BINARY_OUT, MessageType2::SWITCH_BINARY_OUT
};

// binary outputs (have in enpoitns)
constexpr MessageType2 outEndpoints[] = {
	//MessageType::OFF_ON_IN, MessageType::OFF_ON_IN, MessageType::OFF_ON_IN, MessageType::OFF_ON_IN
	MessageType2::SWITCH_BINARY_CMD_IN, MessageType2::SWITCH_BINARY_CMD_IN, MessageType2::SWITCH_BINARY_CMD_IN, MessageType2::SWITCH_BINARY_CMD_IN
};


LocalInterface::LocalInterface() {
	int i = 0;
	this->devices[i++].init(this, BME680_ID, bme680Endpoints);
	this->devices[i++].init(this, HEATING_ID, heatingEndpoints);
	this->devices[i++].init(this, BRIGHTNESS_SENSOR_ID, brightnessSensorEndpoints);
	this->devices[i++].init(this, MOTION_DETECTOR_ID, motionDetectorEndpoints);
	if (INPUT_EXT_COUNT > 0)
		this->devices[i++].init(this, IN_ID, {INPUT_EXT_COUNT, inEndpoints});
	if (OUTPUT_EXT_COUNT > 0)
		this->devices[i++].init(this, OUT_ID, {OUTPUT_EXT_COUNT, outEndpoints});
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
			if (subscriber.source.device.endpointIndex >= array::count(bme680Endpoints))
				break;

			// get source message (measured value)
			float src;
			switch (subscriber.source.device.endpointIndex) {
			case BME680_TEMPERATURE_ENDPOINT:
				// get temperature in celsius
				src = airSensor.getTemperature() + 273.15f;
				break;
			case BME680_HUMIDITY_ENDPOINT:
				src = airSensor.getHumidity();
				break;
			case BME680_PRESSURE_ENDPOINT:
				// get pressure in hectopascal
				src = airSensor.getPressure();
				break;
			case BME680_VOC_ENDPOINT:
				// get gas resistance in ohm
				src = airSensor.getGasResistance();
				break;
			}

			// publish to subscriber
			subscriber.barrier->resumeFirst([&subscriber, src] (PublishInfo::Parameters &p) {
				p.info = subscriber.destination;

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message2 *>(p.message);
				auto srcType = bme680Endpoints[subscriber.source.device.endpointIndex];
				return convertFloat(subscriber.destination.type, dst, src, subscriber.convertOptions);
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
		// wait for message
		MessageInfo2 info;
		Message2 message;
		co_await this->publishBarrier.wait(info, &message);

		// set to device
		switch (info.device.id) {
		case HEATING_ID:
			// set output
			if (message.command <= 1)
				Output::set(OUTPUT_HEATING, message.command != 0);
			else
				Output::toggle(OUTPUT_HEATING);
			break;
		case OUT_ID:
			// set output
			if (message.command <= 1)
				Output::set(OUTPUT_EXT_INDEX + info.device.endpointIndex, message.command != 0);
			else
				Output::toggle(OUTPUT_EXT_INDEX + info.device.endpointIndex);
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

Array<MessageType2 const> LocalInterface::LocalDevice::getEndpoints() const {
	return this->endpoints;
}

void LocalInterface::LocalDevice::subscribe(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.endpointIndex = endpointIndex;
	this->subscribers.add(subscriber);
}

PublishInfo LocalInterface::LocalDevice::getPublishInfo(uint8_t endpointIndex) {
	if (endpointIndex >= this->endpoints.count())
		return {};
	return {{.type = this->endpoints[endpointIndex], .device = {this->interfaceId, endpointIndex}},
		&this->interface->publishBarrier};
}

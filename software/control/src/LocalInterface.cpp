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
	MessageType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT,
	MessageType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR_OUT,
	MessageType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE_OUT,
	MessageType::CONCENTRATION_VOC_OUT,
};

constexpr MessageType heatingEndpoints[] = {
	MessageType::BINARY_OPEN_VALVE_CMD_OUT
};

constexpr MessageType brightnessSensorEndpoints[] = {
	MessageType::PHYSICAL_ILLUMINANCE_OUT,
};

constexpr MessageType motionDetectorEndpoints[] = {
	MessageType::BINARY_OCCUPANCY_OUT,
};

// binary inputs (have out endpoints)
constexpr MessageType inEndpoints[] = {
	MessageType::BINARY_OUT, MessageType::BINARY_OUT, MessageType::BINARY_OUT, MessageType::BINARY_OUT
};

// binary outputs (have in enpoitns)
constexpr MessageType outEndpoints[] = {
	MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN
};


LocalInterface::LocalInterface() {
	this->devices[BME680_ID - 1].init(this, BME680_ID, bme680Endpoints);
	this->devices[HEATING_ID - 1].init(this, HEATING_ID, heatingEndpoints);
	this->devices[BRIGHTNESS_SENSOR_ID - 1].init(this, BRIGHTNESS_SENSOR_ID, brightnessSensorEndpoints);
	this->devices[MOTION_DETECTOR_ID - 1].init(this, MOTION_DETECTOR_ID, motionDetectorEndpoints);
	this->devices[IN_ID - 1].init(this, IN_ID, {INPUT_EXT_COUNT, inEndpoints});
	this->devices[OUT_ID - 1].init(this, OUT_ID, {OUTPUT_EXT_COUNT, outEndpoints});

	int i = 0;
	for (auto &device : this->devices) {
		if (!device.plugs.isEmpty())
			this->deviceIds[i++] = device.id;
	}
	this->deviceCount = i;

	// start coroutines
	readAirSensor();
	publish();
}

LocalInterface::~LocalInterface() {
}

void LocalInterface::setCommissioning(bool enabled) {
}

Array<uint8_t const> LocalInterface::getDeviceIds() {
	return {this->deviceCount, this->deviceIds};
}

Interface::Device *LocalInterface::getDevice(uint8_t id) {
	if (id >= 1 && id <= DEVICE_COUNT)
		return &this->devices[id - 1];
	return nullptr;
}

void LocalInterface::eraseDevice(uint8_t id) {
	// not possible to erase local devices
}

// LocalInterface::LocalDevice

uint8_t LocalInterface::LocalDevice::getId() const {
	return this->id;
}

String LocalInterface::LocalDevice::getName() const {
	int i = int(this->id) - 1;
	return deviceNames[i];
}

void LocalInterface::LocalDevice::setName(String name) {
}

Array<MessageType const> LocalInterface::LocalDevice::getPlugs() const {
	return this->plugs;
}

void LocalInterface::LocalDevice::subscribe(uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = plugIndex;
	this->subscribers.add(subscriber);
}

PublishInfo LocalInterface::LocalDevice::getPublishInfo(uint8_t plugIndex) {
	if (plugIndex >= this->plugs.count())
		return {};
	return {{.type = this->plugs[plugIndex], .device = {this->id, plugIndex}},
		&this->interface->publishBarrier};
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
			if (subscriber.source.device.plugIndex >= array::count(bme680Endpoints))
				break;

			// get source message (measured value)
			float src;
			switch (subscriber.source.device.plugIndex) {
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
				auto &dst = *reinterpret_cast<Message *>(p.message);
				auto srcType = bme680Endpoints[subscriber.source.device.plugIndex];
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
		MessageInfo info;
		Message message;
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
				Output::set(OUTPUT_EXT_INDEX + info.device.plugIndex, message.command != 0);
			else
				Output::toggle(OUTPUT_EXT_INDEX + info.device.plugIndex);
			break;
		}

		/*
		// forward to subscribers
		for (auto &subscriber : device.subscribers) {
			if (subscriber.index == plugIndex) {
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


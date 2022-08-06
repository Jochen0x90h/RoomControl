#include "LocalInterface.hpp"
#include <Timer.hpp>
#include <Output.hpp>
#include <Sound.hpp>
#include <util.hpp>


// device names
constexpr String deviceNames[] = {
	"Air Sensor",
	"Digital Inputs",
	"Digital Outputs",
	"Heating",
	"Audio",
	"Brightness Sensor",
	"Motion Detector",
};

// air sensor endpoints
constexpr int BME680_TEMPERATURE_ENDPOINT = 0;
constexpr int BME680_HUMIDITY_ENDPOINT = 1;
constexpr int BME680_PRESSURE_ENDPOINT = 2;
constexpr int BME680_VOC_ENDPOINT = 3;

// air sensor plugs
constexpr MessageType bme680Plugs[] = {
	MessageType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT,
	MessageType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR_OUT,
	MessageType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE_OUT,
	MessageType::CONCENTRATION_VOC_OUT,
};

// binary inputs (max 4)
constexpr MessageType inPlugs[] = {
	MessageType::BINARY_OUT, MessageType::BINARY_OUT, MessageType::BINARY_OUT, MessageType::BINARY_OUT
};

// binary outputs (max 4)
constexpr MessageType outPlugs[] = {
	MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN, MessageType::BINARY_CMD_IN
};

// heating plugs (max 2)
constexpr MessageType heatingPlugs[] = {
	MessageType::BINARY_OPEN_VALVE_CMD_OUT, MessageType::BINARY_OPEN_VALVE_CMD_OUT
};

// audio plugs (max 32)
constexpr MessageType audioPlugs[] = {
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
	MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN, MessageType::BINARY_IN,
};

// brightness sensor plugs
constexpr MessageType brightnessSensorPlugs[] = {
	MessageType::PHYSICAL_ILLUMINANCE_OUT,
};

// motion detector plugs
constexpr MessageType motionDetectorPlugs[] = {
	MessageType::BINARY_OCCUPANCY_OUT,
};



LocalInterface::LocalInterface() {
	int audioCount = Sound::count();
	this->audioCount = audioCount;
	//this->audioPlug = MessageType::ENUM_CMD_IN | MessageType(audioCount + 1);

	int i = 0;
	this->deviceIds[i++] = BME680_ID;
	if (INPUT_EXT_COUNT > 0)
		this->deviceIds[i++] = IN_ID;
	if (OUTPUT_EXT_COUNT > 0)
		this->deviceIds[i++] = OUT_ID;
	if (OUTPUT_HEATING_COUNT > 0)
		this->deviceIds[i++] = HEATING_ID;
	if (audioCount > 0)
		this->deviceIds[i++] = AUDIO_ID;
	this->deviceIds[i++] = BRIGHTNESS_SENSOR_ID;
	this->deviceIds[i++] = MOTION_DETECTOR_ID;
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

String LocalInterface::getName(uint8_t id) const {
	if (id >= 1 && id <= DEVICE_COUNT)
		return deviceNames[id - 1];
	return {};
}

void LocalInterface::setName(uint8_t id, String name) {
}

Array<MessageType const> LocalInterface::getPlugs(uint8_t id) const {
	switch (id) {
	case BME680_ID:
		return bme680Plugs;
	case IN_ID:
		return {INPUT_EXT_COUNT, inPlugs};
	case OUT_ID:
		return {OUTPUT_EXT_COUNT, outPlugs};
	case HEATING_ID:
		return {OUTPUT_HEATING_COUNT, heatingPlugs};
	case AUDIO_ID:
		return {this->audioCount, audioPlugs};
		//return {1, &this->audioPlug};
	case BRIGHTNESS_SENSOR_ID:
		return brightnessSensorPlugs;
	case MOTION_DETECTOR_ID:
		return motionDetectorPlugs;
	default:
		return {};
	}
}

void LocalInterface::subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	auto plugs = getPlugs(id);
	if (plugIndex < plugs.count()) {
		subscriber.source.device = {id, plugIndex};
		this->devices[id - 1].subscribers.add(subscriber);
	}
}

PublishInfo LocalInterface::getPublishInfo(uint8_t id, uint8_t plugIndex) {
	auto plugs = getPlugs(id);
	if (plugIndex < plugs.count())
		return {{.type = plugs[plugIndex], .device = {id, plugIndex}}, &this->publishBarrier};
	return {};
}

void LocalInterface::erase(uint8_t id) {
	// not possible to erase local devices
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
			if (subscriber.source.device.plugIndex >= array::count(bme680Plugs))
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
				auto srcType = bme680Plugs[subscriber.source.device.plugIndex];
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
		case OUT_ID:
			// set "ext" output
			if (info.device.plugIndex < OUTPUT_EXT_COUNT) {
				if (message.value.u8 <= 1)
					Output::set(OUTPUT_EXT_INDEX + info.device.plugIndex, message.value.u8 != 0);
				else
					Output::toggle(OUTPUT_EXT_INDEX + info.device.plugIndex);
			}
			break;
		case HEATING_ID:
			// set heating output
			if (info.device.plugIndex < OUTPUT_HEATING_COUNT) {
				if (message.value.u8 <= 1)
					Output::set(OUTPUT_HEATING_INDEX, message.value.u8 != 0);
				else
					Output::toggle(OUTPUT_HEATING_INDEX);
			}
			break;
		case AUDIO_ID:
			// start audio
			if (info.device.plugIndex < this->audioCount) {
				if (message.value.u8 != 0)
					Sound::play(info.device.plugIndex);
				else
					Sound::stop(info.device.plugIndex);
			}
			/*
			if (info.device.plugIndex == 0) {
				if (message.value.u16 == 0)
					Audio::stop();
				else
					Audio::play(message.value.u16 - 1);
			}*/
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


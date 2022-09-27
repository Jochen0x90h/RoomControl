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
	"Sound",
	"Brightness Sensor",
	"Motion Detector",
};

// air sensor plugs
constexpr int BME680_TEMPERATURE_PLUG = 0;
constexpr int BME680_HUMIDITY_PLUG = 1;
constexpr int BME680_PRESSURE_PLUG = 2;
constexpr int BME680_VOC_PLUG = 3;

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

// brightness sensor plugs
constexpr MessageType brightnessSensorPlugs[] = {
	MessageType::PHYSICAL_ILLUMINANCE_OUT,
};

// motion detector plugs
constexpr MessageType motionDetectorPlugs[] = {
	MessageType::BINARY_OCCUPANCY_OUT,
};


LocalInterface::LocalInterface()
	: devices{
		{BME680_ID, this->listeners},
		{IN_ID, this->listeners},
		{OUT_ID, this->listeners},
		{HEATING_ID, this->listeners},
		{SOUND_ID, this->listeners},
		{BRIGHTNESS_SENSOR_ID, this->listeners},
		{MOTION_DETECTOR_ID, this->listeners},
	}
{
	// convert available sounds to plugs of sound device
	auto types = Sound::getTypes();
	this->soundCount = min(types.count(), array::count(this->soundPlugs));
	for (int i = 0; i < this->soundCount; ++i) {
		auto &plug = this->soundPlugs[i];
		switch (types[i]) {
		case Sound::Type::EVENT:
			plug = MessageType::BINARY_SOUND_EVENT_IN;
			break;
		case Sound::Type::ACTIVATION:
			plug = MessageType::BINARY_SOUND_ACTIVATION_IN;
			break;
		case Sound::Type::DEACTIVATION:
			plug = MessageType::BINARY_SOUND_DEACTIVATION_IN;
			break;
		case Sound::Type::INFORMATION:
			plug = MessageType::BINARY_SOUND_INFORMATION_IN;
			break;
		case Sound::Type::WARNING:
			plug = MessageType::BINARY_SOUND_WARNING_IN;
			break;
		case Sound::Type::DOORBELL:
			plug = MessageType::BINARY_SOUND_DOORBELL_IN;
			break;
		case Sound::Type::CALL:
			plug = MessageType::BINARY_SOUND_CALL_IN;
			break;
		case Sound::Type::ALARM:
			plug = MessageType::BINARY_SOUND_ALARM_IN;
			break;
		default:
			plug = MessageType::BINARY_SOUND_IN;
		}
	}

	// set available devices to deviceIds array
	int i = 0;
	this->deviceIds[i++] = BME680_ID;
	if (INPUT_EXT_COUNT > 0)
		this->deviceIds[i++] = IN_ID;
	if (OUTPUT_EXT_COUNT > 0)
		this->deviceIds[i++] = OUT_ID;
	if (OUTPUT_HEATING_COUNT > 0)
		this->deviceIds[i++] = HEATING_ID;
	if (this->soundCount > 0)
		this->deviceIds[i++] = SOUND_ID;
	this->deviceIds[i++] = BRIGHTNESS_SENSOR_ID;
	this->deviceIds[i++] = MOTION_DETECTOR_ID;
	this->deviceCount = i;

	// set device id's
	//for (i = 0; i < DEVICE_COUNT; ++i) {
	//	this->devices[i].id = i + 1;
	//}

	// start coroutines
	readAirSensor();
	publish();
}

LocalInterface::~LocalInterface() {
}

String LocalInterface::getName() {
	return "Local";
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
	case SOUND_ID:
		return {this->soundCount, this->soundPlugs};
	case BRIGHTNESS_SENSOR_ID:
		return brightnessSensorPlugs;
	case MOTION_DETECTOR_ID:
		return motionDetectorPlugs;
	default:
		return {};
	}
}

SubscriberInfo LocalInterface::getSubscriberInfo(uint8_t id, uint8_t plugIndex) {
	auto plugs = getPlugs(id);
	if (plugIndex < plugs.count())
		return {plugs[plugIndex], &this->publishBarrier};
	return {};
}

void LocalInterface::subscribe(Subscriber &subscriber) {
	subscriber.remove();
	auto index = subscriber.data->source.deviceId - 1;
	if (index >= 0 && index < array::count(this->devices))
		this->devices[index].subscribers.add(subscriber);
}

void LocalInterface::listen(Listener &listener) {
	assert(listener.barrier	!= nullptr);
	listener.remove();
	this->listeners.add(listener);
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

		device.publishFloat(BME680_TEMPERATURE_PLUG, airSensor.getTemperature() + 273.15f);
		device.publishFloat(BME680_HUMIDITY_PLUG, airSensor.getHumidity());
		device.publishFloat(BME680_PRESSURE_PLUG, airSensor.getPressure());
		device.publishFloat(BME680_VOC_PLUG, airSensor.getGasResistance());

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
		switch (info.deviceId) {
		case OUT_ID:
			// set "ext" output
			if (info.plugIndex < OUTPUT_EXT_COUNT) {
				if (message.value.u8 <= 1)
					Output::set(OUTPUT_EXT_INDEX + info.plugIndex, message.value.u8 != 0);
				else
					Output::toggle(OUTPUT_EXT_INDEX + info.plugIndex);
			}
			break;
		case HEATING_ID:
			// set heating output
			if (info.plugIndex < OUTPUT_HEATING_COUNT) {
				if (message.value.u8 <= 1)
					Output::set(OUTPUT_HEATING_INDEX, message.value.u8 != 0);
				else
					Output::toggle(OUTPUT_HEATING_INDEX);
			}
			break;
		case SOUND_ID:
			// start sound
			if (info.plugIndex < this->soundCount) {
				if (message.value.u8 != 0)
					Sound::play(info.plugIndex);
				else
					Sound::stop(info.plugIndex);
			}
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


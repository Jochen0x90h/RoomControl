#include "LocalInterface.hpp"
#include <timer.hpp>


// device id's
constexpr int BME680_ID = 0x00000001;
constexpr int BRIGHTNESS_SENSOR_ID = 0x00000002;
constexpr int MOTION_DETECTOR_ID = 0x00000003;

// endpoint id's
constexpr int BME680_ENDPOINTS = 1;
constexpr int TEMPERATURE_SENSOR_ENDPOINT = BME680_ENDPOINTS + 0;
constexpr int AIR_PRESSURE_SENSOR_ENDPOINT = BME680_ENDPOINTS + 1;
constexpr int AIR_HUMIDITY_SENSOR_ENDPOINT = BME680_ENDPOINTS + 2;
constexpr int AIR_VOC_SENSOR_ENDPOINT = BME680_ENDPOINTS + 3;
constexpr int BRIGHTNESS_SENSOR_ENDPOINT = BME680_ENDPOINTS + 4;
constexpr int MOTION_DETECTOR_ENDPOINT = BRIGHTNESS_SENSOR_ENDPOINT + 1;


// device id's
constexpr DeviceId deviceIds[] = {
	BME680_ID,
	BRIGHTNESS_SENSOR_ID,
	MOTION_DETECTOR_ID
};

// endpoint types
constexpr EndpointType bme680Endpoints[] = {
	EndpointType::TEMPERATURE_SENSOR,
	EndpointType::AIR_PRESSURE_SENSOR,
	EndpointType::AIR_HUMIDITY_SENSOR,
	EndpointType::AIR_VOC_SENSOR
};
constexpr EndpointType brightnessSensorEndpoints[] = {
	EndpointType::BRIGHTNESS_SENSOR,
};
constexpr EndpointType motionDetectorEndpoints[] = {
	EndpointType::MOTION_DETECTOR,
};


LocalInterface::LocalInterface() : airSensor([this]() {onAirSensorInitialized();}), subscriptions{} {
	measure();
}

LocalInterface::~LocalInterface() {
}

Array<DeviceId> LocalInterface::getDevices() {
	return deviceIds;
}

Array<EndpointType> LocalInterface::getEndpoints(DeviceId deviceId) {
	switch (deviceId) {
	case BME680_ID:
		return bme680Endpoints;
	case BRIGHTNESS_SENSOR_ID:
		return brightnessSensorEndpoints;
	case MOTION_DETECTOR_ID:
		return motionDetectorEndpoints;
	}
	return {};
}

void LocalInterface::subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	uint8_t id = 0;
	switch (deviceId) {
	case BME680_ID:
		id = BME680_ENDPOINTS + endpointIndex;
		break;
	case BRIGHTNESS_SENSOR_ID:
		id = BRIGHTNESS_SENSOR_ENDPOINT + endpointIndex;
		break;
	}
	
	if (endpointId == 0 && id != 0) {
		// count reference
		++this->subscriptions[id - 1];
		endpointId = id;
	} else {
		// assert that existing endpoint id is the same
		assert(endpointId == id);
	}
}

void LocalInterface::unsubscribe(uint8_t &endpointId) {
	if (endpointId != 0) {
		--this->subscriptions[endpointId - 1];
		endpointId = 0;
	}
}

bool LocalInterface::send(uint8_t endpointId, uint8_t const *data, int length) {
	return false;
}

void LocalInterface::setReceiveHandler(std::function<void (uint8_t, uint8_t const *, int)> onReceived) {
	this->onReceived = onReceived;
}

void LocalInterface::measure() {
	if (this->airSensor.isInitialized()) {
		this->airSensor.startMeasurement();
		timer::start(TIMER_INDEX, timer::getTime() + 1s, [this]() {readAirSensor();});
	} else {
		timer::start(TIMER_INDEX, timer::getTime() + 10s, [this]() {measure();});
	}
}


void LocalInterface::onAirSensorInitialized() {
	this->airSensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100); // heater temperature and duration

	// set onReady to getValues
	this->airSensor.onReady = [this]() {airSensorGetValues();};

}

void LocalInterface::readAirSensor() {
	this->airSensor.readMeasurements(); // calls airSensorGetValues() when ready
	timer::start(TIMER_INDEX, timer::getTime() + 9s, [this]() {measure();});
}

void LocalInterface::airSensorGetValues() {
	
	if (this->subscriptions[TEMPERATURE_SENSOR_ENDPOINT - 1] > 0) {
		float temperature = this->airSensor.getTemperature();
		
		uint8_t t[2];
		this->onReceived(TEMPERATURE_SENSOR_ENDPOINT, t, 2);
	}
	if (this->subscriptions[AIR_PRESSURE_SENSOR_ENDPOINT - 1] > 0) {
		this->airSensor.getPressure();

	}
	if (this->subscriptions[AIR_HUMIDITY_SENSOR_ENDPOINT - 1] > 0) {
		this->airSensor.getHumidity();

	}
	if (this->subscriptions[AIR_VOC_SENSOR_ENDPOINT - 1] > 0) {
		this->airSensor.getGasResistance();

	}
}

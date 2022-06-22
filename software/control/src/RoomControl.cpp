#include <iostream>
#include "RoomControl.hpp"
#include "Menu.hpp"
#include "Message.hpp"
#include <Calendar.hpp>
#include <Poti.hpp>
#include <Input.hpp>
#include <Spi.hpp>
#include <Random.hpp>
#include <Radio.hpp>
#include <Output.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <crypt.hpp>
#include <Queue.hpp>
#include "tahoma_8pt.hpp" // font
#include <cmath>


constexpr String weekdaysLong[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr String weekdaysShort[7] = {"M", "T", "W", "T", "F", "S", "S"};

/*
static String const commandNames[] = {"off", "on", "toggle", "release", "trigger", "up", "down","open", "close",
	"set", "add", "sub", ">", "<", "else"};
static uint8_t const onOffCommands[] = {0, 1};
static uint8_t const onOffToggleCommands[] = {0, 1, 2};
static uint8_t const triggerCommands[] = {3, 4};
static uint8_t const upDownCommands[] = {3, 5, 6};
static uint8_t const openCloseCommands[] = {7, 8};
static uint8_t const setCommands[] = {9, 10, 11};
static uint8_t const compareCommands[] = {12, 13, 14};

static Array<uint8_t const> switchCommands[] = {onOffCommands, onOffToggleCommands, triggerCommands, upDownCommands, openCloseCommands};
*/

static String const releasedPressed[] = {"released", "pressed"};
static String const offOnToggle[] = {"off", "on", "toggle"};
static String const closedOpenToggle[] = {"close", "open", "toggle"};
static String const lockedToggle[] = {"unlocked", "locked", "toggle"};
static String const occupancy[] = {"unoccupied", "occupied"};
static String const activation[] = {"deactivated", "activated"};
static String const enabled[] = {"disabled", "enabled"};

static String const offOn1On2[] = {"off", "on1", "on2"};
static String const releasedUpDown[] = {"released", "up", "down"};
static String const stoppedOpeningClosing[] = {"stopped", "opening", "closing"};
static String const unlockedTiltLocked[] = {"unlocked", "tilt", "locked"};

static String const compareOperators[] = {">", "<", "else"};
static String const commands[] = {"set", "add", "sub"};

// functions

// function plugs
static RoomControl::Plug const switchPlugs[] = {
	{"Power", MessageType::BINARY_POWER_LIGHT_CMD_IN},
	{"Power", MessageType::BINARY_POWER_LIGHT_OUT}};
static RoomControl::Plug const blindPlugs[] = {
	{"Up/Down",     MessageType::TERNARY_BUTTON_IN},
	{"Trigger",     MessageType::BINARY_BUTTON_IN},
	{"Position",    MessageType::LEVEL_OPEN_BLIND_CMD_IN},
	{"Enable Down", MessageType::BINARY_ENABLE_CLOSE_IN},
	{"Open/Close",  MessageType::TERNARY_OPENING_BLIND_OUT}, // stop/opening/closing
	{"Position",    MessageType::LEVEL_OPEN_BLIND_OUT}};
static RoomControl::Plug const heatingControlPlugs[] = {
	{"On/Off",             MessageType::BINARY_POWER_AC_CMD_IN},
	{"Night",              MessageType::BINARY_IN},
	{"Summer",             MessageType::BINARY_IN},
	{"Windows",            MessageType::BINARY_OPEN_WINDOW_IN},
	{"Set Temperature",    MessageType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER_CMD_IN},
	{"Temperature Sensor", MessageType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_IN},
	{"Valve",              MessageType::BINARY_OPEN_VALVE_OUT},
};

struct FunctionInfo {
	String name;
	RoomControl::FunctionFlash::Type type;
	RoomControl::FunctionFlash::TypeClass typeClass;
	uint16_t configSize;
	Array<RoomControl::Plug const> plugs;

};

// function infos (must be sorted by type)
static FunctionInfo const functionInfos[] = {
	{
			"Light",
			RoomControl::FunctionFlash::Type::SIMPLE_SWITCH,
			RoomControl::FunctionFlash::TypeClass::SWITCH,
			0,
			switchPlugs
		},
	{
			"Timeout Light",
			RoomControl::FunctionFlash::Type::TIMEOUT_SWITCH,
			RoomControl::FunctionFlash::TypeClass::SWITCH,
			sizeof(RoomControl::TimeoutSwitch::Config),
			switchPlugs
		},
	{
			"Blind",
			RoomControl::FunctionFlash::Type::TIMED_BLIND,
			RoomControl::FunctionFlash::TypeClass::BLIND,
			sizeof(RoomControl::TimedBlind::Config),
			blindPlugs
		},
	{
			"Heating Control",
			RoomControl::FunctionFlash::Type::HEATING_CONTROL,
			RoomControl::FunctionFlash::TypeClass::HEATING_CONTROL,
			sizeof(RoomControl::HeatingControl::Config),
			heatingControlPlugs
		},
};

static FunctionInfo const &getFunctionInfo(RoomControl::FunctionFlash::Type type) {
	int i = array::binaryLowerBound(functionInfos, [type](FunctionInfo const &info) {return info.type < type;});
	FunctionInfo const &functionInfo = functionInfos[i];
	assert(functionInfo.type == type);
	return functionInfo;
}

RoomControl::FunctionFlash::Type nextFunctionType(RoomControl::FunctionFlash::Type type, int delta) {
	int i = array::binaryLowerBound(functionInfos, [type](FunctionInfo const &info) {return info.type < type;});
	i = (i + array::count(functionInfos) * 256 + delta) % array::count(functionInfos);
	return functionInfos[i].type;
}

static bool isCompatible(RoomControl::Plug const &plug, MessageType messageType) {
	if (plug.isInput())
		return isCompatible(plug.messageType, messageType);
	else
		return isCompatible(messageType, plug.messageType);
}


// RoomControl
// -----------

RoomControl::RoomControl()
	: storage(0, FLASH_PAGE_COUNT, configurations, busInterface.devices, radioInterface.gpDevices,
		radioInterface.zbDevices, alarmInterface.alarms, functions)
	, stateManager()
	//, houseTopicId(), roomTopicId()
	, busInterface(stateManager)
	, radioInterface(stateManager)
{
	// set default configuration if necessary
	if (configurations.isEmpty()) {
		ConfigurationFlash configuration;
		
		// name
		assign(configuration.name, "room");
		
		// generate a random 64 bit address
		configuration.ieeeLongAddress = Random::u64();
		
		// set default pan
		configuration.zbeePanId = 0x1a62;
		
		// generate random network key
		for (uint8_t &b : configuration.key) {
			b = Random::u8();
		}
		setKey(configuration.aesKey, configuration.key);
		
		
		// state offsets for interfaces
		//configuration.busSecurityCounterOffset = stateManager.allocate<uint32_t>();
		//configuration.zbeeSecurityCounterOffset = stateManager.allocate<uint32_t>();

		
		// write to flash
		configurations.write(0, new Configuration(configuration));
	}

	applyConfiguration();

	// start coroutines
	idleDisplay();
	for (auto &function : this->functions) {
		function.coroutine = function.start(*this);
	}
}

RoomControl::~RoomControl() {
	// unregister second handler not needed because RoomControl lives forever
}

void RoomControl::applyConfiguration() {
	auto const &configuration = *this->configurations[0];
	this->busInterface.setConfiguration(configuration.key, configuration.aesKey);
	Radio::setLongAddress(configuration.ieeeLongAddress);
	this->radioInterface.setConfiguration(configuration.zbeePanId, configuration.key, configuration.aesKey);
}


// Functions
// ---------

static void printDevices(Interface &interface) {
	for (int i = 0; i < interface.getDeviceCount(); ++i) {
		auto &device = interface.getDeviceByIndex(i);
		Terminal::out << (hex(device.getId()) + '\n');
		auto endpoints = device.getEndpoints();
		for (auto endpoint : endpoints) {
			Terminal::out << '\t' << getTypeLabel(endpoint);
			if ((endpoint & MessageType::DIRECTION_MASK) == MessageType::IN)
				Terminal::out << " In";
			if ((endpoint & MessageType::DIRECTION_MASK) == MessageType::OUT)
				Terminal::out << " Out";
			Terminal::out << '\n';
		}
	}
}

Interface &RoomControl::getInterface(Connection const &connection) {
	switch (connection.interface) {
		case Connection::Interface::LOCAL:
			return this->localInterface;
		case Connection::Interface::BUS:
			return this->busInterface;
		case Connection::Interface::RADIO:
			return this->radioInterface;
		case Connection::Interface::ALARM:
			return this->alarmInterface;
		default:
			return this->localInterface;
	}
}

/*
Coroutine RoomControl::testSwitch() {
	Subscriber::Barrier barrier;

	printDevices(this->radioInterface);

	Subscriber subscriber;
	subscriber.source = {0, 0};
	subscriber.messageType = MessageType::OFF_ON;
	subscriber.barrier = &barrier;

	Publisher publisher;
	uint8_t message;
	publisher.messageType = MessageType::OFF_ON;
	publisher.message = &message;
	
	//Interface &interface = this->busInterface;
	Interface &interface = this->radioInterface;
	bool haveIn = false;
	bool haveOut = false;
	for (int i = 0; i < interface.getDeviceCount(); ++i) {
		auto &device = interface.getDeviceByIndex(i);
		Array<MessageType const> endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < endpoints.count(); ++endpointIndex) {
			auto endpointType = endpoints[endpointIndex];
			if (!haveIn && (endpointType == MessageType::OFF_ON_IN || endpointType == MessageType::UP_DOWN_IN)) {
				haveIn = true;
				device.addSubscriber(endpointIndex, subscriber);
			}
			if (!haveOut && endpointType == MessageType::OFF_ON_OUT) {
				haveOut = true;
				device.addPublisher(endpointIndex, publisher);
			}
		}
	}

	while (true) {
		// wait for event
		Subscriber::Source source;
		co_await barrier.wait(source, &message);

		// publish
		publisher.publish();

		if (message <= 1)
			Output::set(0, message);
		else if (message == 2)
			Output::toggle(0);
	}
}
*/
int RoomControl::connectFunction(RoomControl::FunctionFlash const &flash, Array<Plug const> plugs,
	Array<Subscriber, MAX_INPUT_COUNT> subscribers, PublishInfo::Barrier &barrier,
	Array<Publisher, MAX_OUTPUT_COUNT> publishers)
{
/*	// count number of inputs and outputs
	int inputCount = 0;
	for (auto &plug : plugs) {
		if (plug.isInput())
			++inputCount;
	}
	int outputCount = plugs.count() - inputCount;
*/
	auto it = flash.getConnections();

	// connect inputs
	int inputIndex = 0;
	int lastPlugIndex = -1;
	int connectionIndex;
	while (inputIndex < flash.connectionCount) {
		auto &connection = it.getConnection();

		// safety check
		if (connection.plugIndex >= plugs.count())
			break;

		// get plug info, break if not an input
		auto &plug = plugs[connection.plugIndex];
		if (!plug.isInput())
			break;

		if (connection.plugIndex != lastPlugIndex) {
			lastPlugIndex = connection.plugIndex;
			connectionIndex = 0;
		}

		// initialize subscriber
		auto &subscriber = subscribers[inputIndex];
		subscriber.destination.type = plug.messageType;
		subscriber.destination.plug.id = connection.plugIndex;
		subscriber.destination.plug.connectionIndex = connectionIndex;
		subscriber.convertOptions = connection.convertOptions;
		subscriber.barrier = &barrier;

		// subscribe to device
		auto &interface = getInterface(connection);
		auto *device = interface.getDeviceById(connection.deviceId);
		if (device != nullptr)
			device->subscribe(connection.endpointIndex, subscriber);

		++it;
		++inputIndex;
		++connectionIndex;
	}

	// connect outputs
	int outputIndex = 0;
	while (inputIndex + outputIndex < flash.connectionCount) {
		auto &connection = it.getConnection();

		// safety check
		if (connection.plugIndex >= plugs.count())
			break;

		// get plug info, break if not an output
		auto &plug = plugs[connection.plugIndex];
		if (!plug.isOutput())
			break;

		// initialize publisher
		auto &publisher = publishers[outputIndex];
		publisher.id = 255;

		// add publisher to device
		auto &interface = getInterface(connection);
		auto *device = interface.getDeviceById(connection.deviceId);
		if (device != nullptr) {
			publisher.setInfo(device->getPublishInfo(connection.endpointIndex));
			publisher.id = connection.plugIndex;
			publisher.srcType = plug.messageType;
			publisher.convertOptions = connection.convertOptions;
		}

		++it;
		++outputIndex;
	}

	// return number of outputs
	return outputIndex;
}

// Menu
// ----

Coroutine RoomControl::idleDisplay() {
	while (true) {
		// get current clock time
		auto now = Calendar::now();
		
		// obtain a bitmap
		auto bitmap = this->swapChain.get();
		
		// display weekday and clock time
		StringBuffer<16> b = weekdaysLong[now.getWeekday()] + "  "
			+ dec(now.getHours()) + ':'
			+ dec(now.getMinutes(), 2) + ':'
			+ dec(now.getSeconds(), 2);
		bitmap->drawText(20, 10, tahoma_8pt, b, 1);

		// show bitmap on display
		this->swapChain.show(bitmap);

		// wait for event
		int index;
		bool value;
		switch (co_await select(Input::trigger(1 << INPUT_POTI_BUTTON, 0, index, value), Calendar::secondTick())) {
		case 1:
			// poti button pressed
			co_await mainMenu();
			break;
		case 2:
			// second elapsed
			break;
		}
	}
}

// menu helpers

static int getComponentCount(MessageType messageType) {
	//return (messageType & MessageType::TYPE_MASK) == MessageType::MOVE_TO_LEVEL ? 2 : 1;
	return (messageType & MessageType::CATEGORY) == MessageType::LIGHTING ? 2 : 1;
}

// next message type for alarms
/*
static MessageType nextAlarmMessageType(MessageType messageType, int delta) {
	static MessageType const types[] {
		MessageType::OFF_ON_IN,
		MessageType::OFF_ON_TOGGLE_IN,
		MessageType::TRIGGER_IN,
		MessageType::UP_DOWN_IN,
		MessageType::OPEN_CLOSE_IN,
		MessageType::SET_LEVEL_IN,
		MessageType::MOVE_TO_LEVEL_IN,
		//MessageType::SET_COLOR_IN,
		//MessageType::SET_COLOR_TEMPERATURE_IN,
		MessageType::SET_AIR_TEMPERATURE_IN,
		MessageType::SET_AIR_HUMIDITY_IN,
	};

	int index = array::binaryLowerBound(types, [messageType](MessageType a) {return a < messageType;});
	index = (index + array::count(types) * 256 + delta) % array::count(types);
	return types[index];
}*/

void RoomControl::printConnection(Menu::Stream &stream, Connection const &connection) {
	auto &interface = getInterface(connection);
	auto device = interface.getDeviceById(connection.deviceId);
	if (device != nullptr) {
		String deviceName = device->getName();
		stream << deviceName << ':' << dec(connection.endpointIndex);
		auto endpoints = device->getEndpoints();
		if (connection.endpointIndex < endpoints.count()) {
			String endpointName = getTypeLabel(endpoints[connection.endpointIndex]);
			stream << " (" << endpointName << ')';
		}
	} else {
		stream << "Connect...";
	}
}

/*
static Message getDefaultMessage(MessageType messageType) {
	switch (messageType & MessageType::TYPE_MASK) {
		case MessageType::OFF_ON:
		case MessageType::OFF_ON_TOGGLE:
		case MessageType::TRIGGER:
		case MessageType::UP_DOWN:
		case MessageType::OPEN_CLOSE:
			return {.command = 1};
		case MessageType::LEVEL:
			return {.value = {.f = 1.0}};
		case MessageType::SET_LEVEL:
			return {.command = 0, .value = {.f = 1.0f}};
		case MessageType::AIR_TEMPERATURE:
			return {.value = {.f = 273.15f + 20.0f}};
		case MessageType::SET_AIR_TEMPERATURE:
			return {.command = 0, .value = {.f =273.15f + 20.0f}};
		case MessageType::AIR_PRESSURE:
			return {.value = {.f = 1000.0f * 100.0f}};
		default:
			return {};
	}
}
*/
Array<String const> RoomControl::getSwitchStates(Usage usage) {
	switch (usage) {
	case Usage::OFF_ON:
		return {2, offOnToggle};
	case Usage::OFF_ON_TOGGLE:
		return offOnToggle;
	case Usage::RELEASED_PRESSED:
		return releasedPressed;
	case Usage::CLOSED_OPEN:
		return {2, closedOpenToggle};
	case Usage::CLOSED_OPEN_TOGGLE:
		return closedOpenToggle;
	case Usage::LOCK:
		return {2, lockedToggle};
	case Usage::LOCK_TOGGLE:
		return lockedToggle;
	case Usage::OCCUPANCY:
		return occupancy;
	case Usage::ACTIVATION:
		return activation;
	case Usage::ENABLED:
		return enabled;

	case Usage::OFF_ON1_ON2:
		return offOn1On2;
	case Usage::RELEASED_UP_DOWN:
		return releasedUpDown;
	case Usage::STOPPED_OPENING_CLOSING:
		return stoppedOpeningClosing;
	case Usage::TILT_LOCK:
		return unlockedTiltLocked;

	default:;
	}
	return offOnToggle;
}

// number of different switch types for default convert options
constexpr int SWITCH_TYPE_COUNT = 5;

int RoomControl::getSwitchType(MessageType type) {
	auto category = type & MessageType::CATEGORY;
	if (category == MessageType::TERNARY) {
		// off/on1/on2 or release/up/down
		return (type & MessageType::TERNARY_CATEGORY) == MessageType ::TERNARY_BUTTON ? 4 : 1;
	}
	auto binaryCategory = type & MessageType::BINARY_CATEGORY;
	if (binaryCategory == MessageType::BINARY_BUTTON || binaryCategory == MessageType::BINARY_ALARM) {
		// release/press
		return 3;
	}

	// off/on or off/on/toggle
	return (type & MessageType::CMD) != 0 ? 2 : 0;
}

constexpr int OFF = 0;
constexpr int ON = 1;
constexpr int ON1 = 1;
constexpr int ON2 = 2;
constexpr int TOGGLE = 2;
constexpr int RELEASE = 0;
constexpr int PRESS = 1;
constexpr int UP = 1;
constexpr int DOWN = 2;
constexpr int SET = 0;
constexpr int INCREASE = 1;
constexpr int DECREASE = 2;
constexpr int DISCARD = 7;

constexpr uint16_t convert(int s0, int s1) noexcept {
	return s0 | (s1 << 3) | (-1 << 6);
}

constexpr uint16_t convert(int s0, int s1, int s2) noexcept {
	return s0 | (s1 << 3) | (s2 << 6) | (-1 << 9);
}

// default switch convert matrix
static uint16_t const switch2switch[SWITCH_TYPE_COUNT][SWITCH_TYPE_COUNT] = {
	// off/on ->
	{
		convert(OFF, ON), // off/on
		convert(OFF, ON1), // off/on1/on2
		convert(OFF, ON), // off/on/toggle
		convert(DISCARD, PRESS), // release/press
		convert(DISCARD, UP), // release/up/down
	},
	// off/on1/on2 ->
	{
		convert(OFF, ON, DISCARD), // off/on
		convert(OFF, ON1, ON2), // off/on1/on2
		convert(OFF, ON, DISCARD), // off/on/toggle
		convert(DISCARD, PRESS, DISCARD), // release/press
		convert(DISCARD, UP, DISCARD), // release/up/down
	},
	// off/on/toggle ->
	{
		convert(OFF, ON, DISCARD), // off/on
		convert(OFF, ON1, DISCARD), // off/on1/on2
		convert(OFF, ON, TOGGLE), // off/on/toggle
		convert(DISCARD, DISCARD, PRESS), // release/press
		convert(DISCARD, DISCARD, UP), // release/up/down
	},
	// release/press ->
	{
		convert(DISCARD, ON), // off/on
		convert(DISCARD, ON1), // off/on1/on2
		convert(DISCARD, TOGGLE), // off/on/toggle
		convert(RELEASE, PRESS), // release/press
		convert(RELEASE, UP), // release/up/down
	},
	// release/up/down ->
	{
		convert(DISCARD, ON, OFF), // off/on
		convert(OFF, ON1, ON2), // off/on1/on2
		convert(DISCARD, ON, OFF), // off/on/toggle
		convert(RELEASE, PRESS, DISCARD), // release/press
		convert(RELEASE, UP, DOWN), // release/up/down
	},
};

// default switch to value command convert
static uint16_t const switch2valueCommand[SWITCH_TYPE_COUNT] = {
	convert(DISCARD, SET), // off/on -> set/increase/decrease
	convert(DISCARD, SET, DISCARD), // off/on1/on2 -> set/increase/decrease
	convert(DISCARD, SET, DISCARD), // off/on/toggle -> set/increase/decrease
	convert(DISCARD, SET), // release/press -> set/increase/decrease
	convert(DISCARD, INCREASE, DECREASE), // release/up/down -> set/increase/decrease
};

// default value command to switch convert
static uint16_t const value2switch[SWITCH_TYPE_COUNT] = {
	convert(ON, OFF, DISCARD), // greater/less/else -> off/on
	convert(ON, OFF, DISCARD), // greater/less/else -> off/on/toggle
	convert(PRESS, RELEASE, DISCARD), // greater/less/else -> release/press
	convert(UP, DOWN, RELEASE), // greater/less/else -> release/up/down
};


inline ConvertOptions makeConvertOptions(int16_t commands, float value0, float value1, float value2) {
	return {.commands = uint16_t(commands), .value = {.f = {value0, value1, value2}}};
}

// default convert options for message logger and generator
ConvertOptions RoomControl::getDefaultConvertOptions(MessageType type) {
	switch (type & MessageType::CATEGORY) {
	case MessageType::BINARY:
	case MessageType::TERNARY: {
		int t = getSwitchType(type);
		return {.commands = uint16_t(switch2switch[t][t])};
	}
	default:;
	}
	return {};
}

// default convert options for function connection
ConvertOptions RoomControl::getDefaultConvertOptions(MessageType dstType, MessageType srcType) {
	// check if destination is a switch
	if ((dstType & MessageType::CATEGORY) <= MessageType::TERNARY) {
		int d = getSwitchType(dstType);
		switch (srcType & MessageType::CATEGORY) {
		case MessageType::BINARY:
		case MessageType::TERNARY:
			// both are a switch
			return {.commands = uint16_t(switch2switch[getSwitchType(srcType)][getSwitchType(dstType)])};
		case MessageType::PHYSICAL:
		case MessageType::CONCENTRATION:
		case MessageType::LIGHTING: {
			// source is a float value, use thresholds to convert
			auto usage = getUsage(srcType);
			float value = getDefaultFloatValue(usage);
			return makeConvertOptions(value2switch[d], value, value, value);
		}
		default:;
		}
	}

	// check if source is a switch
	if ((srcType & MessageType::CATEGORY) <= MessageType::TERNARY) {
		int s = getSwitchType(srcType);
		switch (dstType & MessageType::CATEGORY) {
		case MessageType::PHYSICAL:
		case MessageType::CONCENTRATION:
		case MessageType::LIGHTING: {
			// source is a float value, use list of value and command (set, increment, decrement) to convert
			auto usage = getUsage(dstType);
			float value = getDefaultFloatValue(usage);
			float rvalue = stepValue(usage, true, 0.0f, 1);
			int commands = switch2valueCommand[s];
			bool r0 = (commands & 7) != 0;
			bool r1 = ((commands >> 3) & 7) != 0;
			bool r2 = ((commands >> 6) & 7) != 0;
			float f0 = r0 ? rvalue : value;
			float f1 = r1 ? rvalue : value;
			float f2 = r2 ? rvalue : value;
			return makeConvertOptions(switch2valueCommand[s], f0, f1, f2);
		}
		default:;
		}
	}
	return {};
}

constexpr float CELSIUS_OFFSET = 273.15f;

float RoomControl::getDefaultFloatValue(Usage usage) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
	case Usage::PERCENT:
		return 0.5f;
	case Usage::TEMPERATURE:
		return CELSIUS_OFFSET;
	case Usage::TEMPERATURE_FREEZER:
		return CELSIUS_OFFSET - 18.0f;
	case Usage::TEMPERATURE_FRIDGE:
		return CELSIUS_OFFSET + 4.0f;
	case Usage::TEMPERATURE_OUTDOOR:
		return CELSIUS_OFFSET + 10.0f;
	case Usage::TEMPERATURE_OVEN:
		return CELSIUS_OFFSET + 200.0f;
	case Usage::TEMPERATURE_ROOM:
		return CELSIUS_OFFSET + 20.0f;
	case Usage::TEMPERATURE_COLOR:
		return 3000.0f;
	case Usage::PRESSURE_ATMOSPHERIC:
		return 101320.0f;
	default:
		return 0.0f;
	}
}

static float step(float value, int delta, float increment, float lo, float hi) {
	return clamp(float(iround(value / increment) + delta) * increment, lo, hi);
}

static float step(float value, int delta, float increment, float lo, float hi, bool relative, float rhi) {
	return step(value, delta, increment, relative ? 0.0f : lo, relative ? rhi : hi);
}

static float stepCelsius(float value, int delta, float increment, float lo, float hi, bool relative, float rhi) {
	if (relative)
		return step(value, delta, increment, 0.0f, rhi);
	return step(value - CELSIUS_OFFSET, delta, increment, lo, hi) + CELSIUS_OFFSET;
}

float RoomControl::stepValue(Usage usage, bool relative, float value, int delta) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
	case Usage::PERCENT:
		return step(value, delta, 0.05f, 0.0f, 1.0f, relative, 1.0f);
	case Usage::TEMPERATURE:
		return stepCelsius(value, delta, 1.0f, -273.0f, 5000.0f, relative, 100.0f);
	case Usage::TEMPERATURE_FREEZER:
		return stepCelsius(value, delta, 0.5f, -30.0f, 0.0f, relative, 10.0f);
	case Usage::TEMPERATURE_FRIDGE:
		return stepCelsius(value, delta, 0.5f, 0.0f, 20.0f, relative, 10.0f);
	case Usage::TEMPERATURE_OUTDOOR:
		return step(value - CELSIUS_OFFSET, delta, 1.0f, -50.0f, 60.0f) + CELSIUS_OFFSET;
	case Usage::TEMPERATURE_OVEN:
		return stepCelsius(value, delta, 5.0f, 50.0f, 500.0f, relative, 200.0f);
	case Usage::TEMPERATURE_ROOM:
		// todo: Celsius or Fahrenheit
		return stepCelsius(value, delta, 0.5f, 0.0f, 50.0f, relative, 20.0f);
	case Usage::TEMPERATURE_COLOR:
		return step(value, delta, 100.0f, 1000.0f, 10000.0f, relative, 5000.0f);
	case Usage::PRESSURE_ATMOSPHERIC:
		return step(value, delta, 100.0f, 50000.0f, 150000.0f);
	default:
		return value;
	}
}

Flt RoomControl::getDisplayValue(Usage usage, bool relative, float value) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
		return flt(value, -2);
	case Usage::PERCENT:
		return flt(value * 100.0f, 0);
	case Usage::TEMPERATURE:
	case Usage::TEMPERATURE_FREEZER:
	case Usage::TEMPERATURE_FRIDGE:
	case Usage::TEMPERATURE_OUTDOOR:
	case Usage::TEMPERATURE_OVEN:
	case Usage::TEMPERATURE_ROOM:
		// todo: Celsius or Fahrenheit
		if (relative)
			return flt(value, -1);
		return flt(value - CELSIUS_OFFSET, -1);
	case Usage::TEMPERATURE_COLOR:
		return flt(value, -1);
	case Usage::PRESSURE_ATMOSPHERIC:
		// todo: Unit
		return flt(value * 0.01f, 0);
	default:;
	}
	return {};
}

String RoomControl::getDisplayUnit(Usage usage) {
	switch (usage) {
	case Usage::PERCENT:
		return "%";
	case Usage::TEMPERATURE:
	case Usage::TEMPERATURE_FREEZER:
	case Usage::TEMPERATURE_FRIDGE:
	case Usage::TEMPERATURE_OUTDOOR:
	case Usage::TEMPERATURE_OVEN:
	case Usage::TEMPERATURE_ROOM:
		// todo: Celsius or Fahrenheit
		return "oC";
	case Usage::TEMPERATURE_COLOR:
		return "K";
	case Usage::PRESSURE_ATMOSPHERIC:
		// todo: Unit
		return "hPa";
	default:;
	}
	return {};
}

void RoomControl::editMessage(Menu::Stream &stream, MessageType messageType,
	Message &message, bool editMessage1, bool editMessage2, int delta)
{
	auto usage = getUsage(messageType);
	switch (messageType & MessageType::CATEGORY) {
	case MessageType::BINARY:
	case MessageType::TERNARY: {
		// switch
		auto states = getSwitchStates(usage);
		if (editMessage1)
			message.value.u8 = (message.value.u8 + 30 + delta) % states.count();
		stream << underline(states[message.value.u8], editMessage1);
		break;
	}
	case MessageType::LEVEL:
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION:
		// float
		if (editMessage1)
			message.value.f = stepValue(usage, false, message.value.f, delta);
		stream << underline(getDisplayValue(usage, false, message.value.f), editMessage1) << getDisplayUnit(usage);
		break;
	case MessageType::LIGHTING:
		// float + transition
		if (editMessage1)
			message.value.f = stepValue(usage, false, message.value.f, delta);
		if (editMessage2)
			message.transition = clamp(message.transition + delta, 0, 65535);
		stream << underline(getDisplayValue(usage, false, message.value.f), editMessage1) << getDisplayUnit(usage) << ' ';

		// transition time in 1/10 s
		stream << underline(dec(message.transition / 10) + '.' + dec(message.transition % 10), editMessage2) << 's';
		break;
	default:;
	}
}


static int editDestinationCommand(Menu::Stream &stream, ConvertOptions &convertOptions, int index,
	Array<String const> const &dstCommands, bool editCommand, int delta)
{
	// edit destination command
	int offset = index * 3;
	int command = (convertOptions.commands >> offset) & 7;
	int commandCount = dstCommands.count();
	if (editCommand) {
		command += delta;
		while (command > commandCount)
			command -= commandCount + 1;
		while (command < 0)
			command += commandCount + 1;
		if (command >= commandCount)
			command = 7;
		convertOptions.commands = (convertOptions.commands & ~(7 << offset)) | command << offset;
	}

	stream << underline(command < commandCount ? dstCommands[command] : "(ignore)", editCommand);
	return command;
}

void RoomControl::editConvertSwitch2Switch(Menu &menu, ConvertOptions &convertOptions,
	Array<String const> const &dstStates, Array<String const> const &srcStates)
{
	int delta = menu.getDelta();

	// one menu entry per source switch state
	for (int i = 0; i < srcStates.count(); ++i) {
		auto stream = menu.stream();

		// source command
		stream << srcStates[i] << " -> ";

		// edit destination switch state
		bool editState = menu.getEdit(1) == 1;
		editDestinationCommand(stream, convertOptions, i, dstStates, editState, delta);

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << srcStates.count() * 3;
}

void RoomControl::editConvertFloat2Switch(Menu &menu, ConvertOptions &convertOptions,
	Array<String const> const &dstStates, Usage srcUsage) {

	int delta = menu.getDelta();

	// one menu entry per source compare operator
	for (int i = 0; i < array::count(compareOperators); ++i) {
		auto stream = menu.stream();

		// source command
		stream << compareOperators[i];

		if (i <= 1) {
			// edit value
			bool editValue = menu.getEdit(2) == 1;
			if (editValue)
				convertOptions.value.f[i] = stepValue(srcUsage, false, convertOptions.value.f[i], delta);

			// value
			stream << ' ' << underline(getDisplayValue(srcUsage, false, convertOptions.value.f[i]), editValue)
				<< ' ' << getDisplayUnit(srcUsage);
		}
		stream << " -> ";

		// edit destination switch state
		bool editState = menu.getEdit(2) == 2;
		editDestinationCommand(stream, convertOptions, i, dstStates, editState, delta);

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << array::count(compareOperators) * 3;
}

void RoomControl::editConvertSwitch2FloatCommand(Menu &menu, ConvertOptions &convertOptions, Usage dstUsage,
	Array<String const> const &srcCommands)
{
	static_assert(array::count(commands) <= ConvertOptions::MAX_VALUE_COUNT);

	int delta = menu.getDelta();

	// one menu entry per source command
	for (int i = 0; i < srcCommands.count(); ++i) {

		auto stream = menu.stream();

		// source command
		stream << srcCommands[i] << " -> ";

		// edit destination command
		bool editCommand = menu.getEdit(2) == 1;
		bool lastRelative = convertOptions.getCommand(i) != 0;
		int command = editDestinationCommand(stream, convertOptions, i, commands, editCommand, delta);
		bool relative = command != 0;

		float value = convertOptions.value.f[i];
		if (relative != lastRelative) {
			// command was changed between absolute (set) and relative (increase, decrease)
			if (!relative && lastRelative)
				value += getDefaultFloatValue(dstUsage);
			if (relative && !lastRelative)
				value -= getDefaultFloatValue(dstUsage);
		}

		if (command < array::count(commands)) {
			// edit set-value
			bool editValue = menu.getEdit(2) == 2;
			if (editValue)
				value = stepValue(dstUsage, relative, value, delta);

			// set-value
			stream << ' ' << underline(getDisplayValue(dstUsage, relative, value), editValue)
				<< ' ' << getDisplayUnit(dstUsage);
		} else {
			// ignore command: no value to edit
			menu.getEdit(1);
		}
		convertOptions.value.f[i] = value;

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << srcCommands.count() * 3;
}

void RoomControl::editConvertOptions(Menu &menu, ConvertOptions &convertOptions, MessageType dstType, MessageType srcType) {
	auto dstUsage = getUsage(dstType);
	auto srcUsage = getUsage(srcType);

	// check if destination is a switch
	if ((dstType & MessageType::CATEGORY) <= MessageType::TERNARY) {
		auto dstStates = getSwitchStates(dstUsage);

		switch (srcType & MessageType::CATEGORY) {
		case MessageType::BINARY:
		case MessageType::TERNARY:
			// both are a switch
			editConvertSwitch2Switch(menu, convertOptions, dstStates, getSwitchStates(srcUsage));
			return;
		case MessageType::LEVEL:
		case MessageType::PHYSICAL:
		case MessageType::CONCENTRATION:
		case MessageType::LIGHTING:
			editConvertFloat2Switch(menu, convertOptions, dstStates, srcUsage);
			return;
		case MessageType::METERING:
			// todo
			return;
		default:;
		}
	}

	// check if source is a switch
	if ((srcType & MessageType::CATEGORY) <= MessageType::TERNARY) {
		switch (dstType & MessageType::CATEGORY) {
		case MessageType::LEVEL:
		case MessageType::PHYSICAL:
		case MessageType::CONCENTRATION:
		case MessageType::LIGHTING:
			// source is a switch and destination is a value (with command)
			editConvertSwitch2FloatCommand(menu, convertOptions, dstUsage, getSwitchStates(srcUsage));

			return;
		default:;
		}
	}
}


String getName(RoomControl::Connection::Interface interface) {
	switch (interface) {
		case RoomControl::Connection::Interface::LOCAL:
			return "Local";
		case RoomControl::Connection::Interface::BUS:
			return "Bus";
		case RoomControl::Connection::Interface::RADIO:
			return "Radio";
		case RoomControl::Connection::Interface::ALARM:
			return "Alarm";
		default:
			return "MQTT";
	}
}


// menu

AwaitableCoroutine RoomControl::mainMenu() {
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Local Devices"))
			co_await devicesMenu(this->localInterface);
		if (menu.entry("Bus Devices"))
			co_await devicesMenu(this->busInterface);
		if (menu.entry("Radio Devices"))
			co_await devicesMenu(this->radioInterface);
		if (menu.entry("Alarms"))
			co_await alarmsMenu(this->alarmInterface);
		if (menu.entry("Functions"))
			co_await functionsMenu();
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::devicesMenu(Interface &interface) {
	interface.setCommissioning(true);
	Menu menu(this->swapChain);
	while (true) {
		int count = interface.getDeviceCount();
		for (int i = 0; i < count; ++i) {
			auto &device = interface.getDeviceByIndex(i);
			/*StringBuffer<16> b;
			if (deviceId <= 0xffffffff)
				b = hex(uint32_t(deviceId));
			else
				b = hex(deviceId);
			if (menu.entry(b))*/
			auto stream = menu.stream();
			stream << device.getName();
			if (menu.entry()) {
				co_await deviceMenu(device);
			}
		}
		if (menu.entry("Exit"))
			break;
		
		// show menu and wait for new event until timeout so that we can show newly commissioned devices
		co_await select(menu.show(), Timer::sleep(250ms));
	}
	interface.setCommissioning(false);
}

AwaitableCoroutine RoomControl::deviceMenu(Interface::Device &device) {
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Endpoints"))
			co_await endpointsMenu(device);
		if (menu.entry("Message Logger"))
			co_await messageLogger(device);
		if (menu.entry("Message Generator"))
			co_await messageGenerator(device);
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::alarmsMenu(AlarmInterface &alarms) {
	Menu menu(this->swapChain);
	while (true) {
		int count = alarms.getDeviceCount();
		for (int i = 0; i < count; ++i) {
			auto const &flash = alarms.get(i);
			auto stream = menu.stream();

			// time
			auto time = flash.time;
			stream << dec(time.getHours()) << ':' << dec(time.getMinutes(), 2);

			// week days


			//stream << interface.getDeviceName(i);
			if (menu.entry()) {
				AlarmInterface::AlarmFlash flash2(flash);
				co_await alarmMenu(alarms, i, flash2);
			}
		}
		if (menu.entry("Add")) {
			AlarmInterface::AlarmFlash flash;
			co_await alarmMenu(alarms, count, flash);
		}
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::alarmMenu(AlarmInterface &alarms, int index, AlarmInterface::AlarmFlash &flash) {
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Time"))
			co_await alarmTimeMenu(flash.time);
		//if (menu.entry("Actions"))
		//	co_await alarmActionsMenu(flash);

		if (index < alarms.getDeviceCount()) {
			auto &device = alarms.getDeviceByIndex(index);

			if (menu.entry("Message Logger"))
				co_await messageLogger(device);

			// test alarm
			menu.stream() << "Test Trigger (-> " << dec(alarms.getSubscriberCount(index, 1, 1)) << ')';
			if (menu.entry())
				alarms.test(index, 1, 1);
			menu.stream() << "Test Release (-> " << dec(alarms.getSubscriberCount(index, 1, 0)) << ')';
			if (menu.entry())
				alarms.test(index, 1, 0);


			if (menu.entry("Delete")) {
				// delete alarm
				alarms.erase(index);
				break;
			}
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("Save")) {
			// set alarm
			alarms.set(index, flash);
			break;
		}

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::alarmTimeMenu(AlarmTime &time) {
	Menu menu(this->swapChain);
	while (true) {
		int edit = menu.getEdit(9);
		bool editHours = edit == 1;
		bool editMinutes = edit == 2;
		unsigned int editWeekday = edit - 3;
		int delta = menu.getDelta();

		if (editHours)
			time.addHours(delta);
		if (editMinutes)
			time.addMinutes(delta);
		int weekdays = time.getWeekdays();
		if (editWeekday < 7 && delta != 0) {
			weekdays ^= 1 << editWeekday;
			time.setWeekdays(weekdays);
		}

		auto stream = menu.stream();
		stream << underline(dec(time.getHours()), editHours) << ':'
			<< underline(dec(time.getMinutes(), 2), editMinutes);
		for (int i = 0; i < 7; ++i) {
			stream << "   " << invert(underline(weekdaysShort[i], i == editWeekday), ((weekdays >> i) & 1) != 0);
		}
		menu.entry();

		if (menu.entry("Ok"))
			break;

		// show menu
		co_await menu.show();
	}
}
/*
AwaitableCoroutine RoomControl::alarmActionsMenu(AlarmInterface::AlarmFlash &flash) {
	Menu menu(this->swapChain);
	while (true) {
		// actions: messages to send when alarm goes off
		for (int i = 0; i < flash.endpointCount; ++i) {
			auto messageType = flash.endpoints[i];

			// get component to edit
			int edit = menu.getEdit(1 + getComponentCount(messageType));
			bool editMessageType = edit == 1;
			bool editMessage1 = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			auto stream = menu.stream();

			// edit endpoint type
			if (editMessageType && delta != 0) {
				messageType = nextAlarmMessageType(messageType, delta);
				flash.endpoints[i] = messageType;
				flash.messages[i] = getDefaultMessage(messageType);
			}
			stream << underline(getTypeName(messageType), editMessageType);
			stream << ": ";

			// edit message
			editMessage(stream, messageType, flash.messages[i], editMessage1, editMessage2, delta);
			menu.entry();
		}

		// add action
		if (flash.endpointCount < AlarmInterface::AlarmFlash::MAX_ENDPOINT_COUNT && menu.entry("Add")) {
			int i = flash.endpointCount;
			auto messageType = MessageType::OFF_ON_IN;
			flash.endpoints[i] = messageType;
			flash.messages[i] = getDefaultMessage(messageType);
			++flash.endpointCount;
		}

		// remove action
		if (flash.endpointCount > 0 && menu.entry("Remove")) {
			--flash.endpointCount;
			menu.remove();
		}

		if (menu.entry("Ok"))
			break;

		// show menu
		co_await menu.show();
	}
}
*/
AwaitableCoroutine RoomControl::endpointsMenu(Interface::Device &device) {
	Menu menu(this->swapChain);
	while (true) {
		auto endpoints = device.getEndpoints();
		for (int i = 0; i < endpoints.count(); ++i) {
			auto messageType = endpoints[i];
			auto stream = menu.stream();
			stream << dec(i) << ": " << getTypeLabel(messageType);
			if ((messageType & MessageType::DIRECTION_MASK) == MessageType::IN)
				stream << " In";
			if ((messageType & MessageType::DIRECTION_MASK) == MessageType::OUT)
				stream << " Out";
			menu.entry();
		}
		if (menu.entry("Exit"))
			break;

		// show menu and wait for new event until timeout so that we can show endpoints of recommissioned device
		co_await select(menu.show(), Timer::sleep(250ms));
	}
}



AwaitableCoroutine RoomControl::messageLogger(Interface::Device &device) {
	PublishInfo::Barrier barrier;

	Subscriber subscribers[32];

	// subscribe to all endpoints
	for (int endpointIndex = 0; endpointIndex < array::count(subscribers); ++endpointIndex) {
		auto &subscriber = subscribers[endpointIndex];
		subscriber.destination.device.endpointIndex = endpointIndex;
		subscriber.barrier = &barrier;
		device.subscribe(endpointIndex, subscriber);
	}

	// event queue
	struct Event {
		MessageInfo info;
		Message message;
	};
	Queue<Event, 16> queue;
	
	// add an empty event at the back of the queue to receive the first message
	queue.addBack();

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		// display events
		for (int i = queue.count() - 2; i >= 0; --i) {
			Event &event = queue[i];
			auto stream = menu.stream();
			stream << dec(event.info.device.endpointIndex) << ": ";
			auto usage = getUsage(event.info.type);
			switch (event.info.type & MessageType::CATEGORY) {
			case MessageType::BINARY:
			case MessageType::TERNARY:
				stream << getSwitchStates(usage)[event.message.value.u8];
				break;
			case MessageType::LEVEL:
			case MessageType::PHYSICAL:
			case MessageType::CONCENTRATION:
			case MessageType::LIGHTING: {
				bool relative = (event.info.type & MessageType::CMD) != 0 && event.message.command != 0;
				stream << getDisplayValue(usage, relative, event.message.value.f) << getDisplayUnit(usage);
				break;
			}
			case MessageType::METERING:
				//stream << getDisplayValue(usage, event.message.value.u32) << getDisplayUnit(usage);
				break;
			default:;
			}
			menu.entry();
		}
		
		if (menu.entry("Exit"))
			break;

		// update subscribers
		auto endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < min(endpoints.count(), array::count(subscribers)); ++endpointIndex) {
			auto &subscriber = subscribers[endpointIndex];
			auto messageType = endpoints[endpointIndex];
			subscriber.destination.type = messageType ^ (MessageType::OUT ^ MessageType::IN);
			subscriber.convertOptions = getDefaultConvertOptions(messageType);
		}

		// get the empty event at the back of the queue
		Event &event = queue.getBack();

		// show menu or receive event (event gets filled in)
		int selected = co_await select(menu.show(), barrier.wait(event.info, &event.message), Timer::sleep(250ms));
		if (selected == 2) {
			// received an event: add new empty event at the back of the queue
			queue.addBack();
		}
	}
}

AwaitableCoroutine RoomControl::messageGenerator(Interface::Device &device) {
	//! skip "in" endpoints -> or forward to subscriptions

	uint8_t endpointIndex = 0;
	MessageType lastMessageType = MessageType::UNKNOWN;
	Message message = {};
	PublishInfo::Barrier *publishBarrier;

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		auto endpoints = device.getEndpoints();
		if (endpoints.count() > 0) {
			// get endpoint type
			endpointIndex = clamp(endpointIndex, 0, endpoints.count() - 1);
			auto messageType = endpoints[endpointIndex];

			// get message component to edit (some messages such as MOVE_TO_LEVEL have two components)
			int edit = menu.getEdit(1 + getComponentCount(messageType));
			bool editIndex = edit == 1;
			bool editMessage1 = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			// edit endpoint index
			if (editIndex) {
				endpointIndex = clamp(endpointIndex + delta, 0, endpoints.count() - 1);
				messageType = endpoints[endpointIndex];
			}

			// set default message if type has changed
			if (messageType != lastMessageType) {
				lastMessageType = messageType;

				auto usage = getUsage(messageType);
				switch (messageType & MessageType::CATEGORY) {
				case MessageType::BINARY:
				case MessageType::TERNARY:
					message.value.u8 = 1;
					break;
				case MessageType::LEVEL:
				case MessageType::PHYSICAL:
				case MessageType::CONCENTRATION:
				case MessageType::LIGHTING:
					message.value.f = getDefaultFloatValue(usage);
					break;
				default:;
				}
			}

			// edit message
			auto stream = menu.stream();
			stream << underline(dec(endpointIndex), editIndex) << ": ";
			editMessage(stream, messageType, message, editMessage1, editMessage2, delta);
			menu.entry();

			// send message
			if (menu.entry("Send")) {
				auto info = device.getPublishInfo(endpointIndex);
				info.barrier->resumeFirst([&device, endpointIndex, messageType, &message] (PublishInfo::Parameters &p) {
					p.info = {messageType, {.device = {device.getId(), endpointIndex}}};

					// convert to destination message type and resume coroutine if conversion was successful
					auto &dst = *reinterpret_cast<Message *>(p.message);
					if ((messageType & MessageType::CATEGORY) <= MessageType::TERNARY)
						dst.value.u8 = message.value.u8;
					else if ((messageType & MessageType::CMD) == 0)
						dst.value = message.value;
					else
						dst = message;
					return true;
				});
			}
		}

		if (menu.entry("Exit"))
			break;
			
		// show menu
		//co_await select(menu.show(), Timer::sleep(250ms));
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::functionsMenu() {
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		// list functions
		int count = this->functions.count();
		for (int i = 0; i < count; ++i) {
			auto &function = this->functions[i];
			String name = function->getName();

			if (menu.entry(name)) {
				FunctionFlash flash(*function);
				co_await functionMenu(i, flash);
				break; // function may have been deleted (this->functions.count() may be smaller than count)
			}
		}

		if (menu.entry("Add")) {
			FunctionFlash flash;
			co_await functionMenu(count, flash);
		}

		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

static void editDuration(Menu &menu, Menu::Stream &stream, uint16_t &duration) {
	auto delta = menu.getDelta();
	auto d = unpackHourDuration100(duration);

	// edit duration
	int edit = menu.getEdit(3);
	bool editMinutes = edit == 1;
	bool editSeconds = edit == 2;
	bool editHundredths = edit == 3;
	if (editMinutes)
		d.minutes = clamp(d.minutes + delta, 0, 9);
	if (editSeconds)
		d.seconds = clamp(d.seconds + delta, 0, 59);
	if (editHundredths)
		d.hundredths = clamp(d.hundredths + delta, 0, 99);

	duration = packHourDuration100(d);

	// duration
	stream
		<< underline(dec(d.minutes, 1), editMinutes) << ":"
		<< underline(dec(d.seconds, 2), editSeconds) << "."
		<< underline(dec(d.hundredths, 2), editHundredths) << "0";
	menu.entry();
}

static void editDuration(Menu &menu, Menu::Stream &stream, uint32_t &duration) {
	auto delta = menu.getDelta();
	auto d = unpackHourDuration100(duration);

	// edit duration
	int edit = menu.getEdit(4);
	//bool editDays = edit == 1;
	bool editHours = edit == 1;
	bool editMinutes = edit == 2;
	bool editSeconds = edit == 3;
	bool editHundredths = edit == 4;
	//if (editDays)
	//	duration.days = clamp(duration.days + delta, 0, 497);
	if (editHours)
		d.hours = clamp(d.hours + delta, 0, 99);
	if (editMinutes)
		d.minutes = clamp(d.minutes + delta, 0, 59);
	if (editSeconds)
		d.seconds = clamp(d.seconds + delta, 0, 59);
	if (editHundredths)
		d.hundredths = clamp(d.hundredths + delta, 0, 99);

	duration = packHourDuration100(d);

	// duration
	stream
		//<< underline(dec(duration.days), editDays) << "d "
		<< underline(dec(d.hours), editHours) << ":"
		<< underline(dec(d.minutes, 2), editMinutes) << ":"
		<< underline(dec(d.seconds, 2), editSeconds) << "."
		<< underline(dec(d.hundredths, 2), editHundredths) << "0";
	menu.entry();
}

AwaitableCoroutine RoomControl::functionMenu(int index, FunctionFlash &flash) {
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// todo edit name

		// edit function type
		bool editType = menu.getEdit(1) == 1;
		if (editType && delta != 0)
			flash.setType(nextFunctionType(flash.type, delta));

		// get info for function type
		auto &functionInfo = getFunctionInfo(flash.type);
		int plugCount = functionInfo.plugs.count();

		// function type
		{
			auto stream = menu.stream();
			stream << "Type: " << underline(functionInfo.name, editType);
			menu.entry();
		}

		// type dependent configuration
		switch (flash.type) {
			case FunctionFlash::Type::TIMEOUT_SWITCH: {
				auto &config = flash.getConfig<TimeoutSwitch::Config>();

				auto stream = menu.stream();
				stream << "Timeout: ";
				editDuration(menu, stream, config.duration);
				break;
			}
			case FunctionFlash::Type::TIMED_BLIND: {
				auto &config = flash.getConfig<TimedBlind::Config>();

				// minimum rocker hold time to cause switch-off on release
				auto stream1 = menu.stream();
				stream1 << "Hold Time: ";
				editDuration(menu, stream1, config.holdTime);

				// run time from fully closed to fully open
				auto stream2 = menu.stream();
				stream2 << "Run Time: ";
				editDuration(menu, stream2, config.runTime);

				if (menu.entry("Measure Run Time")) {
					auto it = flash.getConnections();
					for (int connectionIndex = 0; connectionIndex < flash.connectionCount; ++connectionIndex, ++it) {
						auto &connection = it.getConnection();
						auto &plug = functionInfo.plugs[connection.plugIndex];
						if (plug.messageType == MessageType::TERNARY_OPENING_BLIND_OUT /*UP_DOWN_OUT*/ && !connection.isMqtt()) {
							auto &interface = getInterface(connection);
							auto *device = interface.getDeviceById(connection.deviceId);
							if (device != nullptr)
								co_await measureRunTime(*device, connection, config.runTime);
							break;
						}
					}

				}
			}
			default:
				;
		}

		// list all plugs (inputs/outputs) and their connections
		int i = 0;
		auto it = flash.getConnections();
		int inputCount = 0;
		int outputCount = 0;
		for (int plugIndex = 0; plugIndex < plugCount; ++plugIndex) {
			auto &plug = functionInfo.plugs[plugIndex];
			menu.line();
			menu.stream() << (plug.isInput() ? "Input:" : "Output:") << ' ' << plug.name;
			menu.label();

			// edit existing connections to the current plug
			while (i < flash.connectionCount) {
				auto &connection = it.getConnection();
				if (connection.plugIndex != plugIndex)
					break;

				auto stream = menu.stream();
				printConnection(stream, connection);
				if (menu.entry()) {
					co_await editFunctionConnection(it, plug, connection, false);
				}

				++i;
				++it;
			}

			// add new connection if maximum connection count not reached
			if (((plug.isInput() && inputCount < MAX_INPUT_COUNT) || (plug.isOutput() && outputCount < MAX_OUTPUT_COUNT))
				&& menu.entry("Connect..."))
			{
				co_await editFunctionConnection(it, plug, {.plugIndex = uint8_t(plugIndex)}, true);
			}

			// count inputs and outputs
			if (plug.isInput())
				++inputCount;
			else
				++outputCount;
		}
		menu.line();

		// check if function is to be added
		if (index < this->functions.count()) {
			// no: meny entry for deleting existing function
			if (menu.entry("Delete")) {
				// delete alarm
				this->functions.erase(index);
				break;
			}
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("Save")) {
			// ensure consistency
			//assert(flash.connectionCount == i);
			//assert(flash.bufferSize == it.buffer - flash.buffer);
			flash.connectionCount = i;
			flash.bufferSize = it.buffer - flash.buffer;

			// allocate new state object
			auto function = flash.allocate();
			assert(function != nullptr);

			// save function to flash and delete old state object (stops coroutine)
			this->functions.write(index, function);

			// start new coroutine
			function->coroutine = function->start(*this);
			break;
		}
		menu.line();

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::measureRunTime(Interface::Device &device, Connection const &connection,
	uint16_t &runTime)
{
	uint8_t state = 0;


	auto info = device.getPublishInfo(connection.endpointIndex);

	bool up = false;

	int duration = runTime;
	SystemTime startTime;

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		// duration
		auto d = unpackHourDuration100(duration);
		menu.stream() << "Run Time: "
			<< dec(d.minutes, 1) << ":"
			<< dec(d.seconds, 2) << "."
			<< dec(d.hundredths, 2) << "0";
		if (menu.entry()) {
			if (state == 0) {
				state = up ? 1 : 2;
				up = !up;
				startTime = Timer::now();
			} else {
				state = 0;
			}

			info.barrier->resumeFirst([&device, &connection, state] (PublishInfo::Parameters &p) {
				auto dstType = device.getEndpoints()[connection.endpointIndex];
				p.info = {dstType, {.device = {device.getId(), connection.endpointIndex}}};
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(dstType, dst, state, connection.convertOptions);
			});

			//publisher.publish();
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("OK")) {
			runTime = duration;
			break;
		}

		// show menu
		if (state == 0) {
			co_await menu.show();
		} else {
			co_await select(menu.show(), Timer::sleep(10ms));

			if (state != 0)
				duration = (Timer::now() - startTime) * 100 / 1s;
		}
	}
}

AwaitableCoroutine RoomControl::editFunctionConnection(ConnectionIterator &it, Plug const &plug, Connection connection,
	bool add)
{
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// select interface
		bool editInterface = menu.getEdit(1) == 1;
		if (editInterface && delta != 0) {
			int count = int(Connection::Interface::COUNT);
			connection.interface = Connection::Interface((int(connection.interface) + count * 256 + delta) % count);
			connection.deviceId = 0;
		}
		menu.stream() << "Interface: " << underline(getName(connection.interface), editInterface);
		menu.entry();

		// select device
		{
			auto stream = menu.stream();
			printConnection(stream, connection);
			if (menu.entry())
				co_await selectFunctionDevice(connection, plug);
		}

		// select convert options
		auto *device = getInterface(connection).getDeviceById(connection.deviceId);
		if (device != nullptr) {
			auto endpoints = device->getEndpoints();
			auto messageType = endpoints[connection.endpointIndex];

			menu.beginSection();
			if (plug.isInput())
				editConvertOptions(menu, connection.convertOptions, plug.messageType, messageType);
			else
				editConvertOptions(menu, connection.convertOptions, messageType, plug.messageType);
			menu.endSection();
		}

		if (!add) {
			if (menu.entry("Delete")) {
				it.erase();
				break;
			}
		}

		if (menu.entry("Cancel"))
			break;

		if (connection.deviceId != 0 && menu.entry("Ok")) {
			if (add)
				it.insert();
			it.getConnection() = connection;
			//it.setTopic();
			break;
		}

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::selectFunctionDevice(Connection &connection, Plug const &plug) {
	auto &interface = getInterface(connection);

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// list devices with at least one compatible endpoint
		for (int i = 0; i < interface.getDeviceCount(); ++i) {
			auto &device = interface.getDeviceByIndex(i);

			// check if the device has at least one compatible endpoint
			auto endpoints = device.getEndpoints();
			for (auto endpointType : endpoints) {
				if (isCompatible(plug, endpointType) != 0) {
					if (menu.entry(device.getName())) {
						co_await selectFunctionEndpoint(device, connection, plug);
						co_return;
					}
					break;
				}
			}
		}

		if (menu.entry("Cancel"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::selectFunctionEndpoint(Interface::Device &device, Connection &connection,
	Plug const &plug)
{
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// list all compatible endpoints of device
		auto endpoints = device.getEndpoints();
		for (int i = 0; i < endpoints.count(); ++i) {
			auto messageType = endpoints[i];
			if (isCompatible(plug, messageType) != 0) {
				// endpoint is compatible
				String typeName = getTypeLabel(messageType);
				menu.stream() << dec(i) << " (" << typeName << ')';
				if (menu.entry()) {
					// endpoint selected
					connection.deviceId = device.getId();
					connection.endpointIndex = i;
					if (plug.isInput())
						connection.convertOptions = getDefaultConvertOptions(plug.messageType, messageType);
					else
						connection.convertOptions = getDefaultConvertOptions(messageType, plug.messageType);
					co_return;
				}
			}
		}

		if (menu.entry("Cancel"))
			break;

		// show menu
		co_await menu.show();
	}
}


// ConnectionIterator

void RoomControl::ConnectionIterator::setTopic(String topic) {
	auto &flash = this->flash;

	// resize input or output list
	Connection &plug = *reinterpret_cast<Connection *>(this->buffer);
	int oldSize = (offsetof(Connection, endpointIndex) + plug.deviceId + 3) / 4;
	int newSize = (offsetof(Connection, endpointIndex) + topic.count() + 3) / 4;
	int d = newSize - oldSize;
	if (d > 0) {
		auto buffer = this->buffer + oldSize;
		flash.bufferSize += d;
		array::insert(flash.bufferSize - (buffer - flash.buffer), buffer, d);
	} else if (d < 0) {
		auto buffer = this->buffer + newSize;
		array::erase(flash.bufferSize - (buffer - flash.buffer), buffer, -d);
		flash.bufferSize += d;
	}

	// set topic
	array::copy(topic.count(), reinterpret_cast<char *>(this->buffer) + offsetof(Connection, endpointIndex), topic.data);
	plug.deviceId = topic.count();
}

void RoomControl::ConnectionIterator::insert() {
	auto &flash = this->flash;
	++flash.connectionCount;

	int d = (sizeof(Connection) + 3) / 4;
	flash.bufferSize += d;
	array::insert(flash.bufferSize - (this->buffer - flash.buffer), this->buffer, d);

	// initialize new plug
	Connection &connection = *reinterpret_cast<Connection *>(this->buffer);
	connection.plugIndex = 0;
	connection.interface = Connection::Interface::LOCAL;
	connection.deviceId = 0;
}

void RoomControl::ConnectionIterator::erase() {
	auto &flash = this->flash;
	--flash.connectionCount;

	Connection &plug = *reinterpret_cast<Connection *>(this->buffer);
	int d = int((!plug.isMqtt() ? sizeof(Connection) : (offsetof(Connection, endpointIndex) + plug.deviceId)) + 3) / 4;
	array::erase(flash.bufferSize - (this->buffer - flash.buffer), this->buffer, d);
	flash.bufferSize -= d;
}


// FunctionFlash

RoomControl::FunctionFlash::FunctionFlash(FunctionFlash const &flash)
	: type(flash.type), nameLength(flash.nameLength), configOffset(flash.configOffset)
	, connectionCount(flash.connectionCount), connectionsOffset(flash.connectionsOffset)
	, bufferSize(flash.bufferSize)
{
	// copy contents of buffer
	array::copy(flash.bufferSize, this->buffer, flash.buffer);
}

static int getSize(RoomControl::FunctionFlash::Type type) {
	return getFunctionInfo(type).configSize;
}

void RoomControl::FunctionFlash::setType(Type type) {
	int oldSize = (getSize(this->type) + 3) / 4;
	int newSize = (getSize(type) + 3) / 4;
	bool keepConnections = getFunctionInfo(this->type).typeClass == getFunctionInfo(type).typeClass;
	this->type = type;

	if (keepConnections) {
		// resize config and keep connections
		int d = newSize - oldSize;
		if (d > 0)
			array::insert(this->bufferSize + d - this->configOffset, this->buffer + this->configOffset, d);
		else if (d < 0)
			array::erase(this->bufferSize - this->configOffset, this->buffer + this->configOffset, -d);

		// adjust offsets
		this->connectionsOffset += d;
		this->bufferSize += d;
	} else {
		// clear connections
		this->connectionCount = 0;
		this->connectionsOffset = this->configOffset + newSize;
		this->bufferSize = this->connectionsOffset;
	}

	// clear config
	array::fill(newSize, this->buffer + this->configOffset, 0);

	// set default config
	switch (this->type) {
	case RoomControl::FunctionFlash::Type::TIMED_BLIND: {
		auto &config = getConfig<TimedBlind::Config>();
		config.holdTime = 200;
		config.runTime = 1000;
		break;
	}
	default:;
	}
}

void RoomControl::FunctionFlash::setName(String name) {

}

RoomControl::Function *RoomControl::FunctionFlash::allocate() const {
	auto &flash = *this;
	switch (flash.type) {
		case Type::SIMPLE_SWITCH:
			return new SimpleSwitch(flash);
		case Type::TIMEOUT_SWITCH:
			return new TimeoutSwitch(flash);
		case Type::TIMED_BLIND:
			return new TimedBlind(flash);
		case Type::HEATING_CONTROL:
			return new HeatingControl(flash);
	}
	assert(false);
	return nullptr;
}


// Function

RoomControl::Function::~Function() {
	this->coroutine.destroy();
}

struct OffOn {
	uint8_t state = 0;

	void apply(uint8_t command) {
		if (command < 2)
			this->state = command;
		else
			this->state ^= 1;
	}

	operator bool() const {return this->state != 0;}
};

struct FloatValue {
	float value = 0.0f;

	void apply(Message const &message) {
		switch (message.command) {
			case 0:
				// set
				this->value = message.value.f;
				break;
			case 1:
				// increase
				this->value += message.value.f;
				break;
			case 2:
				// decrease
				this->value -= message.value.f;
				break;
		}
	}

	operator float() const {return this->value;}
};

// SimpleSwitch

RoomControl::SimpleSwitch::~SimpleSwitch() {
}

Coroutine RoomControl::SimpleSwitch::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	PublishInfo::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	OffOn state;

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers);

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		MessageInfo info;
		uint8_t message;
		co_await barrier.wait(info, &message);

		// process message
		state.apply(message);

		// publish
		for (int i = 0; i < outputCount; ++i) {
			auto &publisher = publishers[i];
			publisher.publishSwitch(state.state);
		}
	}
}


// TimeoutSwitch

RoomControl::TimeoutSwitch::~TimeoutSwitch() {
}

Coroutine RoomControl::TimeoutSwitch::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	PublishInfo::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	OffOn state;

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers);

	auto duration = (*this)->getConfig<Config>().duration;
	//SystemTime time;
	//uint32_t duration;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		MessageInfo info;
		uint8_t message;
		if (state == 0) {
			co_await barrier.wait(info, &message);
		} else {
			auto timeout = Timer::now() + (duration / 100) * 1s + ((duration % 100) * 1s) / 100;
			int s = co_await select(barrier.wait(info, &message), Timer::sleep(timeout));
			if (s == 2) {
				// switch off
				message = 0;
			}

			/*
			while (duration > 0) {
				int h;
				if (duration > 200000100) {
					h = 200000000;
					duration -= 200000000;
				} else {
					h = duration;
					duration = 0;
				}
				time += (h / 100) * 1s + ((h % 100) * 1s) / 100;
				int s = co_await select(barrier.wait(inputIndex, &message), Timer::sleep(time));
				if (s == 1)
					break;
				if (duration == 0) {
					// switch off
					state = 0;
					inputIndex = -1;
				}
			}*/
		}

		// process message
		if (info.plug.id == 0) {
			// on/off in
			state.apply(message);
		}
/*
		// start timeout
		if (state == 1) {
			time = Timer::now();
			duration = config.duration;
		}
*/
		// publish
		for (int i = 0; i < outputCount; ++i) {
			auto &publisher = publishers[i];
			publisher.publishSwitch(state.state);
		}
	}
}


// TimedBlind

RoomControl::TimedBlind::~TimedBlind() {
}

Coroutine RoomControl::TimedBlind::start(RoomControl &roomControl) {
	// connect inputs and outputs
	Subscriber subscribers[MAX_INPUT_COUNT];
	PublishInfo::Barrier barrier;
	Publisher publishers[MAX_OUTPUT_COUNT];
	int outputCount = roomControl.connectFunction(**this, blindPlugs, subscribers, barrier, publishers);

	// state
	uint8_t state = 0;
	bool up = false;
	uint8_t enableDown = 1;

	// rocker timeout
	SystemDuration const holdTime = ((*this)->getConfig<Config>().holdTime * 1s) / 100;

	// position
	SystemDuration const maxPosition = ((*this)->getConfig<Config>().runTime * 1s) / 100;
	SystemDuration position = maxPosition / 2;
	SystemDuration targetPosition = 0s;
	SystemTime startTime;
	SystemTime lastTime;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		MessageInfo info;
		Message message;
		if (state == 0) {
			co_await barrier.wait(info, &message);
		} else {
			auto d = targetPosition - position;

			// set invalid id in case timeout occurs
			info.plug.id = 255;

			// wait for event or timeout with a maximum to regularly report the current position
			int s = co_await select(barrier.wait(info, &message), Timer::sleep(min(up ? -d : d, 200ms)));

			// get time since last time
			auto time = Timer::now();
			d = time - lastTime;
			lastTime = time;

			// update position
			if (up) {
				position -= d;
				if (position <= targetPosition)
					position = targetPosition;
			} else {
				position += d;
				if (position >= targetPosition)
					position = targetPosition;
			}
		}

		// process message
		switch (info.plug.id) {
		case 0:
		case 1:
			// up/down or trigger in
			if (message.value.u8 == 0) {
				// released: stop if timeout elapsed
				if (Timer::now() > startTime + holdTime)
					targetPosition = position;
			} else {
				// up or down pressed
				if (state == 0) {
					// start if currently stopped
					if (info.plug.id == 1) {
						// trigger
						targetPosition = ((up || (targetPosition == 0s)) && targetPosition < maxPosition) ? maxPosition : 0s;
					} else if (message.value.u8 == 1) {
						// up
						targetPosition = 0s;
					} else {
						// down
						targetPosition = maxPosition;
					}
				} else {
					// stop if currently moving
					targetPosition = position;
				}
			}
			break;
		case 2: {
			// position in
			auto p = maxPosition * message.value.f;
			switch (message.command) {
			case 0:
				// set position
				Terminal::out << "set position " << flt(message.value.f) << '\n';
				targetPosition = p;
				break;
			case 1:
				// increase position
				Terminal::out << "increase position " << flt(message.value.f) << '\n';
				targetPosition += p;
				break;
			case 2:
				// decrease position
				Terminal::out << "decrease position " << flt(message.value.f) << '\n';
				targetPosition -= p;
				break;
			}
			if (targetPosition < 0s)
				targetPosition = 0s;
			if (targetPosition > maxPosition)
				targetPosition = maxPosition;
			break;
		}
		case 3:
			// enable down
			enableDown = message.value.u8;
			break;
		}

		// stop if down is disabled and direction is down
		if (!enableDown && targetPosition > position)
			targetPosition = position;

		// check if target position already reached
		if (position == targetPosition) {
			// stop
			state = 0;
		} else if (state == 0) {
			// start
			up = targetPosition < position;
			state = up ? 1 : 2;
			lastTime = startTime = Timer::now();
		}

		// publish
		for (int i = 0; i < outputCount; ++i) {
			auto &publisher = publishers[i];
			//Terminal::out << "id " << dec(publisher.id) << '\n';
			switch (publisher.id) {
			case 4:
				// up/down out
				publisher.publishSwitch(state);
				break;
			case 5: {
				// position out
				float outPosition = position / maxPosition;
				//Terminal::out << "pos " << flt(outPosition) << '\n';
				publisher.publishFloat(outPosition);
				break;
			}
			}
		}
	}
}


// HeatingRegulator

RoomControl::HeatingControl::~HeatingControl() {
}

Coroutine RoomControl::HeatingControl::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	PublishInfo::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	uint8_t valve = 0;

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, heatingControlPlugs, subscribers, barrier, publishers);

	OffOn on;
	OffOn night;
	OffOn summer;
	uint32_t windows = 0xffffffff;
	FloatValue setTemperature = {20.0f + CELSIUS_OFFSET};
	float temperature = 20.0f + 273.15f;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		MessageInfo info;
		Message message;
		co_await barrier.wait(info, &message);

		// process message
		switch (info.plug.id) {
		case 0:
			// on/off in
			on.apply(message.value.u8);
			break;
		case 1:
			// night in
			night.apply(message.value.u8);
			break;
		case 2:
			// summer in
			summer.apply(message.value.u8);
			break;
		case 3:
			// windows in
			Terminal::out << "window " << dec(info.plug.connectionIndex) << (message.value.u8 ? " close\n" : " open\n");
			if (message.value.u8 == 0)
				windows &= ~(1 << info.plug.connectionIndex);
			else
				windows |= 1 << info.plug.connectionIndex;
			break;
		case 4:
			// set temperature in
			setTemperature.apply(message);
			Terminal::out << "set temperature " << flt(float(setTemperature) - 273.15f) + '\n';
			break;
		case 5:
			// measured temperature in
			temperature = message.value.f;
			Terminal::out << "temperature " << flt(temperature - 273.15f) + '\n';
			break;
		}

		// check if on and all windows closed
		if (on == 1 && windows == 0xffffffff) {
			// determine state of valve (simple two-position controller)
			if (valve == 0) {
				// switch on when current temperature below set temperature
				if (temperature < float(setTemperature) - 0.2f)
					valve = 1;
			} else {
				if (temperature > float(setTemperature) + 0.2f)
					valve = 0;
			}
		} else {
			valve = 0;
		}

		// publish
		for (int i = 0; i < outputCount; ++i) {
			auto &publisher = publishers[i];
			publisher.publishSwitch(valve);
		}
	}
}

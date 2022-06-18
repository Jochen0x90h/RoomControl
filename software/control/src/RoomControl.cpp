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

static String const switchReleasePress[] = {"release", "press"};
static String const switchOffOn[] = {"off", "on"};
static String const switchOpenClose[] = {"open", "close"};
static String const switchReleaseUpDown[] = {"release", "up", "down"};
static String const compareOperators[] = {">", "<", "else"};
static String const commands[] = {"set", "add", "sub"};

// functions

// function plugs
/*
static RoomControl::Plug const switchPlugs[] = {
	{"Switch", MessageType::OFF_ON_TOGGLE_IN},
	{"Switch", MessageType::OFF_ON_OUT}};
static RoomControl::Plug const blindPlugs[] = {
	{"Up/Down", MessageType::UP_DOWN_IN},
	{"Trigger", MessageType::TRIGGER_IN},
	{"Position", MessageType::SET_LEVEL_IN},
	{"Enable Down", MessageType::OFF_ON_IN},
	{"Up/Down", MessageType::UP_DOWN_OUT},
	{"Position", MessageType::LEVEL_OUT}};
static RoomControl::Plug const heatingControlPlugs[] = {
	{"On/Off", MessageType::OFF_ON_TOGGLE_IN},
	{"Night", MessageType::OFF_ON_TOGGLE_IN},
	{"Summer", MessageType::OFF_ON_TOGGLE_IN},
	{"Windows", MessageType::OPEN_CLOSE_IN},
	{"Set Temperature", MessageType::SET_AIR_TEMPERATURE_IN},
	{"Temperature Sensor", MessageType::AIR_TEMPERATURE_IN},
	{"Valve", MessageType::OFF_ON_OUT},
};
*/
static RoomControl::Plug const switchPlugs[] = {
	{"Switch", MessageType2::SWITCH_BINARY_ON_OFF_CMD_IN},
	{"Switch", MessageType2::SWITCH_BINARY_ON_OFF_OUT}};
static RoomControl::Plug const blindPlugs[] = {
	{"Up/Down", MessageType2::SWITCH_TERNARY_ONESHOT_ROCKER_IN},
	{"Trigger", MessageType2::SWITCH_BINARY_ONESHOT_BUTTON_IN},
	{"Position", MessageType2::LEVEL_APERTURE_BLIND_CMD_IN},
	{"Enable Down", MessageType2::SWITCH_BINARY_ENABLE_CLOSE_IN},
	{"Open/Close", MessageType2::SWITCH_TERNARY_OPEN_CLOSE_OUT},
	{"Position", MessageType2::LEVEL_APERTURE_BLIND_OUT}};
static RoomControl::Plug const heatingControlPlugs[] = {
	{"On/Off", MessageType2::SWITCH_BINARY_ON_OFF_CMD_IN},
	{"Night", MessageType2::SWITCH_BINARY_ON_OFF_CMD_IN},
	{"Summer", MessageType2::SWITCH_BINARY_ON_OFF_CMD_IN},
	{"Windows", MessageType2::SWITCH_BINARY_APERTURE_IN},
	{"Set Temperature", MessageType2::PHYSICAL_TEMPERATURE_SETPOINT_CMD_IN},
	{"Temperature Sensor", MessageType2::PHYSICAL_TEMPERATURE_MEASURED_IN},
	{"Valve", MessageType2::SWITCH_BINARY_ON_OFF_OUT},
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
			"Switch",
			RoomControl::FunctionFlash::Type::SIMPLE_SWITCH,
			RoomControl::FunctionFlash::TypeClass::SWITCH,
			0,
			switchPlugs
		},
	{
			"Timeout Switch",
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

static bool isCompatible(RoomControl::Plug const &plug, MessageType2 messageType) {
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
			if ((endpoint & MessageType2::DIRECTION_MASK) == MessageType2::IN)
				Terminal::out << " In";
			if ((endpoint & MessageType2::DIRECTION_MASK) == MessageType2::OUT)
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

static int getComponentCount(MessageType2 messageType) {
	//return (messageType & MessageType::TYPE_MASK) == MessageType::MOVE_TO_LEVEL ? 2 : 1;
	return (messageType & MessageType2::CATEGORY) == MessageType2::LIGHTING ? 2 : 1;
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
static Message2 getDefaultMessage(MessageType2 messageType) {
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

// number of different switch types for default convert options
constexpr int SWITCH_TYPE_COUNT = 4;

static int getSwitchType(MessageType2 type) {
	auto switchCategory = type & MessageType2::SWITCH_CATEGORY;
	if (switchCategory == MessageType2::SWITCH_TERNARY)
		return 3;

	auto binaryCategory = type & MessageType2::SWITCH_BINARY_CATEGORY;
	if (binaryCategory == bus::EndpointType2::SWITCH_BINARY_ONESHOT)
		return 2;

	return (type & MessageType2::CMD) != 0 ? 1 : 0;
}

// default switch convert matrix
static int16_t const switch2switch[SWITCH_TYPE_COUNT][SWITCH_TYPE_COUNT] = {
	// off/on ->
	{
		0 | (1 << 3) | -1 << 6, // off/on
		0 | (1 << 3) | -1 << 6, // off/on/toggle
		7 | (1 << 3) | -1 << 6, // release/press
		7 | (1 << 3) | -1 << 6, // release/up/down
	},
	// off/on/toggle ->
	{
		0 | (1 << 3) | -1 << 6, // off/on
		0 | (1 << 3) | (2 << 6) | -1 << 9, // off/on/toggle
		7 | (7 << 3) | (1 << 6) | -1 << 9, // release/press
		7 | (1 << 3) | -1 << 6, // release/up/down
	},
	// release/press ->
	{
		7 | (1 << 3) | -1 << 6, // off/on
		7 | (2 << 3) | -1 << 6, // OFF_ON_TOGGLE
		0 | (1 << 3) | -1 << 6, // release/press
		0 | (1 << 3) | -1 << 6, // release/up/down
	},
	// release/up/down ->
	{
		7 | (1 << 3) | (0 << 6) | -1 << 9, // off/on
		7 | (1 << 3) | (0 << 6) | -1 << 9, // off/on/toggle
		0 | (1 << 3) | -1 << 6, // release/press
		0 | (1 << 3) | (2 << 6) | -1 << 9, // release/up/down
	},
};

// default switch to value command convert
static int16_t const switch2valueCommand[SWITCH_TYPE_COUNT] = {
	7 | (0 << 3) | -1 << 6, // off/on -> set value
	7 | (0 << 3) | -1 << 6, // off/on/toggle -> set value
	7 | (0 << 3) | -1 << 6, // release/press -> set value
	7 | (1 << 3) | (2 << 6) | -1 << 9, // release/up/down -> increase/decrease value
};

// default value command to switch convert
static int16_t const value2switch[SWITCH_TYPE_COUNT] = {
	1 | (0 << 3) | -1 << 6, // greater/less value -> off/on
	1 | (0 << 3) | -1 << 6, // greater/less value -> off/on/toggle
	1 | (1 << 3) | (0 << 6) | -1 << 9, // greater/less/else value -> release/press
	1 | (2 << 3) | (0 << 6) | -1 << 9, // equal/greater/less value -> release/up/down
};

/*
// default command convert matrix
static int16_t const command2command[SWITCH_COMMAND_COUNT][SWITCH_COMMAND_COUNT] = {
	// OFF_ON ->
	{
		0 | (1 << 3) | -1 << 6, // OFF_ON
		0 | (1 << 3) | -1 << 6, // OFF_ON_TOGGLE
		7 | (1 << 3) | -1 << 6, // TRIGGER
		7 | (1 << 3) | -1 << 6, // UP_DOWN
		0 | (1 << 3) | -1 << 6, // OPEN_CLOSE
	},
	// OFF_ON_TOGGLE ->
	{
		0 | (1 << 3) | -1 << 6, // OFF_ON
		0 | (1 << 3) | (2 << 6) | -1 << 9, // OFF_ON_TOGGLE
		7 | (7 << 3) | (1 << 6) | -1 << 9, // TRIGGER
		7 | (1 << 3) | -1 << 6, // UP_DOWN
		0 | (1 << 3) | -1 << 6, // OPEN_CLOSE
	},
	// TRIGGER ->
	{
		7 | (1 << 3) | -1 << 6, // OFF_ON
		7 | (2 << 3) | -1 << 6, // OFF_ON_TOGGLE
		0 | (1 << 3) | -1 << 6, // TRIGGER
		0 | (1 << 3) | -1 << 6, // UP_DOWN
		7 | (1 << 3) | -1 << 6, // OPEN_CLOSE
	},
	// UP_DOWN ->
	{
		7 | (1 << 3) | (0 << 6) | -1 << 9, // OFF_ON
		7 | (1 << 3) | (0 << 6) | -1 << 9, // OFF_ON_TOGGLE
		0 | (1 << 3) | -1 << 6, // TRIGGER
		0 | (1 << 3) | (2 << 6) | -1 << 9, // UP_DOWN
		7 | (1 << 3) | (0 << 6) | -1 << 9, // OPEN_CLOSE
	},
	// OPEN_CLOSE ->
	{
		0 | (1 << 3) | -1 << 6, // OFF_ON
		0 | (1 << 3) | -1 << 6, // OFF_ON_TOGGLE
		7 | (1 << 3) | -1 << 6, // TRIGGER
		7 | (1 << 3) | -1 << 6, // UP_DOWN
		0 | (1 << 3) | -1 << 6, // OPEN_CLOSE
	},
};

// default command to set value convert
static int16_t const command2setValue[SWITCH_COMMAND_COUNT] = {
	7 | (0 << 3) | -1 << 6, // OFF_ON -> set
	7 | (0 << 3) | -1 << 6, // OFF_ON_TOGGLE -> set
	7 | (0 << 3) | -1 << 6, // TRIGGER -> set
	7 | (1 << 3) | (2 << 6) | -1 << 9, // UP_DOWN -> increase/decrease
	7 | (0 << 3) | -1 << 6, // OPEN_CLOSE -> set
};

// default value to command convert
static int16_t const value2command[SWITCH_COMMAND_COUNT] = {
	1 | (0 << 3) | -1 << 6, // greater/less -> OFF_ON
	1 | (0 << 3) | -1 << 6, // greater/less -> OFF_ON_TOGGLE
	1 | (1 << 3) | (0 << 6) | -1 << 9, // greater/less/else -> TRIGGER
	1 | (2 << 3) | (0 << 6) | -1 << 9, // equal/greater/less -> UP_DOWN
	1 | (0 << 3) | -1 << 6, // greater/less -> OPEN_CLOSE
};
*/

inline ConvertOptions makeConvertOptions(int16_t commands, float value1, float value2) {
	return {.commands = uint16_t(commands), .value = {.f = {value1, value2}}};
}

// default convert options for message logger and generator
static ConvertOptions getDefaultConvertOptions(MessageType2 type) {
	switch (type & MessageType2::CATEGORY) {
	case MessageType2::SWITCH: {
		int t = getSwitchType(type);
		return {.commands = uint16_t(switch2switch[t][t])};
	}
	default:;
	}
	return {};
	/*
	switch (messageType & MessageType::TYPE_MASK) {
	case MessageType::OFF_ON:
	case MessageType::OFF_ON_TOGGLE:
	case MessageType::TRIGGER:
	case MessageType::UP_DOWN:
	case MessageType::OPEN_CLOSE: {
		auto mt = int(messageType & MessageType::TYPE_MASK) - int(MessageType::OFF_ON);
		return {.commands = uint16_t(command2command[mt][mt])};
	}
	default:
		break;
	}*/
}

// default convert options for function connection
static ConvertOptions getDefaultConvertOptions(MessageType2 dstType, MessageType2 srcType) {
	// check if destination is a switch
	if ((dstType & MessageType2::CATEGORY) == MessageType2::SWITCH) {
		int d = getSwitchType(dstType);
		switch (srcType & MessageType2::CATEGORY) {
		case MessageType2::SWITCH:
			// both are a switch
			return {.commands = uint16_t(switch2switch[getSwitchType(srcType)][getSwitchType(dstType)])};
		case MessageType2::PHYSICAL:
		case MessageType2::CONCENTRATION:
		case MessageType2::LIGHTING:
			// source is a float value, use thresholds to convert
			// todo: units
			return makeConvertOptions(value2switch[d], 0.5f, 0.5f);
		default:;
		}
	}

	// check if source is a switch
	if ((srcType & MessageType2::CATEGORY) == MessageType2::SWITCH) {
		int s = getSwitchType(srcType);
		switch (dstType & MessageType2::CATEGORY) {
		case MessageType2::PHYSICAL:
		case MessageType2::CONCENTRATION:
		case MessageType2::LIGHTING:
			// source is a float value, use list of value and command (set, increment, decrement) to convert
			// todo: units
			return makeConvertOptions(switch2valueCommand[s], 0.5f, 0.5f);
		default:;
		}
	}
	return {};

/*
	// check if both are a switch command
	auto d = uint(dstType) - uint(MessageType::OFF_ON_IN);
	auto s = uint(srcType) - uint(MessageType::OFF_ON_OUT);
	bool dstIsCommand = d < SWITCH_COMMAND_COUNT;
	bool srcIsCommand = s < SWITCH_COMMAND_COUNT;
	if (dstIsCommand && srcIsCommand)
		return {.commands = uint16_t(command2command[s][d])};

	// check if source is a switch command and destination is a set command
	if (srcIsCommand) {
		switch (dstType) {
		case MessageType::SET_LEVEL_IN:
			return makeConvertOptions(command2setValue[s], 0.5f, 0.1f);
		case MessageType::SET_AIR_TEMPERATURE_IN:
			return makeConvertOptions(command2setValue[s], 293.15f, 0.5f);
		default:;
		}
	}

	// check if source is a simple value and destination is a switch command
	// todo units
	if (dstIsCommand) {
		switch (srcType) {
		case MessageType::LEVEL_OUT:
			return makeConvertOptions(value2command[d], 0.5f, 0.5f);
		case MessageType::AIR_TEMPERATURE_OUT:
			return makeConvertOptions(value2command[d], 293.15f + 0.5f, 293.15f - 0.5f);
		case MessageType::AIR_HUMIDITY_OUT:
			return makeConvertOptions(value2command[d], 0.6f, 0.01f);
		case MessageType::AIR_PRESSURE_OUT:
			return makeConvertOptions(value2command[d], 1000.0f * 100.0f, 100.0f);
		case MessageType::AIR_VOC_OUT:
			return makeConvertOptions(value2command[d], 10.0f, 1.0f);
		default:;
		}
	}

	return {};
 */
}

/*
float RoomControl::stepTemperature(float value, int delta) {
	return std::round(value * 2.0f + float(delta)) * 0.5f;
}

Flt RoomControl::displayTemperature(float kelvin) {
	return flt(kelvin - 273.15f, -1);
}

String RoomControl::getTemperatureUnit() {
	return "oC";
}
*/
Array<String const> RoomControl::getSwitchStates(Usage usage) {
	switch (usage) {
	case Usage::RELEASE_PRESS:
		return switchReleasePress;
	case Usage::OFF_ON:
		return switchOffOn;
	}
	return switchOffOn;
}

static float step(float value, int delta, float increment, float lo, float hi) {
	return clamp(float(iround(value / increment) + delta) * increment, lo, hi);
}

constexpr float CELSIUS_OFFSET = 273.15f;

float RoomControl::getDefaultFloat(Usage usage) {
	switch (usage) {
	case Usage::KELVIN:
		return CELSIUS_OFFSET;
	case Usage::OUTDOOR_TEMPERATURE:
		return CELSIUS_OFFSET + 10.0f;
	case Usage::ROOM_TEMPERATURE:
		return CELSIUS_OFFSET + 20.0f;
	case Usage::COLOR_TEMPERATURE:
		return 3000.0f;
	default:
		return 0.0f;
	}
}

float RoomControl::stepValue(Usage usage, bool relative, float value, int delta) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
	case Usage::PERCENTAGE:
		if (relative)
			return step(value, delta, 0.05f, -1.0f, 1.0f);
		return step(value, delta, 0.05f, 0.0f, 1.0f);
	case Usage::OUTDOOR_TEMPERATURE:
		return step(value - CELSIUS_OFFSET, delta, 1.0f, -50.0f, 60.0f) + CELSIUS_OFFSET;
	case Usage::ROOM_TEMPERATURE:
		// todo: Celsius or Fahrenheit
		if (relative)
			return step(value, delta, 0.5f, -20.0f, 20.0f);
		return step(value - CELSIUS_OFFSET, delta, 0.5f, 0.0f, 50.0f) + CELSIUS_OFFSET;
	case Usage::COLOR_TEMPERATURE:
		if (relative)
			return step(value, delta, 100.0f, -5000.0f, 5000.0f);
		return step(value, delta, 100.0f, 1000.0f, 10000.0f);
	default:
		return value;
	}
}

Flt RoomControl::getDisplayValue(Usage usage, bool relative, float value) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
		return flt(value, -2);
	case Usage::PERCENTAGE:
		return flt(value, 0);
	case Usage::OUTDOOR_TEMPERATURE:
	case Usage::ROOM_TEMPERATURE:
		// todo: Celsius or Fahrenheit
		if (relative)
			return flt(value, -1);
		return flt(value - CELSIUS_OFFSET, -1);
	case Usage::COLOR_TEMPERATURE:
		return flt(value, -1);
	}
	return {};
}

String RoomControl::getDisplayUnit(Usage usage) {
	switch (usage) {
	case Usage::PERCENTAGE:
		return "%";
	case Usage::OUTDOOR_TEMPERATURE:
	case Usage::ROOM_TEMPERATURE:
		// todo: Celsius or Fahrenheit
		return "oC";
	case Usage::COLOR_TEMPERATURE:
		return "K";
	default:;
	}
	return {};
}

/*
static void editCommand(Menu::Stream &stream, Message &message, bool editMessage, int delta, Array<uint8_t const> commands) {
	if (editMessage)
		message.command = (message.command + 30 + delta) % commands.count();
	stream << underline(commandNames[commands[message.command]], editMessage);
}*/

void RoomControl::editMessage(Menu::Stream &stream, MessageType2 messageType,
	Message2 &message, bool editMessage1, bool editMessage2, int delta)
{
	auto usage = getUsage(messageType);
	switch (messageType & MessageType2::CATEGORY) {
	case MessageType2::SWITCH: {
		// switch
		auto states = getSwitchStates(usage);
		if (editMessage1)
			message.value.u8 = (message.value.u8 + 30 + delta) % states.count();
		stream << underline(states[message.command], editMessage1);
		break;
	}
	case MessageType2::LEVEL:
	case MessageType2::PHYSICAL:
	case MessageType2::CONCENTRATION:
		// float
		if (editMessage1)
			message.value.f = stepValue(usage, false, message.value.f, delta);
		stream << underline(getDisplayValue(usage, false, message.value.f), editMessage1) << getDisplayUnit(usage);
		break;
	case MessageType2::LIGHTING:
		// float + transition
		if (editMessage1)
			message.value.f = stepValue(usage, false, message.value.f, delta);
		if (editMessage2)
			message.transition = clamp(message.transition + delta, 0, 65535);
		stream << underline(getDisplayValue(usage, false, message.value.f), editMessage1) << getDisplayUnit(usage) << ' ';

		// transition time in 1/10 s
		stream << underline(dec(message.transition / 10) + '.' + dec(message.transition % 10), editMessage2) << 's';
		break;
	}



	/*
	// edit and display message values
	switch (messageType & MessageType::TYPE_MASK) {
	case MessageType::OFF_ON:
		editCommand(stream, message, editMessage1, delta, onOffCommands);
		break;
	case MessageType::OFF_ON_TOGGLE:
		editCommand(stream, message, editMessage1, delta, onOffToggleCommands);
		break;
	case MessageType::TRIGGER:
		editCommand(stream, message, editMessage1, delta, triggerCommands);
		break;
	case MessageType::UP_DOWN:
		editCommand(stream, message, editMessage1, delta, upDownCommands);
		break;
	case MessageType::OPEN_CLOSE:
		editCommand(stream, message, editMessage1, delta, openCloseCommands);
		break;
	case MessageType::LEVEL:
		break;
	case MessageType::SET_LEVEL:
		break;
	case MessageType::MOVE_TO_LEVEL:
		if (editMessage1)
			message.value.f = clamp(float(message.value.f) + delta * 0.05f, 0.0f, 1.0f);
		if (editMessage2)
			message.transition = clamp(message.transition + delta, 0, 65535);
		stream << underline(flt(message.value.f, 2), editMessage1) << ' ';

		// transition time in 1/10 s
		stream << underline(dec(message.transition / 10) + '.' + dec(message.transition % 10), editMessage2) << 's';
		break;
	case MessageType::AIR_TEMPERATURE:
		if (editMessage1)
			message.value.f = stepTemperature(message.value.f, delta);
		stream << underline(displayTemperature(message.value.f), editMessage1) << getTemperatureUnit();
		break;
	case MessageType::SET_AIR_TEMPERATURE:
		break;
	case MessageType::AIR_PRESSURE:
		if (editMessage1)
			message.value.f = message.value.f + delta;
		stream << underline(flt(message.value.f * 0.01f, 0), editMessage1) << "hPa";
		break;
	}*/
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
				value += getDefaultFloat(dstUsage);
			if (relative && !lastRelative)
				value -= getDefaultFloat(dstUsage);
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

void RoomControl::editConvertOptions(Menu &menu, ConvertOptions &convertOptions, MessageType2 dstType, MessageType2 srcType) {
	auto dstUsage = getUsage(dstType);
	auto srcUsage = getUsage(srcType);

	// check if destination is a switch
	if ((dstType & MessageType2::CATEGORY) == MessageType2::SWITCH) {
		auto dstStates = getSwitchStates(dstUsage);

		switch (srcType & MessageType2::CATEGORY) {
		case MessageType2::SWITCH:
			// both are a switch
			editConvertSwitch2Switch(menu, convertOptions, dstStates, getSwitchStates(srcUsage));
			return;
		case MessageType2::LEVEL:
		case MessageType2::PHYSICAL:
		case MessageType2::CONCENTRATION:
		case MessageType2::LIGHTING:
			editConvertFloat2Switch(menu, convertOptions, dstStates, srcUsage);
			return;
		case MessageType2::METERING:
			// todo
			return;
		default:;
		}
	}

	// check if source is a switch
	if ((srcType & MessageType2::CATEGORY) == MessageType2::SWITCH) {
		switch (dstType & MessageType2::CATEGORY) {
		case MessageType2::LEVEL:
		case MessageType2::PHYSICAL:
		case MessageType2::CONCENTRATION:
		case MessageType2::LIGHTING:
			// source is a switch and destination is a value (with command)
			editConvertSwitch2FloatCommand(menu, convertOptions, dstUsage, getSwitchStates(srcUsage));

			return;
		default:;
		}
	}

/*
	// check if both are a switch command
	auto d = uint(dstType) - uint(MessageType::OFF_ON_IN);
	auto s = uint(srcType) - uint(MessageType::OFF_ON_OUT);
	bool dstIsCommand = d < SWITCH_COMMAND_COUNT;
	bool srcIsCommand = s < SWITCH_COMMAND_COUNT;
	if (dstIsCommand && srcIsCommand) {
		editConvertSwitch2Switch(menu, convertOptions, switchCommands[d], switchCommands[s]);
		return;
	}

	// check if source is a switch command and destination is a set command
	if (srcIsCommand) {
		switch (dstType) {
		case MessageType::SET_LEVEL_IN:
			editConvertSwitch2SetFloat(menu, convertOptions, switchCommands[s], 1.0f, 0.0f, 100.0f, -2, 1.0f, String());
			return;
		case MessageType::MOVE_TO_LEVEL_IN:
			return;
		case MessageType::SET_AIR_TEMPERATURE_IN:
			editConvertSwitch2SetFloat(menu, convertOptions, switchCommands[s], 1.0f, -273.15f, 2.0f, -1, 10000.0f, "oC");
			return;
		default:;
		}
	}

	// check if source is a simple value and destination is a switch command
	if (dstIsCommand) {
		switch (srcType) {
		case MessageType::LEVEL_OUT:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, 0.0f, 100.0f, -2, 1.0f, String());
			return;
		case MessageType::AIR_TEMPERATURE_OUT:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, -273.15f, 2.0f, -1, 10000.0f, "oC");
			return;
		case MessageType::AIR_HUMIDITY_OUT:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 100.0f, 0.0f, 1.0f, 0, 1.0f, "%");
			return;
		case MessageType::AIR_PRESSURE_OUT:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 0.01f, 0.0f, 1.0f, 0, 2000.0f * 100.0f, "hPa");
			return;
		case MessageType::AIR_VOC_OUT:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, 0.0f, 0.1f, 1, 100.0f, "O");
			return;
		default:;
		}
	}*/
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
			if ((messageType & MessageType2::DIRECTION_MASK) == MessageType2::IN)
				stream << " In";
			if ((messageType & MessageType2::DIRECTION_MASK) == MessageType2::OUT)
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
		MessageInfo2 info;
		Message2 message;
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
			switch (event.info.type & MessageType2::CATEGORY) {
			case MessageType2::SWITCH:
				stream << getSwitchStates(usage)[event.message.value.u8];
				break;
			case MessageType2::LEVEL:
			case MessageType2::PHYSICAL:
			case MessageType2::CONCENTRATION:
			case MessageType2::LIGHTING: {
				bool relative = (event.info.type & MessageType2::CMD) != 0 && event.message.command != 0;
				stream << getDisplayValue(usage, relative, event.message.value.f) << getDisplayUnit(usage);
				break;
			}
			case MessageType2::METERING:
				//stream << getDisplayValue(usage, event.message.value.u32) << getDisplayUnit(usage);
				break;
			default:;
			}

/*
			switch (event.info.type & MessageType::TYPE_MASK) {
			case MessageType::OFF_ON:
				stream << commandNames[onOffCommands[event.message.command]];
				break;
			case MessageType::OFF_ON_TOGGLE:
				stream << commandNames[onOffToggleCommands[event.message.command]];
				break;
			case MessageType::TRIGGER:
				stream << commandNames[triggerCommands[event.message.command]];
				break;
			case MessageType::UP_DOWN:
				stream << commandNames[upDownCommands[event.message.command]];
				break;
			case MessageType::OPEN_CLOSE:
				stream << commandNames[openCloseCommands[event.message.command]];
				break;
			case MessageType::AIR_TEMPERATURE:
				// todo: units
				stream << flt(event.message.value.f - 273.15f, 1) << "oC";
				break;
			case MessageType::AIR_PRESSURE:
				// todo: units
				stream << flt(event.message.value.f * 0.01, 1) << "hPa";
				break;
			default:;
			}*/
			menu.entry();
		}
		
		if (menu.entry("Exit"))
			break;

		// update subscribers
		auto endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < min(endpoints.count(), array::count(subscribers)); ++endpointIndex) {
			auto &subscriber = subscribers[endpointIndex];
			auto messageType = endpoints[endpointIndex];
			subscriber.destination.type = messageType ^ (MessageType2::OUT ^ MessageType2::IN);
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
	MessageType2 lastMessageType = MessageType2::UNKNOWN;
	Message2 message = {};
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
				switch (messageType & MessageType2::CATEGORY) {
				case MessageType2::SWITCH:
					message.value.u8 = 1;
					break;
				case MessageType2::LEVEL:
				case MessageType2::PHYSICAL:
				case MessageType2::CONCENTRATION:
				case MessageType2::LIGHTING:
					message.value.f = getDefaultFloat(usage);
					break;
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
					auto &dst = *reinterpret_cast<Message2 *>(p.message);
					if ((messageType & MessageType2::CATEGORY) == MessageType2::SWITCH)
						dst.value.u8 = message.value.u8;
					else if ((messageType & MessageType2::CMD) == 0)
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
		co_await select(menu.show(), Timer::sleep(250ms));
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
						if (plug.messageType == MessageType2::SWITCH_TERNARY_OPEN_CLOSE_OUT /*UP_DOWN_OUT*/ && !connection.isMqtt()) {
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
				auto &dst = *reinterpret_cast<Message2 *>(p.message);
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


/*

static void editConvertOptionsValue2Command(Menu &menu, ConvertOptions &convertOptions,
	Array<uint8_t const> dstCommands)
{
	int delta = menu.getDelta();

	// one menu entry for greater, equals, less
	for (int i = 0; i < 3; ++i) {
		auto stream = menu.stream();

		// source command
		stream << commandNames[compareCommands[i]] << " -> ";

		// edit destination command
		int offset = i * 3;
		int command = (convertOptions.commands >> offset) & 7;
		bool editCommand = menu.getEdit(1) == 1;
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

		stream << underline(command < commandCount ? commandNames[dstCommands[command]] : "(ignore)", editCommand);
		menu.entry();


	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << array::count(compareCommands) * 3;
}

// edit convert optons for command messages (e.g. OFF_ON, UP_DOWN)
static void editConvertOptionsCommand(Menu &menu, ConvertOptions &convertOptions, Array<uint8_t const> dstCommands,
	MessageType srcType)
{
	switch (srcType & MessageType::TYPE_MASK) {
	case MessageType::OFF_ON:
		editConvertOptionsCommand2Command(menu, convertOptions, dstCommands, onOffCommands);
		break;
	case MessageType::OFF_ON_TOGGLE:
		editConvertOptionsCommand2Command(menu, convertOptions, dstCommands, onOffToggleCommands);
		break;
	case MessageType::TRIGGER:
		editConvertOptionsCommand2Command(menu, convertOptions, dstCommands, triggerCommands);
		break;
	case MessageType::UP_DOWN:
		editConvertOptionsCommand2Command(menu, convertOptions, dstCommands, upDownCommands);
		break;
	case MessageType::OPEN_CLOSE:
		editConvertOptionsCommand2Command(menu, convertOptions, dstCommands, openCloseCommands);
		break;
	case MessageType::AIR_TEMPERATURE:
		editConvertOptionsValue2Command(menu, convertOptions, dstCommands);
		break;
	}
}

static bool hasSetCommand(uint16_t commands) {
	for (int i = 0; i < 4; ++i) {
		if ((commands & 7) == 0)
			return true;
		commands >>= 3;
	}
	return false;
}

static bool hasIncDecCommand(uint16_t commands) {
	for (int i = 0; i < 4; ++i) {
		int c = commands & 7;
		if (c == 1 || c == 2)
			return true;
		commands >>= 3;
	}
	return false;
}

// edit convert options for "set float" messages (e.g. SET_LEVEL, SET_TEMPERATURE)
static void editConvertOptionsSetFloat(Menu &menu, ConvertOptions &convertOptions,
	MessageType srcType, float scale, float offset, float precision, float maxValue, int decimalCount, String unit)
{
	// edit convert options for set command (set, increase, decrease)
	editConvertOptionsCommand(menu, convertOptions, setCommands, srcType);

	// absolute value
	int delta = menu.getDelta();
	if (hasSetCommand(convertOptions.commands)) {
		bool editLevel = menu.getEdit(1);
		if (editLevel)
			convertOptions.value.f[0] = clamp(float(iround((convertOptions.value.f[0] - offset) * precision) + delta)
				/ precision + offset, 0.0f, maxValue);
		menu.stream() << "Set: "
			<< underline(flt(convertOptions.value.f[0] * scale - offset, decimalCount), editLevel)
			<< ' ' << unit;
		menu.entry();
	}

	// relative value
	if (hasIncDecCommand(convertOptions.commands)) {
		bool editLevel = menu.getEdit(1);
		if (editLevel)
			convertOptions.value.f[1] = clamp(float(iround((convertOptions.value.f[1]) * precision) + delta)
				/ precision, 0.0f, maxValue);
		menu.stream() << "Inc/Dec: "
			<< underline(flt(convertOptions.value.f[1] * scale, decimalCount), editLevel)
			<< ' ' << unit;
		menu.entry();
	}
}
*/

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

	void apply(Message2 const &message) {
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
		MessageInfo2 info;
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
		MessageInfo2 info;
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
		MessageInfo2 info;
		Message2 message;
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
		MessageInfo2 info;
		Message2 message;
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

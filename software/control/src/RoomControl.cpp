#include "RoomControl.hpp"
#include "Menu.hpp"
#include "Message.hpp"
#include <Calendar.hpp>
#include <Input.hpp>
#include <Random.hpp>
#include <Radio.hpp>
#include <Output.hpp>
#include <Storage.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <crypt.hpp>
#include <Queue.hpp>
#include "tahoma_8pt.hpp" // font
#include <cmath>


constexpr String weekdaysLong[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr String weekdaysShort[7] = {"M", "T", "W", "T", "F", "S", "S"};

static String const releasedPressed[] = {"released", "pressed"};
static String const offOnToggle[] = {"off", "on", "toggle"};
static String const closedOpenToggle[] = {"close", "open", "toggle"};
static String const lockedToggle[] = {"unlocked", "locked", "toggle"};
static String const occupancy[] = {"unoccupied", "occupied"};
static String const activation[] = {"deactivated", "activated"};
static String const enabled[] = {"disabled", "enabled"};
static String const sound[] = {"stop", "play"};

static String const offOn1On2[] = {"off", "on1", "on2"};
static String const releasedUpDown[] = {"released", "up", "down"};
static String const stoppedOpeningClosing[] = {"stopped", "opening", "closing"};
static String const unlockedTiltLocked[] = {"unlocked", "tilt", "locked"};

static String const compareOperators[] = {">", "<", "else"};
static String const setStep[] = {"set", "step"};

// https://www.nixsensor.com/blog/what-is-hsl-color/
static String const colorNames[] = {
	"Red",
	"Warm Red",
	"Orange",
	"Warm Yellow",
	"Yellow",
	"Cool Yellow",
	"Yellow Green",
	"Warm Green",
	"Green",
	"Cool Green",
	"Green Cyan",
	"Warm Cyan",
	"Cyan",
	"Cool Cyan",
	"Blue Cyan",
	"Cool Blue",
	"Blue",
	"Warm Blue",
	"Violet",
	"Cool Magenta",
	"Magenta",
	"Warm Magenta",
	"Red Magenta",
	"Cool Red",
	"Red"
};


// functions
/*
// function plugs
static RoomControl::Plug const switchPlugs[] = {
	{"On/Off", MessageType::BINARY_POWER_CMD_IN},
	{"On/Off", MessageType::BINARY_POWER_OUT}};
static RoomControl::Plug const lightPlugs[] = {
	{"On/Off", MessageType::BINARY_POWER_LIGHT_CMD_IN},
	{"On/Off", MessageType::BINARY_POWER_LIGHT_OUT},
	{"Brightness", MessageType::LIGHTING_BRIGHTNESS_CMD_OUT}};
static RoomControl::Plug const colorLightPlugs[] = {
	{"On/Off", MessageType::BINARY_POWER_LIGHT_CMD_IN},
	{"On/Off", MessageType::BINARY_POWER_LIGHT_OUT},
	{"Brightness", MessageType::LIGHTING_BRIGHTNESS_CMD_OUT},
	{"Color X", MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X_CMD_OUT},
	{"Color Y", MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y_CMD_OUT}};
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
*/


// RoomControl
// -----------

RoomControl::RoomControl()
	: //storage(0, FLASH_PAGE_COUNT, functions),
	stateManager()
	//, houseTopicId(), roomTopicId()
	, busInterface(stateManager)
	, radioInterface(stateManager)
	, interfaces{&localInterface, &busInterface, &radioInterface, &alarmInterface, &functionInterface}
{
	// load configuration
	if (Storage::read(STORAGE_CONFIG, 0, sizeof(Configuration), &this->configuration) != sizeof(Configuration)) {
		auto &configuration = this->configuration;

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
		Storage::write(STORAGE_CONFIG, 0, sizeof(Configuration), &configuration);
	}

	applyConfiguration();

	// load connections
	for (uint8_t interfaceIndex = 0; interfaceIndex < INTERFACE_COUNT; ++interfaceIndex) {
		auto &interface = *this->interfaces[interfaceIndex];

		// iterate over all devices in the interface
		auto deviceIds = interface.getDeviceIds();
		for (auto deviceId : deviceIds) {
			int const storageId = STORAGE_ID_CONNECTION | (interfaceIndex << 8) | deviceId;

			// determine size
			int size = Storage::size(STORAGE_CONFIG, storageId);
			uint8_t count = size / sizeof(ConnectionData);
			if (count == 0)
				continue;

			// allocate and read connection data
			auto connectionData = reinterpret_cast<ConnectionData *>(malloc(size));
			Storage::read(STORAGE_CONFIG, storageId, size, connectionData);

			// allocate subscribers
			auto subscribers = new Subscriber[count];

			// create new connection group for the device
			auto connections = new Connections{this->connectionGroups, interfaceIndex, deviceId, count, connectionData, subscribers};
			this->connectionGroups = connections;

			connect(connections);
		}
	}

	// start coroutines
	idleDisplay();
}

RoomControl::~RoomControl() {
}

void RoomControl::applyConfiguration() {
	auto const &configuration = this->configuration;
	this->busInterface.setConfiguration(configuration.key, configuration.aesKey);
	Radio::setLongAddress(configuration.ieeeLongAddress);
	this->radioInterface.setConfiguration(configuration.zbeePanId, configuration.key, configuration.aesKey);
}


// Interfaces
// ----------

static String getInterfaceName(int interface) {
	switch (interface) {
	case RoomControl::LOCAL_INTERFACE:
		return "Local";
	case RoomControl::BUS_INTERFACE:
		return "Bus";
	case RoomControl::RADIO_INTERFACE:
		return "Radio";
	case RoomControl::ALARM_INTERFACE:
		return "Alarm";
	default:
		return "MQTT";
	}
}


// Connections
// -----------

void RoomControl::connect(Connections *connections) {
	auto &interface = *this->interfaces[connections->interfaceIndex];
	uint8_t lastPlugIndex = 0;
	uint8_t connectionIndex = 0;
	for (int i = 0; i < connections->count; ++i) {
		auto data = &connections->data[i];
		auto &subscriber = connections->subscribers[i];

		// reset connectionIndex if connection is for the next plug
		if (data->destination.plugIndex != lastPlugIndex) {
			lastPlugIndex = data->destination.plugIndex;
			connectionIndex = 0;
		}

		// initialize subscriber
		subscriber.data = data;
		subscriber.deviceId = connections->deviceId;
		subscriber.connectionIndex = connectionIndex;
		subscriber.info = interface.getSubscriberInfo(connections->deviceId, data->destination.plugIndex);

		// subscribe
		if (data->source.interfaceIndex < INTERFACE_COUNT) {
			auto &sourceInterface = *this->interfaces[data->source.interfaceIndex];
			sourceInterface.subscribe(subscriber);
		}
	}
}

RoomControl::TempConnections RoomControl::getConnections(uint8_t interfaceIndex, uint8_t deviceId) {
	TempConnections tc;
	auto p = &this->connectionGroups;
	while (*p != nullptr) {
		auto connections = *p;
		if (connections->interfaceIndex == interfaceIndex && connections->deviceId == deviceId) {
			tc.p = p;
			tc.count = min(connections->count, TempConnections::MAX_CONNECTIONS);

			// copy connection data
			for (int i = 0; i < tc.count; ++i) {
				tc.data[i] = connections->data[i];
			}
			break;
		}
		p = &connections->next;
	}
	return tc;
}

void RoomControl::writeConnections(uint8_t interfaceIndex, uint8_t deviceId, TempConnections &tc) {
	auto count = tc.count;
	int size = count * sizeof(ConnectionData);

	Connections *connections;
	if (tc.p == nullptr) {
		// new connections

		// allocate and connection data
		auto connectionData = reinterpret_cast<ConnectionData *>(malloc(size));

		// allocate subscribers
		auto subscribers = new Subscriber[count];

		// create new connections
		connections = new Connections{this->connectionGroups, interfaceIndex, deviceId, count, connectionData, subscribers};
		this->connectionGroups = connections;
	} else {
		// existing connections
		connections = *tc.p;

		if (count != connections->count) {
			connections->count = count;

			// reallocate connection data
			connections->data = reinterpret_cast<ConnectionData *>(realloc(connections->data, size));

			// reallocate subscribers
			delete [] connections->subscribers;
			connections->subscribers = new Subscriber[count];
		}
	}

	// copy connection data
	for (int i = 0; i < count; ++i) {
		connections->data[i] = tc.data[i];
	}

	// connect
	connect(connections);

	// write to flash
	int const storageId = STORAGE_ID_CONNECTION | (interfaceIndex << 8) | deviceId;
	Storage::write(STORAGE_CONFIG, storageId, size, connections->data);
}

void RoomControl::eraseConnections(uint8_t interfaceIndex, uint8_t deviceId, TempConnections &tc) {
	auto connections = *tc.p;

	// remove from linked list
	*tc.p = connections->next;

	// delete connection data
	free(connections->data);

	// delete subscribers (automatically unsubscribe)
	delete [] connections->subscribers;

	// erase from flash
	int const storageId = STORAGE_ID_CONNECTION | (interfaceIndex << 8) | deviceId;
	Storage::erase(STORAGE_CONFIG, storageId);
}

void RoomControl::printConnection(Menu::Stream &stream, ConnectionData const &connection) {
	if (connection.source.interfaceIndex < array::count(this->interfaces)) {
		auto ifc = this->interfaces[connection.source.interfaceIndex];
		if (ifc != nullptr) {
			auto &interface = *ifc;
			auto plugs = interface.getPlugs(connection.source.deviceId);
			if (connection.source.plugIndex < plugs.count()) {
				String deviceName = interface.getName(connection.source.deviceId);
				stream << deviceName << '/' << dec(connection.source.plugIndex);
				String plugName = getTypeLabel(plugs[connection.source.plugIndex]);
				stream << " (" << plugName << ')';
				return;
			}
		}
	}
	stream << "Connect...";
}


// Menu Helpers
// ------------

// print all devices in an interface
static void printDevices(Interface &interface) {
	auto deviceIds = interface.getDeviceIds();
	for (auto id : deviceIds) {
		Terminal::out << dec(id) + '\n';
		auto plugs = interface.getPlugs(id);
		for (auto plug : plugs) {
			Terminal::out << '\t' << getTypeLabel(plug);
			if ((plug & MessageType::DIRECTION_MASK) == MessageType::IN)
				Terminal::out << " In";
			if ((plug & MessageType::DIRECTION_MASK) == MessageType::OUT)
				Terminal::out << " Out";
			Terminal::out << '\n';
		}
	}
}

Array<String const> RoomControl::getSwitchStates(Usage usage) {
	switch (usage) {
	// binary
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
	case Usage::SOUND:
		return sound;

	// ternary
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

/*
	number of different switch types for default convert options
	0: off/on
	1: off/on1/on2
	2: off/on/toggle
	3: release/press or stop/play
	4: release/up/down
*/
constexpr int SWITCH_TYPE_COUNT = 5;

int RoomControl::getSwitchType(MessageType type) {
	auto category = type & MessageType::CATEGORY;
	if (category == MessageType::TERNARY) {
		// off/on1/on2 or release/up/down
		return (type & MessageType::TERNARY_CATEGORY) == MessageType ::TERNARY_BUTTON ? 4 : 1;
	}
	auto binaryCategory = type & MessageType::BINARY_CATEGORY;
	if (binaryCategory == MessageType::BINARY_BUTTON
		|| binaryCategory == MessageType::BINARY_ALARM
		|| binaryCategory == MessageType::BINARY_SOUND)
	{
		// release/press or stop/play
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
constexpr int IGNORE = 7;

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
		convert(IGNORE, PRESS), // release/press
		convert(IGNORE, UP), // release/up/down
	},
	// off/on1/on2 ->
	{
		convert(OFF, ON, IGNORE), // off/on
		convert(OFF, ON1, ON2), // off/on1/on2
		convert(OFF, ON, IGNORE), // off/on/toggle
		convert(IGNORE, PRESS, IGNORE), // release/press
		convert(IGNORE, UP, IGNORE), // release/up/down
	},
	// off/on/toggle ->
	{
		convert(OFF, ON, IGNORE), // off/on
		convert(OFF, ON1, IGNORE), // off/on1/on2
		convert(OFF, ON, TOGGLE), // off/on/toggle
		convert(IGNORE, IGNORE, PRESS), // release/press
		convert(IGNORE, IGNORE, UP), // release/up/down
	},
	// release/press ->
	{
		convert(IGNORE, ON), // off/on
		convert(IGNORE, ON1), // off/on1/on2
		convert(IGNORE, TOGGLE), // off/on/toggle
		convert(RELEASE, PRESS), // release/press
		convert(RELEASE, UP), // release/up/down
	},
	// release/up/down ->
	{
		convert(IGNORE, ON, OFF), // off/on
		convert(OFF, ON1, ON2), // off/on1/on2
		convert(IGNORE, ON, OFF), // off/on/toggle
		convert(RELEASE, PRESS, IGNORE), // release/press
		convert(RELEASE, UP, DOWN), // release/up/down
	},
};

// default switch to value command convert
static uint16_t const switch2valueCommand[SWITCH_TYPE_COUNT] = {
	convert(IGNORE, SET), // off/on -> set/increase/decrease
	convert(IGNORE, SET, IGNORE), // off/on1/on2 -> set/increase/decrease
	convert(IGNORE, SET, IGNORE), // off/on/toggle -> set/increase/decrease
	convert(IGNORE, SET), // release/press -> set/increase/decrease
	convert(IGNORE, INCREASE, DECREASE), // release/up/down -> set/increase/decrease
};

// default value command to switch convert
static uint16_t const value2switch[SWITCH_TYPE_COUNT] = {
	convert(ON, OFF, IGNORE), // greater/less/else -> off/on
	convert(ON, OFF, IGNORE), // greater/less/else -> off/on/toggle
	convert(PRESS, RELEASE, IGNORE), // greater/less/else -> release/press
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

static float step(float value, int delta, float increment, float lo, float hi, bool relative, float range) {
	return step(value, delta, increment, relative ? -range : lo, relative ? range : hi);
}

static float stepCelsius(float value, int delta, float increment, float lo, float hi, bool relative, float range) {
	if (relative)
		return step(value, delta, increment, -range, range);
	return step(value - CELSIUS_OFFSET, delta, increment, lo, hi) + CELSIUS_OFFSET;
}

float RoomControl::stepValue(Usage usage, bool relative, float value, int delta) {
	switch (usage) {
	case Usage::UNIT_INTERVAL:
	case Usage::PERCENT:
		return step(value, delta, 0.01f, 0.0f, 1.0f, relative, 1.0f);
	case Usage::TEMPERATURE:
		return stepCelsius(value, delta, 1.0f, -273.0f, 5000.0f, relative, 100.0f);
	case Usage::TEMPERATURE_FREEZER:
		return stepCelsius(value, delta, 0.5f, -30.0f, 0.0f, relative, 10.0f);
	case Usage::TEMPERATURE_FRIDGE:
		return stepCelsius(value, delta, 0.5f, 0.0f, 20.0f, relative, 10.0f);
	case Usage::TEMPERATURE_OUTDOOR:
		return stepCelsius(value, delta, 1.0f, -50.0f, 60.0f, relative, 10.0f);
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
	case MessageType::ENUM: {
		// enumeration
		uint count = uint(messageType & (MessageType::TYPE_MASK & ~MessageType::CATEGORY));
		if (editMessage1)
			message.value.u16 = uint32_t(message.value.u16 + count * 32 + delta) % count;
		stream << underline(dec(message.value.u16), editMessage1);
		break;
	}
	case MessageType::LEVEL:
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION:
		// float
		if (editMessage1)
			message.value.f32 = stepValue(usage, false, message.value.f32, delta);
		stream << underline(getDisplayValue(usage, false, message.value.f32), editMessage1) << getDisplayUnit(usage);
		break;
	case MessageType::LIGHTING:
		// float + transition
		if (editMessage1)
			message.value.f32 = stepValue(usage, false, message.value.f32, delta);
		if (editMessage2)
			message.transition = clamp(message.transition + delta, 0, 65535);
		stream << underline(getDisplayValue(usage, false, message.value.f32), editMessage1) << getDisplayUnit(usage) << ' ';

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
		if (command >= commandCount)
			command = commandCount;
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
	static_assert(array::count(setStep) <= ConvertOptions::MAX_VALUE_COUNT);

	int delta = menu.getDelta();

	// one menu entry per source command
	for (int i = 0; i < srcCommands.count(); ++i) {

		auto stream = menu.stream();

		// source command
		stream << srcCommands[i] << " -> ";

		// edit destination command (directly in convertOptions)
		bool editCommand = menu.getEdit(2) == 1;
		bool lastRelative = convertOptions.getCommand(i) != 0;
		int command = editDestinationCommand(stream, convertOptions, i, setStep, editCommand, delta);
		bool relative = command != 0;

		float value = convertOptions.value.f[i];
		if (relative != lastRelative) {
			// command was changed between absolute (set) and relative (increase, decrease)
			if (!relative && lastRelative)
				value += getDefaultFloatValue(dstUsage);
			if (relative && !lastRelative)
				value -= getDefaultFloatValue(dstUsage);
		}

		if (command < array::count(setStep)) {
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

AwaitableCoroutine RoomControl::mainMenu() {
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Local Devices"))
			co_await devicesMenu(LOCAL_INTERFACE);
		if (menu.entry("Bus Devices"))
			co_await devicesMenu(BUS_INTERFACE);
		if (menu.entry("Radio Devices"))
			co_await devicesMenu(RADIO_INTERFACE);
		if (menu.entry("Alarms"))
			co_await alarmsMenu();
		if (menu.entry("Functions"))
			co_await functionsMenu();
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::devicesMenu(int interfaceIndex) {
	auto &interface = *this->interfaces[interfaceIndex];
	interface.setCommissioning(true);
	Menu menu(this->swapChain);
	while (true) {
		// list devices
		auto deviceIds = interface.getDeviceIds();
		for (auto id : deviceIds) {
			auto stream = menu.stream();
			stream << interface.getName(id);
			if (menu.entry()) {
				// set commissioning to false when in deviceMenu to prevent deletion of device
				// todo: "leave" command can still delete the device
				interface.setCommissioning(false);
				co_await deviceMenu(interfaceIndex, id);
				interface.setCommissioning(true);
				break;
			}
		}
		if (menu.entry("Exit"))
			break;
		
		// show menu and wait for new event until timeout so that we can show newly commissioned devices
		co_await select(menu.show(), Timer::sleep(250ms));
	}
	interface.setCommissioning(false);
}

AwaitableCoroutine RoomControl::deviceMenu(int interfaceIndex, uint8_t id) {
	auto &interface = *this->interfaces[interfaceIndex];

	// connections
	TempConnections tc = getConnections(interfaceIndex, id);

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Connections"))
			co_await connectionsMenu(interface.getPlugs(id), tc);
		if (menu.entry("Plugs"))
			co_await plugsMenu(interface, id);
		if (menu.entry("Message Logger"))
			co_await messageLogger(interface, id);
		if (menu.entry("Message Generator"))
			co_await messageGenerator(interface, id);
		if (menu.entry("Delete")) {
			interface.erase(id);
			break;
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("Save")) {
			// save connections
			writeConnections(interfaceIndex, id, tc);
			break;
		}
		//if (menu.entry("Exit"))
		//	break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::alarmsMenu() {
	Menu menu(this->swapChain);
	auto &alarms = this->alarmInterface;
	while (true) {
		// list alarms
		auto ids = alarms.getDeviceIds();
		for (auto id : ids) {
			auto const &data = *alarms.get(id);
			auto stream = menu.stream();

			// time
			auto time = data.time;
			stream << dec(time.getHours()) << ':' << dec(time.getMinutes(), 2);

			// week days


			//stream << interface.getDeviceName(i);
			if (menu.entry()) {
				AlarmInterface::Data data2(data);
				co_await alarmMenu(data2);
				break;
			}
		}
		if (menu.entry("Add")) {
			AlarmInterface::Data data;
			data.id = 0;
			assign(data.name, "Alarm");
			co_await alarmMenu(data);
		}
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::alarmMenu(AlarmInterface::Data &data) {
	Menu menu(this->swapChain);
	auto &alarms = this->alarmInterface;
	auto id = data.id;
	while (true) {
		if (menu.entry("Time"))
			co_await alarmTimeMenu(data.time);

		// check if alarm already exists
		if (id != 0) {
			if (menu.entry("Message Logger"))
				co_await messageLogger(alarms, id);

			// test alarm
			menu.stream() << "Test Trigger (-> " << dec(alarms.getSubscriberCount(id,  1)) << ')';
			if (menu.entry())
				alarms.test(id, 1);
			menu.stream() << "Test Release (-> " << dec(alarms.getSubscriberCount(id,  0)) << ')';
			if (menu.entry())
				alarms.test(id, 0);

			if (menu.entry("Delete")) {
				// delete alarm
				alarms.erase(id);
				break;
			}
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("Save")) {
			// set alarm (id=0 creates new alarm)
			alarms.set(id, data);
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

AwaitableCoroutine RoomControl::functionsMenu() {
	// menu loop
	Menu menu(this->swapChain);
	auto &functions = this->functionInterface;
	while (true) {
		// list functions
		auto deviceIds = functions.getDeviceIds();
		for (auto id : deviceIds) {
			auto stream = menu.stream();
			stream << functions.getName(id);
			if (menu.entry()) {
				FunctionInterface::DataUnion data;
				functions.getData(id, data);
				co_await functionMenu(data);
				break;
			}
		}
		if (menu.entry("Add")) {
			FunctionInterface::DataUnion data;
			data.data.id = 0;
			data.data.type = FunctionInterface::Type::SWITCH;
			assign(data.data.name, "Function");
			data.switchData.timeout = 0;
			co_await functionMenu(data);
		}
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

static void editDuration(Menu &menu, Menu::Stream &stream, uint16_t &duration, int resolution) {
	auto delta = menu.getDelta();
	auto d = unpackMinuteDuration(duration, resolution);

	// edit duration
	int edit = menu.getEdit(3);
	bool editMinutes = edit == 1;
	bool editSeconds = edit == 2;
	bool editFraction = edit == 3;
	if (editMinutes)
		d.minutes = clamp(d.minutes + delta, 0, 99);
	if (editSeconds)
		d.seconds = clamp(d.seconds + delta, 0, 59);
	if (editFraction)
		d.fraction = clamp(d.fraction + delta, 0, resolution - 1);

	duration = packMinuteDuration(d, resolution);

	// duration
	stream
		<< underline(dec(d.minutes, 2), editMinutes) << ":"
		<< underline(dec(d.seconds, 2), editSeconds) << "."
		<< underline(dec(d.fraction, resolution > 10 ? 2 : 1), editFraction);
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
		d.hours = clamp(d.hours + delta, 0, 255);
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
		<< underline(dec(d.hundredths, 2), editHundredths);
	menu.entry();
}

static void editColorSettings(Menu &menu, int settingCount, FunctionInterface::ColorSetting *settings, bool on) {
	auto delta = menu.getDelta();

	for (int i = 0; i < settingCount; ++i) {
		menu.beginSection();
		auto &setting = settings[i];

		// brightness
		auto stream = menu.stream();
		stream << "Brightness: ";
		int editBrightness = menu.getEdit(1);
		if (editBrightness)
			setting.brightness = clamp(setting.brightness + delta, 0, 100);
		stream << underline(dec(setting.brightness), editBrightness) << '%';
		menu.entry();

		// color
		stream = menu.stream();
		stream << "Hue: ";
		int editHue = menu.getEdit(1);
		if (editHue)
			setting.hue = (setting.hue + 72 + delta) % 72;
		stream << underline(dec(setting.hue * 5), editHue);
		stream << ' ' << colorNames[((setting.hue + 1) / 3) % 24];
		menu.entry();
		stream = menu.stream();
		stream << "Saturation: ";
		int editSaturation = menu.getEdit(1);
		if (editSaturation)
			setting.saturation = clamp(setting.saturation + delta, 0, 100);
		stream << underline(dec(setting.saturation), editSaturation) << '%';
		menu.entry();

		// fade time
		stream = menu.stream();
		if (on)
			stream << "On ";
		stream << "Fade: ";
		editDuration(menu, stream, setting.fade, 10);

		menu.endSection();
	}
}

AwaitableCoroutine RoomControl::functionMenu(FunctionInterface::DataUnion &data) {
	// function (interface and id)
	auto &functions = this->functionInterface;
	auto id = data.data.id;

	// connections
	TempConnections tc = getConnections(FUNCTION_INTERFACE, id);

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// todo edit name

		// function type
		{
			bool editType = menu.getEdit(1) == 1;
			if (editType && delta != 0)
				FunctionInterface::setType(data, FunctionInterface::getNextType(data.data.type, delta));

			auto stream = menu.stream();
			stream << "Type: " << underline(FunctionInterface::getName(data.data.type), editType);
			menu.entry();
		}

		// type dependent configuration
		switch (data.data.type) {
		case FunctionInterface::Type::SWITCH: {
			auto &d = data.switchData;
			auto stream = menu.stream();
			stream << "Timeout: ";
			editDuration(menu, stream, d.timeout);
			break;
		}
		case FunctionInterface::Type::LIGHT: {
			auto &d = data.lightData;

			// timeout
			auto stream = menu.stream();
			stream << "Timeout: ";
			editDuration(menu, stream, d.timeout);

			// brightness
			// todo: one per input
			for (int i = 0; i < 4; ++i) {
				auto &setting = d.settings[i];

				stream = menu.stream();
				stream << "Brightness: ";
				int editBrightness = menu.getEdit(1);
				if (editBrightness)
					setting.brightness = clamp(setting.brightness + delta, 0, 100);
				stream << underline(dec(setting.brightness), editBrightness) << '%';
				menu.entry();

				// on fade time
				stream = menu.stream();
				stream << "On Fade: ";
				editDuration(menu, stream, setting.fade, 10);
			}

			// off and timeout fade times
			stream = menu.stream();
			stream << "Off Fade: ";
			editDuration(menu, stream, d.offFade, 10);

			stream = menu.stream();
			stream << "Timeout Fade: ";
			editDuration(menu, stream, d.timeoutFade, 10);

			break;
		}
		case FunctionInterface::Type::COLOR_LIGHT: {
			auto &d = data.colorLightData;

			// timeout
			auto stream = menu.stream();
			stream << "Timeout: ";
			editDuration(menu, stream, d.timeout);

			// todo: one per input
			editColorSettings(menu, 4, d.settings, true);

			// off and timeout fade times
			stream = menu.stream();
			stream << "Off Fade: ";
			editDuration(menu, stream, d.offFade, 10);

			stream = menu.stream();
			stream << "Timeout Fade: ";
			editDuration(menu, stream, d.timeoutFade, 10);

			break;
		}
		case FunctionInterface::Type::ANIMATED_LIGHT: {
			auto &d = data.animatedLightData;

			// timeout
			auto stream = menu.stream();
			stream << "Timeout: ";
			editDuration(menu, stream, d.timeout);

			// animation step count
			stream = menu.stream();
			stream << "Step Count: ";
			int editStepCount = menu.getEdit(1);
			if (editStepCount)
				d.stepCount = clamp(d.stepCount + delta, 0, 15);
			stream << underline(dec(d.stepCount), editStepCount);
			menu.entry();

			// animation steps
			editColorSettings(menu, d.stepCount, d.steps, false);

			// fade times
			stream = menu.stream();
			stream << "On Fade: ";
			editDuration(menu, stream, d.onFade, 10);

			stream = menu.stream();
			stream << "Off Fade: ";
			editDuration(menu, stream, d.offFade, 10);

			stream = menu.stream();
			stream << "Timeout Fade: ";
			editDuration(menu, stream, d.timeoutFade, 10);

			break;
		}
		case FunctionInterface::Type::TIMED_BLIND: {
			auto &d = data.timedBlindData;

			// minimum rocker hold time to cause switch-off on release
			auto stream1 = menu.stream();
			stream1 << "Hold Time: ";
			editDuration(menu, stream1, d.holdTime, 100);

			// run time from fully closed to fully open
			auto stream2 = menu.stream();
			stream2 << "Run Time: ";
			editDuration(menu, stream2, d.runTime, 100);

			if (id != 0 && menu.entry("Measure Run Time")) {
				co_await measureRunTime(id, 4, d.runTime);
			}
		}
		default:
			;
		}

		if (menu.entry("Connections"))
			co_await connectionsMenu(FunctionInterface::getPlugs(data.data.type), tc);

		// check if function already exists
		if (id != 0) {
			if (menu.entry("Plugs"))
				co_await plugsMenu(functions, id);
			if (menu.entry("Message Logger"))
				co_await messageLogger(functions, id);
			if (menu.entry("Message Generator"))
				co_await messageGenerator(functions, id);
			if (menu.entry("Delete")) {
				// delete connections
				eraseConnections(FUNCTION_INTERFACE, id, tc);

				// delete function
				functions.erase(id);
				break;
			}
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("Save")) {
			// save function
			functions.set(data);

			// save connections
			writeConnections(FUNCTION_INTERFACE, id, tc);

			break;
		}

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::measureRunTime(uint8_t id, uint8_t plugIndex, uint16_t &runTime) {
	uint8_t state = 0;
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
				// start
				state = up ? 1 : 2;
				up = !up;
				startTime = Timer::now();
			} else {
				// stop
				state = 0;
			}

			this->functionInterface.publishSwitch(id, plugIndex, state);
		}
		if (menu.entry("Cancel"))
			break;
		if (menu.entry("OK")) {
			runTime = duration;
			break;
		}

		// show menu
		if (state == 0) {
			// stopped:
			co_await menu.show();
		} else {
			// running: timeout so that the duration gets updated on display
			co_await select(menu.show(), Timer::sleep(25ms));

			if (state != 0)
				duration = (Timer::now() - startTime) / 10ms;
		}
	}
}


AwaitableCoroutine RoomControl::connectionsMenu(Array<MessageType const> plugs, TempConnections &tc) {
	Menu menu(this->swapChain);
	while (true) {
		// list all input plugs and their connections
		int i = 0;
		for (int plugIndex = 0; plugIndex < plugs.count(); ++plugIndex) {
			auto dstType = plugs[plugIndex];
			if ((dstType & MessageType::DIRECTION_MASK) != MessageType::IN)
				continue;
			menu.line();
			menu.stream() << getTypeLabel(dstType);
			menu.label();

			// edit existing connections to the current plug
			while (i < tc.count) {
				auto &connection = tc.data[i];
				if (connection.destination.plugIndex != plugIndex)
					break;

				auto stream = menu.stream();
				printConnection(stream, connection);
				if (menu.entry()) {
					co_await editFunctionConnection(tc, i, dstType, connection, false);
				}

				++i;
			}

			// add new connection if maximum connection count not reached
			if (i < TempConnections::MAX_CONNECTIONS && menu.entry("Connect...")) {
				ConnectionData connection = {
					{0, 0, 0},
					{uint8_t(plugIndex)},
					{}};
				co_await editFunctionConnection(tc, i, dstType, connection, true);
			}
		}
		menu.line();
		if (menu.entry("Exit"))
			break;

		// show menu and wait for new event until timeout so that we can show endpoints of recommissioned device
		co_await select(menu.show(), Timer::sleep(250ms));
	}
}

AwaitableCoroutine RoomControl::editFunctionConnection(TempConnections &tc, int index, MessageType dstType,
	ConnectionData connection, bool add)
{
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// get interface index and make sure it is valid
		int i = connection.source.interfaceIndex;
		while (i >= array::count(this->interfaces) || this->interfaces[i]->getDeviceIds().count() == 0) {
			i = i >= array::count(this->interfaces) - 1 ? 0 : i + 1;
		}

		// select interface
		bool editInterface = menu.getEdit(1) == 1;
		if (editInterface && delta != 0) {
			while (delta > 0) {
				i = i >= array::count(this->interfaces) - 1 ? 0 : i + 1;
				if (this->interfaces[i]->getDeviceIds().count() > 0)
					--delta;
			}
			while (delta < 0) {
				i = i == 0 ? array::count(this->interfaces) - 1 : i - 1;
				if (this->interfaces[i]->getDeviceIds().count() > 0)
					++delta;
			}
			connection.source.interfaceIndex = i;
			connection.source.deviceId = 0;
		}
		auto &interface = *this->interfaces[i];

		menu.stream() << "Interface: " << underline(interface.getName(), editInterface);
		menu.entry();

		// select device
		{
			auto stream = menu.stream();
			printConnection(stream, connection);
			if (menu.entry())
				co_await selectFunctionDevice(connection, dstType);
		}

		// select convert options
		auto plugs = interface.getPlugs(connection.source.deviceId);
		if (connection.source.plugIndex < plugs.count()) {
			auto messageType = plugs[connection.source.plugIndex];

			menu.beginSection();
			editConvertOptions(menu, connection.convertOptions, dstType, messageType);
			menu.endSection();
		}

		if (!add) {
			if (menu.entry("Delete")) {
				tc.erase(index);
				break;
			}
		}

		if (menu.entry("Cancel"))
			break;

		if (connection.source.deviceId != 0 && menu.entry("Ok")) {
			if (add)
				tc.insert(index, connection);
			break;
		}

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::selectFunctionDevice(ConnectionData &connection, MessageType dstType) {
	auto &interface = *this->interfaces[connection.source.interfaceIndex];

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// list devices with at least one compatible plug
		auto deviceIds = interface.getDeviceIds();
		for (auto id : deviceIds) {
			// check if the device has at least one compatible endpoint
			auto plugs = interface.getPlugs(id);
			for (auto srcType : plugs) {
				if (isCompatible(dstType, srcType) != 0) {
					if (menu.entry(interface.getName(id))) {
						co_await selectFunctionPlug(interface, id, connection, dstType);
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

AwaitableCoroutine RoomControl::selectFunctionPlug(Interface &interface, uint8_t id, ConnectionData &connection,
	MessageType dstType)
{
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// list all compatible endpoints of device
		auto plugs = interface.getPlugs(id);
		for (int plugIndex = 0; plugIndex < plugs.count(); ++plugIndex) {
			auto srcType = plugs[plugIndex];
			if (isCompatible(dstType, srcType) != 0) {
				// plug is compatible
				String typeName = getTypeLabel(srcType);
				menu.stream() << dec(plugIndex) << " (" << typeName << ')';
				if (menu.entry()) {
					// endpoint selected
					connection.source.deviceId = id;
					connection.source.plugIndex = plugIndex;
					connection.convertOptions = getDefaultConvertOptions(dstType, srcType);
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

AwaitableCoroutine RoomControl::plugsMenu(Interface &interface, uint8_t id) {
	Menu menu(this->swapChain);
	while (true) {
		auto plugs = interface.getPlugs(id);
		for (int i = 0; i < plugs.count(); ++i) {
			auto messageType = plugs[i];
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

AwaitableCoroutine RoomControl::messageLogger(Interface &interface, uint8_t id) {
	MessageBarrier barrier;

	Listener listener;
	listener.sourceIndex = 0; // not used
	listener.barrier = &barrier;
	interface.listen(listener);

	// event queue
	struct Event {
		MessageInfo info;
		MessageType type;
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
			stream << dec(event.info.plugIndex) << ": ";
			auto usage = getUsage(event.type);
			switch (event.type & MessageType::CATEGORY) {
			case MessageType::BINARY:
			case MessageType::TERNARY:
				stream << getSwitchStates(usage)[event.message.value.u8];
				break;
			case MessageType::LEVEL:
			case MessageType::PHYSICAL:
			case MessageType::CONCENTRATION:
			case MessageType::LIGHTING: {
				bool relative = (event.type & MessageType::CMD) != 0 && event.message.command != 0;
				stream << getDisplayValue(usage, relative, event.message.value.f32) << getDisplayUnit(usage);
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

		// get the empty event at the back of the queue
		Event &event = queue.getBack();

		// show menu or receive event (event gets filled in)
		int selected = co_await select(menu.show(), barrier.wait(event.info, &event.message), Timer::sleep(250ms));
		if (selected == 2 && event.info.deviceId == id) {
			// received an event: add new empty event at the back of the queue
			event.type = interface.getPlugs(id)[event.info.plugIndex];
			queue.addBack();
		}
	}
}

static int getComponentCount(MessageType messageType) {
	return (messageType & MessageType::CATEGORY) == MessageType::LIGHTING ? 2 : 1;
}

AwaitableCoroutine RoomControl::messageGenerator(Interface &interface, uint8_t id) {
	//! skip "in" endpoints -> or forward to subscriptions

	uint8_t plugIndex = 0;
	MessageType lastMessageType = MessageType::UNKNOWN;
	Message message = {};
	MessageBarrier *publishBarrier;

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		auto plugs = interface.getPlugs(id);
		if (plugs.count() > 0) {
			// get endpoint type
			plugIndex = clamp(plugIndex, 0, plugs.count() - 1);
			auto messageType = plugs[plugIndex];

			// get message component to edit (some messages such as MOVE_TO_LEVEL have two components such as transition)
			int edit = menu.getEdit(1 + getComponentCount(messageType));
			bool editIndex = edit == 1;
			bool editMessage1 = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			// edit endpoint index
			if (editIndex) {
				plugIndex = clamp(plugIndex + delta, 0, plugs.count() - 1);
				messageType = plugs[plugIndex];
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
				//case MessageType::MULTISTATE:
				case MessageType::ENUM:
					message.value.u16 = 0;
					break;
				case MessageType::LEVEL:
				case MessageType::PHYSICAL:
				case MessageType::CONCENTRATION:
				case MessageType::LIGHTING:
					message.value.f32 = getDefaultFloatValue(usage);
					break;
				default:;
				}
			}

			// edit message
			auto stream = menu.stream();
			stream << underline(dec(plugIndex), editIndex) << ": ";
			editMessage(stream, messageType, message, editMessage1, editMessage2, delta);
			menu.entry();

			// send message
			if (menu.entry("Send")) {
				auto info = interface.getSubscriberInfo(id, plugIndex);
				info.barrier->resumeFirst([id, plugIndex, messageType, &message] (MessageParameters &p) {
					p.info.deviceId = id;
					p.info.plugIndex = plugIndex;
					p.info.sourceIndex = 0;

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

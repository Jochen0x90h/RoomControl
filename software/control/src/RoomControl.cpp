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


using EndpointType = Interface::EndpointType;

constexpr String weekdaysLong[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr String weekdaysShort[7] = {"M", "T", "W", "T", "F", "S", "S"};

//constexpr String switchStatesLong[] = {"off", "on", "toggle", String()};
//constexpr String triggerStatesLong[] = {"inactive", "active"};
//constexpr String upDownStatesLong[] = {"inactive", "up", "down", String()};

static String const onOffCommands[] = {"off", "on", "toggle"};
static String const triggerCommands[] = {"release", "trigger"};
static String const upDownCommands[] = {"release", "up", "down"};

static String getCommandName(EndpointType endpointType, Command command) {
	switch (command) {
		case Command::OFF:
			return "off";
		case Command::ON:
			return "on";
		case Command::TRIGGER:
			return (endpointType & EndpointType::TYPE_MASK) == EndpointType::ON_OFF ? "toggle" : "trigger";
		case Command::UP:
			return "up";
		case Command::DOWN:
			return "down";
		case Command::RELEASE:
			return "release";
	}
	return {};
}


// functions

struct FunctionInfo {
	RoomControl::FunctionFlash::Type type;
	String name;
	Array<RoomControl::PlugInfo const> plugs;
};

constexpr uint8_t onOffFlags = flag(Command::ON) | flag(Command::OFF);
constexpr uint8_t onOffToggleFlags = onOffFlags | flag(Command::TOGGLE);
constexpr uint8_t upDownToggleFlags = flag(Command::RELEASE) | flag(Command::UP) | flag(Command::DOWN) | flag(Command::TOGGLE);

static RoomControl::PlugInfo const switchPlugs[] = {
	{"Switch", RoomControl::PlugInfo::INPUT, MessageType::COMMAND, onOffToggleFlags},
	{"Switch", RoomControl::PlugInfo::OUTPUT, MessageType::ON_OFF}};
static RoomControl::PlugInfo const blindPlugs[] = {
	{"Up/Down", RoomControl::PlugInfo::INPUT, MessageType::COMMAND, upDownToggleFlags},
	{"Position", RoomControl::PlugInfo::INPUT, MessageType::LEVEL},
	{"Up/Down", RoomControl::PlugInfo::OUTPUT, MessageType::UP_DOWN},
	{"Position", RoomControl::PlugInfo::OUTPUT, MessageType::LEVEL}};
static RoomControl::PlugInfo const heatingControlPlugs[] = {
	{"On/Off", RoomControl::PlugInfo::INPUT, MessageType::COMMAND, onOffToggleFlags},
	{"Night", RoomControl::PlugInfo::INPUT, MessageType::COMMAND, onOffToggleFlags},
	{"Summer", RoomControl::PlugInfo::INPUT, MessageType::COMMAND, onOffToggleFlags},
	{"Windows", RoomControl::PlugInfo::INPUT, MessageType::INDEXED_COMMAND, onOffFlags},
	{"Temperature", RoomControl::PlugInfo::INPUT, MessageType::TEMPERATURE},
	{"Set Temperature", RoomControl::PlugInfo::INPUT, MessageType::SET_TEMPERATURE},
	{"Valve", RoomControl::PlugInfo::OUTPUT, MessageType::ON_OFF},
};

static FunctionInfo const functionInfos[] = {
	{RoomControl::FunctionFlash::Type::SIMPLE_SWITCH, "Switch", switchPlugs},
	{RoomControl::FunctionFlash::Type::TIMEOUT_SWITCH, "Timeout Switch", switchPlugs},
	{RoomControl::FunctionFlash::Type::TIMED_BLIND, "Blind", blindPlugs},
};
static FunctionInfo const unknownFunctionInfo = {RoomControl::FunctionFlash::Type::UNKNOWN, "<unknown>", {}};

static FunctionInfo const &getFunctionInfo(RoomControl::FunctionFlash::Type type) {
	int i = array::binaryLowerBound(functionInfos, [type](FunctionInfo const &info) {return info.type < type;});
	FunctionInfo const &functionInfo = functionInfos[i];
	return functionInfo.type == type ? functionInfo : unknownFunctionInfo;
}

static int isInputCompatible(MessageType dstType, EndpointType srcType) {
	switch (srcType) {
		case EndpointType::ON_OFF_IN:
		case EndpointType::TRIGGER_IN:
		case EndpointType::UP_DOWN_IN:
			switch (dstType) {
				/*case MessageType::ON_OFF:
					return 4;
				case MessageType::TRIGGER:
				case MessageType::UP_DOWN:
					return 2;*/
				case MessageType::COMMAND:
				case MessageType::INDEXED_COMMAND:
				case MessageType::SET_TEMPERATURE:
					return 1;
				default:
					return 0;
			}
		case EndpointType::TEMPERATURE_IN:
			return dstType == MessageType::TEMPERATURE ? 1 : 0;
		default:
			return 0;
	}
}

static int isOutputCompatible(EndpointType dstType, MessageType srcType) {
	switch (srcType) {
		case MessageType::ON_OFF:
			switch (dstType) {
				case EndpointType::ON_OFF_OUT:
				case EndpointType::TRIGGER_OUT:
				case EndpointType::UP_DOWN_OUT:
					return 2;
				default:
					return 0;
			}

		case MessageType::TRIGGER:
			switch (dstType) {
				case EndpointType::TRIGGER_OUT:
				case EndpointType::UP_DOWN_OUT:
					return 2;
				default:
					return 0;
			}

		case MessageType::UP_DOWN:
			switch (dstType) {
				case EndpointType::ON_OFF_OUT:
				case EndpointType::TRIGGER_OUT:
				case EndpointType::UP_DOWN_OUT:
					return 2;
				default:
					return 0;
			}

		case MessageType::TEMPERATURE:
			switch (dstType) {
				case EndpointType::TEMPERATURE_OUT:
					return 1;
				default:
					return 0;
			}

		default:
			return 0;
	}
}

static int isCompatible(RoomControl::PlugInfo const &info, EndpointType endpointType) {
	if (info.isInput())
		return isInputCompatible(info.type, endpointType);
	return isOutputCompatible(endpointType, info.type);
}


/*
constexpr String buttonStatesLong[] = {"release", "press"};
constexpr String rockerStatesLong[] = {"release", "up", "down", String()};
constexpr String blindStatesLong[] = {"stopped", "up", "down", String()};

constexpr String buttonStates[] = {"#", "^", "!"};
constexpr String switchStates[] = {"0", "1"};
constexpr String rockerStates[] = {"#", "+", "-", "!"};

constexpr int8_t QOS = 1;
*/

// EndpointInfo
// ------------
/*
struct EndpointInfo {
	// type of endpoint
	EndpointType type;
	
	// name of endpoint (used in gui)
	String name;
	
	// number of array elements
	uint8_t elementCount;
};

constexpr EndpointInfo endpointInfos[1] = {
/ *	//{EndpointType::BINARY_SENSOR, "Binary Sensor", 0},
	{EndpointType::BUTTON, "Button Sensor", 0},
	{EndpointType::ON_OFF, "Switch Sensor", 0},
	{EndpointType::ROCKER, "Rocker Sensor", 2},
	{EndpointType::RELAY, "Relay Actuator", 0},
	{EndpointType::LIGHT, "Light Actuator", 0},
	{EndpointType::BLIND, "Blind Actuator", 2},
	{EndpointType::TEMPERATURE_IN, "Temperature Sensor", 0},
	{EndpointType::AIR_PRESSURE_IN, "Air Pressure Sensor", 0},
	{EndpointType::AIR_HUMIDITY_IN, "Air Humidity Sensor", 0},
	{EndpointType::AIR_VOC_IN, "Air VOC Sensor", 0},* /
};

EndpointInfo const *findEndpointInfo(EndpointType type) {
	int l = 0;
	int h = array::size(endpointInfos) - 1;
	while (l < h) {
		int mid = l + (h - l) / 2;
		if (endpointInfos[mid].type < type) {
			l = mid + 1;
		} else {
			h = mid;
		}
	}
	EndpointInfo const *endpointInfo = &endpointInfos[l];
	return endpointInfo->type == type ? endpointInfo : nullptr;
}


// ComponentInfo
// -------------

struct ClassInfo {
	uint8_t size;
	uint8_t classFlags;
};

struct ComponentInfo {
	// component has an element plugIndex
	static constexpr uint8_t ELEMENT_INDEX_1 = 1;
	static constexpr uint8_t ELEMENT_INDEX_2 = 2;

	// component has a configurable duration (derives from DurationElement)
	static constexpr uint8_t DURATION = 4;

	// component can receive commands (state derives from CommandElement)
	static constexpr uint8_t COMMAND = 8;


	RoomControl::Device::Component::Type type;
	String name;
	uint8_t shortNameIndex;
	bool hasCommand;
	ClassInfo component;
	ClassInfo state;
	uint8_t elementIndexStep;
};

template <typename T>
constexpr ClassInfo makeClassInfo() {
	return {uint8_t((sizeof(T) + 3) / 4), uint8_t(T::CLASS_FLAGS)};
}

constexpr String componentShortNames[] = {
	"bt",
	"sw",
	"rk",
	"rl",
	"bl",
	"tc",
	"tf"
};

constexpr int BT = 0;
constexpr int SW = 1;
constexpr int RK = 2;
constexpr int RL = 3;
constexpr int BL = 4;
constexpr int TC = 5;
constexpr int TF = 6;

constexpr ComponentInfo componentInfos[] = {
	{RoomControl::Device::Component::BUTTON, "Button", BT, false,
		makeClassInfo<RoomControl::Device::Button>(),
		makeClassInfo<RoomControl::DeviceState::Button>(),
		1},
	{RoomControl::Device::Component::HOLD_BUTTON, "Hold Button", BT, false,
		makeClassInfo<RoomControl::Device::HoldButton>(),
		makeClassInfo<RoomControl::DeviceState::HoldButton>(),
		1},
	{RoomControl::Device::Component::DELAY_BUTTON, "Delay Button", BT, false,
		makeClassInfo<RoomControl::Device::DelayButton>(),
		makeClassInfo<RoomControl::DeviceState::DelayButton>(),
		1},
	{RoomControl::Device::Component::SWITCH, "Switch", SW, false,
		makeClassInfo<RoomControl::Device::Switch>(),
		makeClassInfo<RoomControl::DeviceState::Switch>(),
		1},
	{RoomControl::Device::Component::ROCKER, "Rocker", RK, false,
		makeClassInfo<RoomControl::Device::Rocker>(),
		makeClassInfo<RoomControl::DeviceState::Rocker>(),
		2},
	{RoomControl::Device::Component::HOLD_ROCKER, "Hold Rocker", RK, false,
		makeClassInfo<RoomControl::Device::HoldRocker>(),
		makeClassInfo<RoomControl::DeviceState::HoldRocker>(),
		2},
	{RoomControl::Device::Component::RELAY, "Relay", RL, true,
		makeClassInfo<RoomControl::Device::Relay>(),
		makeClassInfo<RoomControl::DeviceState::Relay>(),
		0},
	{RoomControl::Device::Component::TIME_RELAY, "Time Relay", RL, true,
		makeClassInfo<RoomControl::Device::TimeRelay>(),
		makeClassInfo<RoomControl::DeviceState::TimeRelay>(),
		0},
	{RoomControl::Device::Component::BLIND, "Blind", BL, true,
		makeClassInfo<RoomControl::Device::Blind>(),
		makeClassInfo<RoomControl::DeviceState::Blind>(),
		0},
	{RoomControl::Device::Component::CELSIUS, "Celsius", TC, false,
		makeClassInfo<RoomControl::Device::TemperatureSensor>(),
		makeClassInfo<RoomControl::DeviceState::TemperatureSensor>(),
		0},
	{RoomControl::Device::Component::FAHRENHEIT, "Fahrenheit", TF, false,
		makeClassInfo<RoomControl::Device::TemperatureSensor>(),
		makeClassInfo<RoomControl::DeviceState::TemperatureSensor>(),
		0}
};


// ValueInfo
// -----------

struct ValueInfo {
	RoomControl::Command::ValueType type;
	String name;
	uint8_t size;
	uint8_t elementCount;
};

ValueInfo valueInfos[] = {
	{RoomControl::Command::ValueType::BUTTON, "Button", 0, 0},
	{RoomControl::Command::ValueType::SWITCH, "Switch", 1, 1},
	{RoomControl::Command::ValueType::ROCKER, "Rocker", 1, 1},
	{RoomControl::Command::ValueType::PERCENTAGE, "Percentage", 1, 1},
	{RoomControl::Command::ValueType::CELSIUS, "Celsius", 2, 1},
	{RoomControl::Command::ValueType::FAHRENHEIT, "Fahrenheit", 2, 1},
	{RoomControl::Command::ValueType::COLOR_RGB, "Color RGB", 3, 3},
	{RoomControl::Command::ValueType::VALUE8, "Value 0-255", 1, 1},
};
*/

// RoomControl
// -----------

RoomControl::RoomControl()
	: storage(0, FLASH_PAGE_COUNT, configurations, busInterface.devices, radioInterface.gpDevices,
		radioInterface.zbDevices, alarmInterface.alarms, functions)
	, stateManager()
	, houseTopicId(), roomTopicId()
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
	auto &configuration = *configurations[0];
	
	this->busInterface.start(configuration.key, configuration.aesKey);
	this->radioInterface.start(configuration.zbeePanId, configuration.key, configuration.aesKey);
	
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
	Radio::setLongAddress(configuration.ieeeLongAddress);
	//radio::setPan(RADIO_ZBEE, configuration.zbeePanId);
}



/*
// todo don't register all topics in one go, message buffer may overflow
void RoomControl::subscribeAll() {
	
	// register/subscribe room topics
	auto const &configuration = *this->configurations[0];
	
	// house topic (room name is published when empty message is received on this topic)
	TopicBuffer topic;
	subscribeTopic(this->houseTopicId, topic.enumeration(), QOS);

	// toom topic (device names are published when empty message is received on this topic)
	topic /= configuration.name;
	subscribeTopic(this->roomTopicId, topic.enumeration(), QOS);


	// register/subscribe device topics
	for (int i = 0; i < this->localDevices.size(); ++i) {
		subscribeDevice(this->localDevices[i]);
	}
	for (int i = 0; i < this->busDevices.size(); ++i) {
		subscribeDevice(this->busDevices[i]);
	}

	// register routes
	for (int i = 0; i < this->routes.size(); ++i) {
		subscribeRoute(i);
	}

	// register timers
	for (int i = 0; i < this->timers.size(); ++i) {
		subscribeTimer(i);
	}
}


// Devices
// -------

Plus<String, Dec<uint8_t>> RoomControl::Device::Component::getName() const {
	//return componentInfos[this->type].shortName + dec(this->endpointIndex) + ('a' + this->componentIndex);
	uint8_t shortNameIndex = componentInfos[this->type].shortNameIndex;
	return componentShortNames[shortNameIndex] + dec(this->nameIndex);
}

void RoomControl::Device::Component::init(EndpointType endpointType) {
/ *	auto type = Device::Component::BUTTON;
	
	int i = array::size(componentInfos);
	while (i > 0 && !isCompatible(endpointType, type)) {
		--i;
		int t = type + 1;
		type = Device::Component::Type(t % array::size(componentInfos));
	}
	
	int newSize = componentInfos[type].component.size;
	auto dst = reinterpret_cast<uint32_t *>(this);
	array::fill(dst, dst + newSize, 0);

	this->type = type;* /
}

void RoomControl::Device::Component::rotateType(EndpointType endpointType, int delta) {
/ *	auto type = this->type;
	int oldSize = componentInfos[type].component.size;
	
	int dir = delta >= 0 ? 1 : -1;
	int i = array::size(componentInfos);
	while (delta != 0) {
		delta -= dir;
		do {
			--i;
			int t = type + dir;
			type = Device::Component::Type((t + array::size(componentInfos)) % array::size(componentInfos));
		} while (i > 0 && !isCompatible(endpointType, type));
	}
	this->type = type;
	int newSize = componentInfos[type].component.size;
	
	if (newSize > oldSize) {
		auto dst = reinterpret_cast<uint32_t *>(this) + oldSize;
		array::fill(dst, dst + (newSize - oldSize), 0);
	}* /
}

bool RoomControl::Device::Component::checkClass(uint8_t classFlags) const {
	return (componentInfos[this->type].component.classFlags & classFlags) == classFlags;
}

int RoomControl::Device::getFlashSize() const {
	// flash size of name and elements
	uint32_t const *flash = this->buffer;
	int size = 0;
	for (int i = 0; i < this->componentCount; ++i) {
		auto type = reinterpret_cast<Device::Component const *>(flash + size)->type;
		size += componentInfos[type].component.size;
	}
	return getOffset(Device, buffer[size]);
}

int RoomControl::Device::getRamSize() const {
	// ram size of elements
	uint32_t const *flash = this->buffer;
	int size = 0;
	for (int i = 0; i < this->componentCount; ++i) {
		auto type = reinterpret_cast<Device::Component const *>(flash)->type;
		flash += componentInfos[type].component.size;
		size += componentInfos[type].state.size;
	}
	return getOffset(DeviceState, buffer[size]);
}

void RoomControl::Device::setName(String name) {
	assign(this->name, name);
}

uint8_t RoomControl::Device::getNameIndex(Component::Type type, int skipComponentIndex) const {
	// set of already used name indices
	uint32_t usedSet = 0;
	
	uint8_t shortNameIndex = componentInfos[type].shortNameIndex;
	
	uint32_t const *flash = this->buffer;
	for (int i = 0; i < this->componentCount; ++i) {
		auto &component = *reinterpret_cast<Component const *>(flash);
		auto &componentInfo = componentInfos[component.type];
		if (i != skipComponentIndex) {
			if (componentInfo.shortNameIndex == shortNameIndex)
				usedSet |= 1 << component.nameIndex;
		}
		flash += componentInfo.component.size;
	}
	
	// find first unused name index
	for (int i = 0; i < 32; ++i) {
		if (((usedSet >> i) & 1) == 0)
			return i;
	}
	return 0;
}


bool RoomControl::DeviceState::Component::checkClass(Device::Component::Type type, uint8_t classFlags) const {
	return (componentInfos[type].state.classFlags & classFlags) == classFlags;
}

void RoomControl::subscribeDevice(Storage::Element<Device, DeviceState> e) {
	Device const &device = *e.flash;
	DeviceState &deviceState = *e.ram;

	// subscribe to device topic (enum/<room>/<device>)
	TopicBuffer topic = getRoomName() / device.getName();
	subscribeTopic(deviceState.deviceTopicId, topic.enumeration(), QOS);

	// iterate over device components
	ComponentIterator it(device, deviceState);
	while (!it.atEnd()) {
		auto &component = it.getComponent();
		auto &state = it.getState();
		
		// append component topic
		StringBuffer<8> b = component.getName();
		topic /= b;
		
		// register status topic (stat/<room>/<device>/<component>)
		registerTopic(state.statusTopicId, topic.state());

		// check if component has a command topic
		if (state.is<DeviceState::CommandComponent>(component)) {
			// subscribe to command topic (cmnd/<room>/<device>/<component>)
			auto &commandComponentState = state.cast<DeviceState::CommandComponent>();
			subscribeTopic(commandComponentState.commandTopicId, topic.command(), QOS);
		}
		
		topic.removeLast();
		it.next();
	}
}

SystemTime RoomControl::updateDevices(SystemTime time,
	uint8_t localEndpointId, uint8_t busEndpointId, uint8_t radioEndpointId, uint8_t const *data,
	uint16_t topicId, String message)
{
	// get duration since last update
	SystemDuration duration = time - this->lastUpdateTime;
	this->lastUpdateTime = time;

	// flag that indicates if changing values (e.g. moving blind) should be reported
	bool reportChanging = time >= this->nextReportTime;
	if (reportChanging) {
		// publish changing values in a regular interval
		this->nextReportTime = time + 1s;
		//std::cout << "report" << std::endl;
	}

	// next timeout, may be decreased by a device that needs earlier timeout
	SystemTime nextTimeout = this->nextReportTime;

	//nextTimeout = updateDevices(time, duration, reportChanging, this->localInterface, this->localDevices,
	//	localEndpointId, data, topicId, message, nextTimeout);
//	nextTimeout = updateDevices(time, duration, reportChanging, this->busInterface, this->busDevices,
//		busEndpointId, data, topicId, message, nextTimeout);
//	nextTimeout = updateDevices(time, duration, reportChanging, this->radioInterface, this->radioDevices,
//		radioEndpointId, data, topicId, message, nextTimeout);

	return nextTimeout;
}*/
/*
SystemTime RoomControl::updateDevices(SystemTime time, SystemDuration duration, bool reportChanging,
	Interface &interface, Storage::Array<Device, DeviceState> &devices, uint8_t endpointId, uint8_t const *data,
	uint16_t topicId, String message, SystemTime nextTimeout)
{
	// iterate over devices
	int deviceCount = devices.size();
	int extendedCount = deviceCount + (this->tempFor == &devices ? 1 : 0);
	for (int deviceIndex = 0; deviceIndex < extendedCount; ++deviceIndex) {
		// check if there is a device being edited
		Storage::Element<Device, DeviceState> e;
		if (deviceIndex == deviceCount || this->tempFor == devices[deviceIndex].flash)
			e = {&this->temp.device, &this->tempState.device};
		else
			e = devices[deviceIndex];
		Device const &device = *e.flash;
		DeviceState &deviceState = *e.ram;

		// check for request to list attributes in this device
		if (topicId == deviceState.deviceTopicId && message.isEmpty()) {
			for (ComponentIterator it(device, deviceState); !it.atEnd(); it.next()) {
				auto &component = it.getComponent();
				StringBuffer<8> b = component.getName();
				b += ' ';
				b += componentInfos[component.type].hasCommand ? 'c' : 's';
				publish(deviceState.deviceTopicId, b, QOS);
			}
		}
		
		// output data to send to a device endpoint
		uint8_t outValid = false;
		uint8_t outData[8] = {};

		// iterate over device elements
		ComponentIterator it(device, deviceState);
		while (!it.atEnd()) {
			auto &component = it.getComponent();
			auto &state = it.getState();
			
			// check if we have data from device or output data (that can be used as input by button/switch components)
			uint8_t const *d = endpointId == state.endpointId ? data : (outValid ? outData : nullptr);
			
			switch (component.type) {
			case Device::Component::BUTTON:
				if (d != nullptr) {
					//std::cout << "button1 " << int(data[0]) << " " << int(d[0]) << std::endl;
					// get button state
					uint8_t value = (d[0] >> component.elementIndex) & 1;
					if (value != state.flags) {
						state.flags = value;
						publish(state.statusTopicId, buttonStates[value], QOS);
					}
					//std::cout << "button2 " << int(data[0]) << " " << int(d[0]) << std::endl;
				}
				break;
			case Device::Component::HOLD_BUTTON:
				if (d != nullptr) {
					// get button state
					uint8_t value = (d[0] >> component.elementIndex) & 1;
					if (value != state.flags) {
						state.flags = value;

						auto &holdButton = component.cast<Device::HoldButton>();
						auto &holdButtonState = state.cast<DeviceState::HoldButton>();
						if (value != 0) {
							// pressed: set timeout
							holdButtonState.timeout = time + holdButton.duration;
						} else {
							// released: publish "stop" instead of "release" when timeout has elapsed
							if (time >= holdButtonState.timeout)
								value = 2;
						}
						publish(state.statusTopicId, buttonStates[value], QOS);
					}
				}
				break;
			case Device::Component::DELAY_BUTTON:
				{
					auto &delayButton = component.cast<Device::DelayButton>();
					auto &delayButtonState = state.cast<DeviceState::DelayButton>();
				
					if (d != nullptr) {
						// get button state
						uint8_t value = (d[0] >> component.elementIndex) & 1;
//std::cout << "delay button " << int(value) << " " << int(state.flags) << std::endl;
std::cout << "delay button " << int(data[0]) << " " << int(d[0]) << std::endl;
						if (value != state.flags) {
							uint8_t pressed = state.flags & 2;
							state.flags = value;

							if (value != 0) {
								// pressed: set timeout, but not change state yet
								delayButtonState.timeout = time + delayButton.duration;
								nextTimeout = min(nextTimeout, delayButtonState.timeout);
							} else {
								// released: publish "release" if "press" was sent
								if (pressed != 0) {
									publish(state.statusTopicId, buttonStates[0], QOS);
								}
							}
						}
					}
					
					if (state.flags == 1) {
						// currently pressed, but timeout has not elapsed yet
						if (time < delayButtonState.timeout) {
							// calc next timeout
							nextTimeout = min(nextTimeout, delayButtonState.timeout);
						} else {
							// timeout elapsed: set pressed flag and publish press
							state.flags = 3;
							publish(delayButtonState.statusTopicId, switchStates[1], QOS);
						}
					}
				}
				break;
			case Device::Component::SWITCH:
				if (d != nullptr) {
					// get switch state
					uint8_t value = (d[0] >> component.elementIndex) & 1;
					if (value != state.flags) {
						state.flags = value;
						publish(state.statusTopicId, switchStates[value], QOS);
					}
				}

				break;
			case Device::Component::ROCKER:
				if (d != nullptr) {
					// get rocker state
					uint8_t value = (d[0] >> component.elementIndex) & 3;
					if (value != state.flags) {
						state.flags = value;
						publish(state.statusTopicId, rockerStates[value], QOS);
					}
				}

				break;
			case Device::Component::HOLD_ROCKER:
				if (d != nullptr) {
					// get rocker state
					uint8_t value = (d[0] >> component.elementIndex) & 3;
					if (value != state.flags) {
						state.flags = value;

						auto &holdRocker = component.cast<Device::HoldRocker>();
						auto &holdRockerState = state.cast<DeviceState::HoldRocker>();
						if (value != 0) {
							// up or down pressed: set timeout
							holdRockerState.timeout = time + holdRocker.duration;
						} else {
							// released: publish "stop" instead of "release" when timeout has elapsed
							if (time >= holdRockerState.timeout)
								value = 3;
						}
						publish(state.statusTopicId, rockerStates[value], QOS);
					}
				}
				break;
			case Device::Component::RELAY:
			case Device::Component::TIME_RELAY:
				{
					auto &relayState = state.cast<DeviceState::Relay>();
					uint8_t relay = state.flags & DeviceState::Blind::RELAY;
					bool report  = false;
					
					// handle mqtt message
					if (topicId == relayState.commandTopicId) {
						if (message.isEmpty()) {
							// indicate that we want to publish the current state
							report = true;
						} else {
							bool on = relay != 0;

							// try to parse float value
							optional<float> value = parseFloat(message);
							if (value != null) {
								on = *value != 0.0f;
								relayState.flags &= ~DeviceState::Relay::PRESSED;
							} else {
								char command = message.length == 1 ? message[0] : 0;
								bool toggle = command == '^';
								bool up = command == '+';
								bool down = command == '-';
								bool release = command == '#';
								bool pressed = (relayState.flags & DeviceState::Blind::PRESSED) != 0;

								if (release) {
									// release
									relayState.flags &= ~DeviceState::Blind::PRESSED;
								} else if (!pressed || !(toggle || up || down)) {
									// switch off when not pressed or unrecognized command
									on = up || (toggle && !on);
									relayState.flags &= ~DeviceState::Blind::PRESSED;
								}
							}
							relay = uint8_t(on);
						}
						
						// publish relay state if it has changed
						if (relay != (relayState.flags & DeviceState::Relay::RELAY)) {
							state.flags ^= DeviceState::Relay::RELAY;

							// indicate we want to send the new state to the device endpoint
							outValid = true;

							// indicate that we want to publish the current state
							report = true;
						
							// set timeout
							if (relay != 0 && component.type == Device::Component::TIME_RELAY) {
								auto &timeRelay = component.cast<Device::TimeRelay>();
								auto &timeRelayState = state.cast<DeviceState::TimeRelay>();
							
								// set on timeout
								timeRelayState.timeout = time + timeRelay.duration;
							}
						}
					}

					// handle timeout of time relay
					if (relay != 0 && component.type == Device::Component::TIME_RELAY) {
						// currently on
						auto &timeRelayState = state.cast<DeviceState::TimeRelay>();
						if (time < timeRelayState.timeout) {
							// calc next timeout
							nextTimeout = min(nextTimeout, timeRelayState.timeout);
						} else {
							// switch off
							timeRelayState.flags &= ~DeviceState::Blind::RELAY;
							relay = 0;
							
							// indicate we want to send the new state to the device endpoint
							outValid = true;

							// indicate that we want to publish the current state
							report = true;
						}
					}
					
					// publish current state
					if (report)
						publish(state.statusTopicId, switchStates[relay], QOS);
					
					// set current state to output data
					outData[0] |= relay << component.elementIndex;
				}
				break;
			case Device::Component::BLIND:
				{
					auto &blind = component.cast<Device::Blind>();
					auto &blindState = state.cast<DeviceState::Blind>();
					uint8_t relays = state.flags & DeviceState::Blind::RELAYS;
					bool report = false;
					
					// handle topic
					if (topicId == blindState.commandTopicId) {
						if (message.isEmpty()) {
							// indicate that we want to publish the current state
							report = true;
						} else {
							// try to parse float value
							optional<float> value = parseFloat(message);
							if (value != null) {
								// move to target position in the range [0, 1]
								// todo: maybe prevent instant direction change
								if (*value >= 1.0f) {
									// move up with extra time
									blindState.timeout = time + blind.duration - blindState.duration + 500ms;
									relays = DeviceState::Blind::RELAY_UP;
								} else if (*value <= 0.0f) {
									// move down with extra time
									blindState.timeout = time + blind.duration + 500ms;
									relays = DeviceState::Blind::RELAY_DOWN;
								} else {
									// move to target position
									SystemDuration targetDuration = blind.duration * *value;
									if (targetDuration > blindState.duration) {
										// move up
										blindState.timeout = time + targetDuration - blindState.duration;
										relays = DeviceState::Blind::RELAY_UP;
										blindState.flags |= DeviceState::Blind::DIRECTIION_UP;
									} else {
										// move down
										blindState.timeout = time + blindState.duration - targetDuration;
										relays = DeviceState::Blind::RELAY_DOWN;
										blindState.flags &= ~DeviceState::Blind::DIRECTIION_UP;
									}
								}
								blindState.flags &= ~DeviceState::Blind::PRESSED;
							} else {
								char command = message.length == 1 ? message[0] : 0;
								bool toggle = command == '^';
								bool up = command == '+';
								bool down = command == '-';
								bool release = command == '#';
								bool pressed = (blindState.flags & DeviceState::Blind::PRESSED) != 0;
								
								if (relays != 0) {
									// currently moving
									if (release) {
										// release
										blindState.flags &= ~DeviceState::Blind::PRESSED;
									} else if (!pressed || !(toggle || up || down)) {
										// stop when not pressed or unrecognized command (e.g. empty message)
										relays = 0;
										blindState.flags &= ~DeviceState::Blind::PRESSED;
									}
								} else {
									// currently stopped: start on toggle, up and down
									bool directionUp = (blindState.flags & DeviceState::Blind::DIRECTIION_UP) != 0;
									if (up || (toggle && !directionUp)) {
										// up with extra time
										blindState.timeout = time + blind.duration - blindState.duration + 500ms;
										relays = DeviceState::Blind::RELAY_UP;
										blindState.flags |= DeviceState::Blind::DIRECTIION_UP | DeviceState::Blind::PRESSED;
									} else if (down || (toggle && directionUp)) {
										// down with extra time
										blindState.timeout = time + blindState.duration + 500ms;
										relays = DeviceState::Blind::RELAY_DOWN;
										blindState.flags &= ~DeviceState::Blind::DIRECTIION_UP;
										blindState.flags |= DeviceState::Blind::PRESSED;
									}
								}
							}
						}
						
						// check if state of relays has changed
						if (relays != (state.flags & DeviceState::Blind::RELAYS)) {
							state.flags = (state.flags & ~DeviceState::Blind::RELAYS) | relays;

							// indicate we want to send the new state to the device endpoint
							outValid = true;
							
							if (relays == 0) {
								// stopped: indicate that we want to publish the current state
								report = true;
							}
						}
					}

					// handle timeout if currently moving
					if (relays != 0) {
						if (relays & DeviceState::Blind::RELAY_UP) {
							// currently moving up
							blindState.duration += duration;
							if (blindState.duration > blind.duration)
								blindState.duration = blind.duration;
						} else {
							// currently moving down
							blindState.duration -= duration;
							if (blindState.duration < 0s)
								blindState.duration = 0s;
						}
					
						// check if run time has elapsed
						if (time < blindState.timeout) {
							// no: calc next timeout
							nextTimeout = min(nextTimeout, blindState.timeout);
						} else {
							// yes: stop
							blindState.flags &= ~DeviceState::Blind::RELAYS;
							relays = 0;

							// indicate we want to send the new state to the device endpoint
							outValid = true;

							// stopped: indicate that we want to publish the current state
							report = true;
						}
					}

					// publish current position
					if (report || (reportChanging && relays != 0)) {
						float pos = blindState.duration / blind.duration;
						StringBuffer<8> b = flt(pos, 0, relays == 0 ? 3 : 2);
						publish(state.statusTopicId, b, QOS);
					}

					// set current state to output data
					outData[0] |= relays << component.elementIndex;
				}
				break;
			case Device::Component::CELSIUS:
				if (d != nullptr) {
					auto &temperatureSensorState = state.cast<DeviceState::TemperatureSensor>();
					uint16_t t = d[0] | (d[1] << 8);
					temperatureSensorState.temperature = t;

					// convert temperature from 1/20 K to 1/10 °C
					int celsius = (t - 5463) / 2; // subtract 273.15 * 20 and divide by 2
					StringBuffer<6> b = dec(celsius / 10) + '.' + dec((celsius + 3000) % 10);
					publish(state.statusTopicId, b, QOS);
				}
				break;
			case Device::Component::FAHRENHEIT:
				if (d != nullptr) {
					auto &temperatureSensorState = state.cast<DeviceState::TemperatureSensor>();
					uint16_t t = d[0] | (d[1] << 8);
					temperatureSensorState.temperature = t;

					// convert temperature from 1/20 K to 1/10 °F
					int fahrenheit = (t - 5463) * 9 / 10 + 320;
					StringBuffer<6> b = dec(fahrenheit / 10) + '.' + dec((fahrenheit - 5000) % 10);
					publish(state.statusTopicId, b, QOS);
				}

				break;
			}

			uint8_t endpointIndex = component.endpointIndex;
			uint8_t endpointId = state.endpointId;
			it.next();
			if (it.atEnd() || it.getComponent().endpointIndex != endpointIndex) {
				if (outValid) {
					//busSend(endpointId, outData, 1);
					interface.send(endpointId, outData, 1);
					outValid = false;
				}
				outData[0] = 0;
				//array::fill(outData, array::end(outData), 0);
			}
		}
	}
	return nextTimeout;
}

bool RoomControl::isCompatible(EndpointType endpointType, Device::Component::Type type) {
	bool binary = type == Device::Component::BUTTON || type == Device::Component::SWITCH;
	bool timedBinary = type == Device::Component::HOLD_BUTTON || type == Device::Component::DELAY_BUTTON;
	bool ternary = type == Device::Component::ROCKER;
	bool timedTernary = type == Device::Component::HOLD_ROCKER;
	bool relay = type == Device::Component::RELAY || type == Device::Component::TIME_RELAY;
	
	switch (endpointType) {
	//case EndpointType::BINARY_SENSOR:
	case EndpointType::BUTTON:
	case EndpointType::ON_OFF:
		// buttons and switchs
		return binary || timedBinary;
	case EndpointType::ROCKER:
		// buttons, switches and rockers
		return binary || timedBinary || ternary || timedTernary;
	//case EndpointType::RELAY:
	//case EndpointType::LIGHT:
		// one relay with binary sensors for the relay state
		return relay || binary;
	case EndpointType::BLIND:
		// two relays with binary and ternaty sensors for the relay states
		return type == relay || Device::Component::BLIND || binary || ternary;
	case EndpointType::TEMPERATURE_IN:
		// temperature sensor
		return type == Device::Component::CELSIUS || type == Device::Component::FAHRENHEIT;
	}
	return false;
}

void RoomControl::listDevices(Interface &interface, Storage::Array<Device, DeviceState> &devices,
	MenuState editDevice, MenuState addDevice)
{
	// list devices
	for (auto e : devices) {
		auto &device = *e.flash;
		
		StringBuffer<24> b = device.getName();
		if (entry(b)) {
			// edit device
			clone(interface, this->temp.device, this->tempState.device, device, *e.ram);
			
			// indicate that the device is being edited
			this->tempFor = &device;
			push(editDevice);
		}
	}

	// menu entries to add missing devices
	int deviceCount = interface.getDeviceCount();
	for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
		DeviceId deviceId = interface.getDeviceId(deviceIndex);
		bool found = false;
		for (auto e : devices) {
			auto &device = *e.flash;
			if (device.deviceId == deviceId) {
				found = true;
				break;
			}
		}
		if (!found) {
			StringBuffer<24> b = "Add " + hex(deviceId, deviceId <= 0xffffffff ? 8 : 16);
			if (entry(b)) {
				Device &device = this->temp.device;
				DeviceState &deviceState = this->tempState.device;
				device.deviceId = deviceId;
				device.setName(b.string().substring(4));
				device.componentCount = 0;
				deviceState.deviceTopicId = 0;
				
				// subscribe to device topic (enum/<room>/<device>)
				TopicBuffer topic = getRoomName() / device.getName();
				subscribeTopic(deviceState.deviceTopicId, topic.enumeration(), QOS);

				// indicate that a new device is being edited by letting tempFor point to the device array
				this->tempFor = &devices;
				push(addDevice);
			}
		}
	}

	if (entry("Exit")) {
		interface.setCommissioning(false);
		pop();
	}
}

void RoomControl::editDevice(Interface &interface, Storage::Array<Device, DeviceState> &devices, bool add,
	MenuState editComponent, MenuState addComponent)
{
	Device &device = this->temp.device;
	DeviceState &deviceState = this->tempState.device;

	// device name
	//todo: edit name
	label(device.getName());

	// device endpoints
	Array<EndpointType const> endpoints = interface.getEndpoints(device.deviceId);
	ComponentIterator it(device, deviceState);
	for (int endpointIndex = 0; endpointIndex < endpoints.length; ++endpointIndex) {
		line();
		
		// endpoint type
		auto endpointType = endpoints[endpointIndex];
		StringBuffer<24> b;
		auto endpointInfo = findEndpointInfo(endpointType);
		if (endpointInfo != nullptr)
			b += endpointInfo->name;
		else
			b += '?';
		label(b);
		
		// device components that are associated with the current device endpoint
		while (!it.atEnd() && it.getComponent().endpointIndex == endpointIndex) {
			auto &component = it.getComponent();
			auto &state = it.getState();
			ComponentInfo const &componentInfo = componentInfos[component.type];

			// menu entry for compnent (component name and current state/value)
			b = componentInfo.name + ": "; // component.getName()
			switch (component.type) {
			case Device::Component::BUTTON:
			case Device::Component::HOLD_BUTTON:
			case Device::Component::DELAY_BUTTON:
				{
					b += buttonStatesLong[state.flags & 1];
				}
				break;
			case Device::Component::ROCKER:
			case Device::Component::HOLD_ROCKER:
				{
					b += rockerStatesLong[state.flags & 3];
				}
				break;
			case Device::Component::SWITCH:
			case Device::Component::RELAY:
			case Device::Component::TIME_RELAY:
				{
					b += switchStatesLong[state.flags & 1];
				}
				break;
			case Device::Component::BLIND:
				{
					auto blind = component.cast<Device::Blind>();
					auto blindState = state.cast<DeviceState::Blind>();
					float pos = blindState.duration / blind.duration;
					b += flt(pos, 1, (state.flags & 3) == 0 ? 3 : 2);
				}
				break;
			case Device::Component::CELSIUS:
				{
					auto temperatureSensorState = state.cast<DeviceState::TemperatureSensor>();
					uint16_t t = temperatureSensorState.temperature;

					// convert temperature from 1/20 K to 1/10 °C
					int celsius = (t - 5463) / 2; // subtract 273.15 * 20 and divide by 2
					b += dec(celsius / 10) + '.' + dec((celsius + 3000) % 10) + " oC";
				}
				break;
			case Device::Component::FAHRENHEIT:
				{
					auto temperatureSensorState = state.cast<DeviceState::TemperatureSensor>();
					uint16_t t = temperatureSensorState.temperature;

					// convert temperature from 1/20 K to 1/10 °F
					int fahrenheit = (t - 5463) * 9 / 10 + 320;
					b += dec(fahrenheit / 10) + '.' + dec((fahrenheit - 5000) % 10) + "oF";
				}
				break;
			}
			if (entry(b)) {
				// set index
				this->tempIndex = it.componentIndex;

				// copy component
				auto dst = reinterpret_cast<uint32_t *>(&this->temp2.component);
				auto src = reinterpret_cast<uint32_t const *>(&component);
				array::copy(dst, dst + componentInfo.component.size, src);
				
				push(editComponent);
			}
			
			it.next();
		}
		
		// add device component
		if (device.componentCount < Device::MAX_COMPONENT_COUNT) {
			if (entry("Add Component")) {
				// set index
				this->tempIndex = it.componentIndex;

				// select first valid type
				auto &component = this->temp2.component;
				component.init(endpointType);
				component.endpointIndex = endpointIndex;
				component.nameIndex = device.getNameIndex(component.type, -1);

				// set default duration
				if (component.is<Device::TimeComponent>())
					this->temp2.timeComponent.duration = 1s;

				push(addComponent);
			}
		}
	}
	line();

	// save, delete, cancel
	if (entry("Save Device")) {
		int index = getThisIndex();
		
		// unsubscribe old command topics
		if (index < devices.size()) {
			auto e = devices[index];
			destroy(interface, *e.flash, *e.ram);
		}
		devices.write(index, &device, &deviceState);
		this->tempFor = nullptr;
		pop();
	}
	if (!add) {
		if (entry("Delete Device")) {
			int index = getThisIndex();

			// unsubscribe new command topics
			destroy(interface, device, deviceState);

			// unsubscribe old command topics
			auto e = devices[index];
			destroy(interface, *e.flash, *e.ram);
			
			this->routes.erase(getThisIndex());
			this->tempFor = nullptr;
			pop();
		}
	}
	if (entry("Cancel")) {
		// unsubscribe new command topics
		destroy(interface, device, deviceState);
		this->tempFor = nullptr;
		pop();
	}
}

void RoomControl::editComponent(int delta, Interface &interface, bool add) {
	uint8_t editBegin[5];
	uint8_t editEnd[5];
	Device const &device = this->temp.device;
	Device::Component &component = this->temp2.component;
	
	// get endpoint type
	auto endpointType = interface.getEndpoints(device.deviceId)[component.endpointIndex];
	auto endpointInfo = findEndpointInfo(endpointType);
	
	// edit component type
	int edit = getEdit(1);
	if (edit > 0 && delta != 0) {
		component.rotateType(endpointType, delta);
		component.nameIndex = device.getNameIndex(component.type, this->tempIndex);

		// set default values
		if (component.is<Device::TimeComponent>())
			this->temp2.timeComponent.duration = 1s;
	}
	ComponentInfo const &componentInfo = componentInfos[component.type];
	
	// menu entry for component type
	editBegin[1] = 0;
	StringBuffer<24> b = componentInfo.name;
	editEnd[1] = b.length();
	entry(b, edit > 0, editBegin[1], editEnd[1]);

	// component name (auto-generated)
	b = "Name: " + component.getName();
	label(b);

	// element index
	int step = componentInfo.elementIndexStep;
	if (endpointInfo != nullptr && step < endpointInfo->elementCount) {
		// edit element index
		int edit = getEdit(1);
		if (edit > 0) {
			int elementCount = endpointInfo->elementCount;
			component.elementIndex = (component.elementIndex + delta * step + elementCount) % elementCount;
		}

		// menu entry for element index
		b = "Index: ";
		editBegin[1] = b.length();
		b += dec(component.elementIndex);
		editEnd[1] = b.length();
		entry(b, edit > 0, editBegin[1], editEnd[1]);
	}

	// duration
	if (component.is<Device::TimeComponent>()) {
		SystemDuration duration = this->temp2.timeComponent.duration;
						
		// decompose duration
		int tenths = duration % 1s / 100ms;
		int seconds = int(duration / 1s) % 60;
		int minutes = int(duration / 1min) % 60;
		int hours = duration / 1h;
		
		// edit duration
		int edit = getEdit(hours == 0 ? 4 : 3);
		if (edit > 0) {
			if (delta != 0) {
				if (edit == 1) {
					hours = (hours + delta + 512) & 511;
					tenths = 0;
				}
				if (edit == 2)
					minutes = (minutes + delta + 60) % 60;
				if (edit == 3)
					seconds = (seconds + delta + 60) % 60;
				if (edit == 4) {
					tenths = (tenths + delta + 10) % 10;
				}
				this->temp2.timeComponent.duration = ((hours * 60 + minutes) * 60 + seconds) * 1s + tenths * 100ms;
			}
		}/ * else if (duration < 100ms) {
			// enforce minimum duration
			this->temp2.timeComponent.duration = 100ms;
		}* /

		// menu entry for duration
		b = "Duration: ";
		editBegin[1] = b.length();
		b += dec(hours);
		editEnd[1] = b.length();
		b += ':';
		editBegin[2] = b.length();
		b += dec(minutes, 2);
		editEnd[2] = b.length();
		b += ':';
		editBegin[3] = b.length();
		b += dec(seconds, 2);
		editEnd[3] = b.length();
		if (hours == 0) {
			b += '.';
			editBegin[4] = b.length();
			b += dec(tenths);
			editEnd[4] = b.length();
		}
		entry(b, edit > 0, editBegin[edit], editEnd[edit]);
	}

	// save, delete, cancel
	if (entry("Save Component")) {
		// seek to component
		ComponentEditor editor(this->temp.device, this->tempState.device, this->tempIndex);
		auto &state = editor.getState();
		TopicBuffer topic = getRoomName() / device.getName();

		// set type or insert component
		if (!add) {
			// edit component
			auto &oldComponent = editor.getComponent();

			// unregister old status topic
			unregisterTopic(state.statusTopicId);
			
			// unsubscribe from old command topic
			if (state.is<DeviceState::CommandComponent>(oldComponent)) {
				auto &oldCommandState = state.cast<DeviceState::CommandComponent>();
				StringBuffer<8> b = oldComponent.getName();
				topic /= b;
				unsubscribeTopic(oldCommandState.commandTopicId, topic.command());
				topic.removeLast();
			}
								
			// change component
			editor.changeType(component.type);
		} else {
			// add component
			editor.insert(component.type);
			++this->temp.device.componentCount;
		
			// subscribe to endpoint
			interface.subscribe(state.endpointId, device.deviceId, component.endpointIndex);
		}

		// copy component
		auto &newComponent = editor.getComponent();
		auto dst = reinterpret_cast<uint32_t *>(&newComponent);
		auto src = reinterpret_cast<uint32_t const *>(&component);
		array::copy(dst, dst + componentInfo.component.size, src);

		// register new state topic
		StringBuffer<8> b = component.getName();
		topic /= b;
		registerTopic(state.statusTopicId, topic.state());

		// subscribe new command topic
		if (state.is<DeviceState::CommandComponent>(component)) {
			auto &commandState = state.cast<DeviceState::CommandComponent>();
			subscribeTopic(commandState.commandTopicId, topic.command(), QOS);
		}

		pop();
	}
	if (!add) {
		if (entry("Delete Component")) {
			ComponentEditor editor(this->temp.device, this->tempState.device, this->tempIndex);
			auto &state = editor.getState();
			TopicBuffer topic = getRoomName() / device.getName();

			// unsubscribe old command topic
			if (state.is<DeviceState::CommandComponent>(component)) {
				auto &commandState = state.cast<DeviceState::CommandComponent>();
				unsubscribeTopic(commandState.commandTopicId, topic.command());
			}

			editor.erase();
			pop();
		}
	}
	if (entry("Cancel")) {
		pop();
	}
}

void RoomControl::clone(Interface &interface, Device &dstDevice, DeviceState &dstDeviceState,
	Device const &srcDevice, DeviceState const &srcDeviceState)
{
	// copy device
	{
		auto begin = reinterpret_cast<uint32_t *>(&dstDevice);
		auto end = begin + (srcDevice.getFlashSize() >> 2);
		array::copy(begin, end, reinterpret_cast<uint32_t const *>(&srcDevice));
	}
	
	// copy device state
	{
		auto begin = reinterpret_cast<uint32_t *>(&dstDeviceState);
		auto end = begin + (srcDevice.getRamSize() >> 2);
		array::copy(begin, end, reinterpret_cast<uint32_t const *>(&srcDeviceState));
	}
	
	// duplicate subscriptions to device topic
	addSubscriptionReference(dstDeviceState.deviceTopicId);
	
	// duplicate subscriptions to interface and mptt (command topics)
	for (ComponentIterator it(dstDevice, dstDeviceState); !it.atEnd(); it.next()) {
		auto &component = it.getComponent();
		auto &state = it.getState();

		// subscription to interface
		state.endpointId = 0;
		interface.subscribe(state.endpointId, dstDevice.deviceId, component.endpointIndex);

		// subscription to mqtt
		if (state.is<DeviceState::CommandComponent>(component)) {
			addSubscriptionReference(state.cast<DeviceState::CommandComponent>().commandTopicId);
		}
	}
}

void RoomControl::destroy(Interface &interface, Device const &device, DeviceState &deviceState) {
	TopicBuffer topic = getRoomName() / device.getName();

	// unsubscribe from device topic (enum/<room>/<device>)
	unsubscribeTopic(deviceState.deviceTopicId, topic.enumeration());

	// destroy components
	for (ComponentIterator it(device, deviceState); !it.atEnd(); it.next()) {
		auto &component = it.getComponent();
		auto &state = it.getState();
		
		// unsubscribe from interface
		interface.unsubscribe(state.endpointId, device.deviceId, component.endpointIndex);
		
		// unsubscribe from mqtt
		if (state.is<DeviceState::CommandComponent>(component)) {
			StringBuffer<8> b = component.getName();
			topic /= b;
			unsubscribeTopic(state.cast<DeviceState::CommandComponent>().commandTopicId, topic.command());
			topic.removeLast();
		}
	}
}
*/

// ComponentIterator
// -----------------
/*
void RoomControl::ComponentIterator::next() {
	auto type = getComponent().type;
	this->component += componentInfos[type].component.size;
	this->state += componentInfos[type].state.size;
	++this->componentIndex;
}


// ComponentEditor
// ---------------

RoomControl::ComponentEditor::ComponentEditor(Device &device, DeviceState &state, int index)
	: component(device.buffer), componentsEnd(array::end(device.buffer))
	, state(state.buffer), statesEnd(array::end(state.buffer))
{
	for (int i = 0; i < index; ++i) {
		auto type = getComponent().type;
		this->component += componentInfos[type].component.size;
		this->state += componentInfos[type].state.size;
	}
}

RoomControl::Device::Component &RoomControl::ComponentEditor::insert(Device::Component::Type type) {
	auto &componentInfo = componentInfos[type];
	
	// insert element
	array::insert(this->component, this->componentsEnd, componentInfo.component.size);
	array::insert(this->state, this->statesEnd, componentInfo.state.size);

	// clear ram
	array::fill(this->state, this->state + componentInfo.state.size, 0);

	// set type
	auto &component = *reinterpret_cast<Device::Component *>(this->component);
	component.type = type;
	return component;
}

void RoomControl::ComponentEditor::changeType(Device::Component::Type type) {
	auto oldType = getComponent().type;
	int oldComponentSize = componentInfos[oldType].component.size;
	int oldStateSize = componentInfos[oldType].state.size;

	int newComponentSize = componentInfos[type].component.size;
	int newStateSize = componentInfos[type].state.size;

	// reallocate flash
	if (newComponentSize > oldComponentSize)
		array::insert(this->component + oldComponentSize, this->componentsEnd, newComponentSize - oldComponentSize);
	else if (newComponentSize < oldComponentSize)
		array::erase(this->component + newComponentSize, this->componentsEnd, oldComponentSize - newComponentSize);
	
	// reallocate ram
	if (newStateSize > oldStateSize)
		array::insert(this->state + oldStateSize, this->statesEnd, newStateSize - oldStateSize);
	else if (newStateSize < oldStateSize)
		array::erase(this->state + newStateSize, this->statesEnd, oldStateSize - newStateSize);

	// clear ram behind base class
	array::fill(this->state + (sizeof(DeviceState::Component) + 3) / 4, this->state + newStateSize, 0);

	// set new type
	getComponent().type = type;
}

void RoomControl::ComponentEditor::erase() {
	auto &componentInfo = componentInfos[getComponent().type];

	array::erase(this->component, this->componentsEnd, componentInfo.component.size);
	array::erase(this->state, this->statesEnd, componentInfo.state.size);
}

/ *
void RoomControl::ComponentEditor::next() {
	auto type = getComponent().type;
	++this->componentIndex;
	this->component += componentInfos[type].component.size;
	this->state += componentInfos[type].state.size;
}
* /


// Interface
// ---------
/ *
void RoomControl::subscribeInterface(Interface &interface, Storage::Element<Device, DeviceState> e) {
	Device const &device = *e.flash;
	DeviceState &deviceState = *e.ram;

	// get list of endpoints (is empty if device not found)
	Array<EndpointType const> endpoints = interface.getEndpoints(device.deviceId);
	
	// iterate over device components
	ComponentIterator it(device, deviceState);
	while (!it.atEnd()) {
		auto &component = it.getComponent();
		auto &state = it.getState();

		// subscribe component to device endpoint
		int endpointIndex = component.endpointIndex;
		if (endpointIndex < endpoints.length) {
			// check endpoint type compatibility
			if (isCompatible(endpoints[endpointIndex], component.type)) {
				// subscribe
				interface.subscribe(state.endpointId, device.deviceId, endpointIndex);
			}
		}
		
		// next component
		it.next();
	}
}

void RoomControl::subscribeInterface(Interface &interface, Storage::Array<Device, DeviceState> &devices) {
	for (auto e : devices) {
		Device const &device = *e.flash;
		DeviceState &deviceState = *e.ram;
	
		// get list of endpoints (is empty if device not found)
		Array<EndpointType const> endpoints = interface.getEndpoints(device.deviceId);
		
		ComponentIterator it(device, deviceState);
		while (!it.atEnd()) {
			auto &component = it.getComponent();
			auto &state = it.getState();

			int endpointIndex = component.endpointIndex;
			if (endpointIndex < endpoints.length) {
				// check endpoint type compatibility
				if (isCompatible(endpoints[endpointIndex], component.type)) {
					// subscribe
					interface.subscribe(state.endpointId, device.deviceId, endpointIndex);
				}
			}
			
			// next element
			it.next();
		}
	}
}
* /

// Routes
// ------

int RoomControl::Route::getFlashSize() const {
	return getOffset(Route, buffer[this->srcTopicLength + this->dstTopicLength]);
}

int RoomControl::Route::getRamSize() const {
	return sizeof(RouteState);
}

void RoomControl::Route::setSrcTopic(String topic) {
	// move data of destination topic
	int oldSize = this->srcTopicLength;
	int newSize = topic.length;
	if (newSize > oldSize)
		array::insert(this->buffer, this->buffer + newSize + this->dstTopicLength, newSize - oldSize);
	else if (newSize < oldSize)
		array::erase(this->buffer, this->buffer + oldSize + this->dstTopicLength, oldSize - newSize);

	uint8_t *dst = this->buffer;
	array::copy(dst, dst + topic.length, topic.begin());
	this->srcTopicLength = newSize;
}

void RoomControl::Route::setDstTopic(String topic) {
	uint8_t *dst = this->buffer + this->srcTopicLength;
	array::copy(dst, dst + topic.length, topic.begin());
	this->dstTopicLength = topic.length;
}

void RoomControl::subscribeRoute(int index) {
	auto e = this->routes[index];
	Route const &route = *e.flash;
	RouteState &routeState = *e.ram;
	
	TopicBuffer topic = route.getSrcTopic();
	subscribeTopic(routeState.srcTopicId, topic.state(), QOS);
	
	topic = route.getDstTopic();
	registerTopic(routeState.dstTopicId, topic.command());
}

void RoomControl::clone(Route &dstRoute, RouteState &dstRouteState,
	Route const &srcRoute, RouteState const &srcRouteState)
{
	// copy route
	uint8_t *dst = reinterpret_cast<uint8_t *>(&dstRoute);
	uint8_t const *src = reinterpret_cast<uint8_t const *>(&srcRoute);
	array::copy(dst, dst + srcRoute.getFlashSize(), src);

	// clone state
	addSubscriptionReference(dstRouteState.srcTopicId = srcRouteState.srcTopicId);
	dstRouteState.dstTopicId = srcRouteState.dstTopicId;
}


// Command
// -------

void RoomControl::publishCommand(uint16_t topicId, Command::ValueType valueType, uint8_t const *value) {
	StringBuffer<16> b;
	switch (valueType) {
	case Command::ValueType::BUTTON:
		publish(topicId, buttonStates[1], 1);
		b += buttonStates[0];
		break;
	case Command::ValueType::SWITCH:
		b += switchStates[*value & 1];
		break;
	case Command::ValueType::ROCKER:
		publish(topicId, buttonStates[(*value & 1) + 1], 1);
		b += rockerStates[0];
		break;
	case Command::ValueType::VALUE8:
		b += dec(*value);
		break;
	case Command::ValueType::PERCENTAGE:
		b += flt(*value * 0.01f, 0, 2);
		break;
	case Command::ValueType::CELSIUS:
		b += flt(8.0f + *value * 0.1f, 0, 1);
		break;
	case Command::ValueType::COLOR_RGB:
		b += "rgb(" + dec(value[0]) + ',' + dec(value[1]) + ',' + dec(value[2]) + ')';
		break;
	}
	
	publish(topicId, b, 1);
}


// Timers
// ------

int RoomControl::Timer::getFlashSize() const {
	int size = 0;
	for (int i = 0; i < this->commandCount; ++i) {
		auto &command = *reinterpret_cast<Command const *>(this->commands + size);
		size += sizeof(Command) + command.topicLength + valueInfos[int(command.valueType)].size;
	}
	return getOffset(Timer, commands[size]);
}

int RoomControl::Timer::getRamSize() const {
	return getOffset(TimerState, commands[this->commandCount]);
}

Coroutine RoomControl::doTimers() {
	while (true) {
		co_await calendar::secondTick();
		
		// check for timer event
		auto now = calendar::now();
		if (now.getSeconds() == 0) {
			int minutes = now.getMinutes();
			int hours = now.getHours();
			int weekday = now.getWeekday();
			for (auto e : this->timers) {
				Timer const &timer = *e.flash;
				TimerState &timerState = *e.ram;
				
				ClockTime t = timer.time;
				if (t.getMinutes() == minutes && t.getHours() == hours && (t.getWeekdays() & (1 << weekday)) != 0) {
					// timer event: publish commands
					for (CommandIterator it(timer, timerState); !it.atEnd(); it.next()) {
						auto &command = it.getCommand();
						auto &state = it.getState();

						publishCommand(state.topicId, command.valueType, it.getValue());
					}
				}
			}
		}
	}
}

void RoomControl::subscribeTimer(int index) {
	auto e = this->timers[index];
	
	Timer const &timer = *e.flash;
	TimerState &timerState = *e.ram;
	
	for (CommandIterator it(timer, timerState); !it.atEnd(); it.next()) {
		auto &command = it.getCommand();
		auto &state = it.getState();

		// get topic
		TopicBuffer topic = it.getTopic();

		// register topic (so that we can publish on timer event)
		registerTopic(state.topicId, topic.command());
	}
}

void RoomControl::clone(Timer &dstTimer, TimerState &dstTimerState,
	Timer const &srcTimer, TimerState const &srcTimerState)
{
	// copy timer
	uint8_t *dst = reinterpret_cast<uint8_t *>(&dstTimer);
	uint8_t const *src = reinterpret_cast<uint8_t const *>(&srcTimer);
	array::copy(dst, dst + srcTimer.getFlashSize(), src);

	// clone state
	for (int i = 0; i < srcTimer.commandCount; ++i) {
		addSubscriptionReference(dstTimerState.commands[i].topicId = srcTimerState.commands[i].topicId);
	}
}


// CommandIterator
// ---------------

void RoomControl::CommandIterator::next() {
	++this->commandIndex;
	Command const &command = *reinterpret_cast<Command const *>(this->command);
	this->command += sizeof(Command) + command.topicLength + valueInfos[int(command.valueType)].size;
	++this->state;
}


// CommandEditor
// -------------

RoomControl::CommandEditor::CommandEditor(Timer &timer, TimerState &state, int index)
	: command(timer.commands), commandsEnd(array::end(timer.commands))
	, state(state.commands + index), statesEnd(array::end(state.commands))
{
	for (int i = 0; i < index; ++i) {
		Command &command = *reinterpret_cast<Command *>(this->command);
		this->command += sizeof(Command) + command.topicLength + valueInfos[int(command.valueType)].size;
	}
}

void RoomControl::CommandEditor::setTopic(String topic) {
	Command &command = getCommand();
	int oldSize = command.topicLength;
	int newSize = topic.length;
	
	// reallocate
	uint8_t *t = this->command  + sizeof(Command);
	if (newSize > oldSize)
		array::insert(t + oldSize, this->commandsEnd, newSize - oldSize);
	else if (newSize < oldSize)
		array::erase(t + newSize, this->commandsEnd, oldSize - newSize);

	// set new topic
	command.topicLength = newSize;
	array::copy(t, t + newSize, topic.data);
}

void RoomControl::CommandEditor::setValueType(Command::ValueType valueType) {
	Command &command = getCommand();
	int oldSize = valueInfos[int(command.valueType)].size;
	int newSize = valueInfos[int(valueType)].size;

	// reallocate
	uint8_t *value = this->command  + sizeof(Command) + command.topicLength;
	if (newSize > oldSize)
		array::insert(value + oldSize, this->commandsEnd, newSize - oldSize);
	else if (newSize < oldSize)
		array::erase(value + newSize, this->commandsEnd, oldSize - newSize);

	// set new value type and clear value
	command.valueType = valueType;
	array::fill(value, value + newSize, 0);
}

void RoomControl::CommandEditor::insert() {
	Command::ValueType valueType = Command::ValueType::BUTTON;
	int size = sizeof(Command) + 0 + valueInfos[int(valueType)].size;

	// insert element
	array::insert(this->command, this->commandsEnd, size);
	array::insert(this->state, this->statesEnd, 1);

	// clear ram
	*this->state = CommandState{};

	// set type
	auto &command = getCommand();
	command.topicLength = 0;
	command.valueType = valueType;
}

void RoomControl::CommandEditor::erase() {
	auto &valueInfo = valueInfos[int(getCommand().valueType)];

	array::erase(this->command, this->commandsEnd, valueInfo.size);
	array::erase(this->state, this->statesEnd, 1);
}

void RoomControl::CommandEditor::next() {
	auto &command = getCommand();
	this->command += sizeof(Command) + command.topicLength + valueInfos[int(command.valueType)].size;
	++this->state;
}



// Topic Selector
// --------------

void RoomControl::enterTopicSelector(String topic, bool onlyCommands, int index) {
	this->selectedTopic = topic;

	// check if current topic has the format <room>/<device>/<attribute>
	if (this->selectedTopic.getElementCount() == 3) {
		// remove attibute from topic
		this->selectedTopic.removeLast();
		this->topicDepth = 2;
	} else {
		// start at this room
		this->selectedTopic = getRoomName();
		this->topicDepth = 1;
	}

	// only list attributes that have a command topic
	this->onlyCommands = onlyCommands;

	// subscribe to selected topic
	subscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration(), QOS);

	// request enumeration of rooms, devices or attributes
	publish(this->selectedTopicId, String(), QOS);

	// enter topic selector menu
	push(SELECT_TOPIC);
	this->tempIndex = index;
}
*/

// Functions
// ---------

static void printDevices(Interface &interface) {
	for (int i = 0; i < interface.getDeviceCount(); ++i) {
		auto &device = interface.getDeviceByIndex(i);
		Terminal::out << (hex(device.getId()) + '\n');
		Array<Interface::EndpointType const> endpoints = device.getEndpoints();
		for (auto endpoint : endpoints) {
			Terminal::out << ('\t');
			switch (endpoint & Interface::EndpointType::TYPE_MASK) {
			case Interface::EndpointType::ON_OFF:
				Terminal::out << ("ON_OFF");
				break;
			case Interface::EndpointType::TRIGGER:
				Terminal::out << ("TRIGGER");
				break;
			case Interface::EndpointType::UP_DOWN:
				Terminal::out << ("UP_DOWN");
				break;
			case Interface::EndpointType::BATTERY_LEVEL:
				Terminal::out << ("BATTERY_LEVEL");
				break;
			default:
				Terminal::out << ("unknown");
				break;
			}
			if ((endpoint & Interface::EndpointType::DIRECTION_MASK) == Interface::EndpointType::IN)
				Terminal::out << (" IN");
			else
				Terminal::out << (" OUT");
			Terminal::out << ('\n');
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


Coroutine RoomControl::testSwitch() {
	Subscriber::Barrier barrier;

	printDevices(this->radioInterface);

	Subscriber subscriber;
	subscriber.subscriptionIndex = 0;
	subscriber.messageType = MessageType::ON_OFF;
	subscriber.barrier = &barrier;

	Publisher publisher;
	uint8_t message;
	publisher.messageType = MessageType::ON_OFF;
	publisher.message = &message;
	
	//Interface &interface = this->busInterface;
	Interface &interface = this->radioInterface;
	bool haveIn = false;
	bool haveOut = false;
	for (int i = 0; i < interface.getDeviceCount(); ++i) {
		auto &device = interface.getDeviceByIndex(i);
		Array<Interface::EndpointType const> endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < endpoints.count(); ++endpointIndex) {
			auto endpointType = endpoints[endpointIndex];
			if (!haveIn && (endpointType == Interface::EndpointType::ON_OFF_IN || endpointType == Interface::EndpointType::UP_DOWN_IN)) {
				haveIn = true;
				device.addSubscriber(endpointIndex, subscriber);
			}
			if (!haveOut && endpointType == Interface::EndpointType::ON_OFF_OUT) {
				haveOut = true;
				device.addPublisher(endpointIndex, publisher);
			}
		}
	}

	while (true) {
		// wait for event
		uint8_t index;
		co_await barrier.wait(index, &message);

		// publish
		publisher.publish();

		if (message <= 1)
			Output::set(0, message);
		else if (message == 2)
			Output::toggle(0);
	}
}

int RoomControl::connectFunction(RoomControl::FunctionFlash const &flash, Array<PlugInfo const> plugInfos,
	Array<Subscriber, MAX_INPUT_COUNT> subscribers, Subscriber::Barrier &barrier,
	Array<Publisher, MAX_OUTPUT_COUNT> publishers, Array<void const *> states)
{
	// count number of inputs and outputs
	int inputCount;
	for (inputCount = 0; plugInfos[inputCount].isInput(); ++inputCount);
	int outputCount = plugInfos.count() - inputCount;

	// number of states must be equal to number of outputs
	assert(outputCount == states.count());

	auto it = flash.getConnections();

	// connect inputs
	int inputIndex = 0;
	while (inputIndex < flash.connectionCount) {
		auto &connection = it.getConnection();

		// safety check
		if (connection.plugIndex >= plugInfos.count())
			break;

		// get plug info, break if not an input
		auto &info = plugInfos[connection.plugIndex];
		if (!info.isInput())
			break;

		// initialize subscriber
		auto &subscriber = subscribers[inputIndex];
		subscriber.subscriptionIndex = connection.plugIndex;
		subscriber.messageType = info.type;
		subscriber.convertOptions = connection.convertOptions;
		subscriber.barrier = &barrier;

		// subscribe to device
		auto &interface = getInterface(connection);
		auto *device = interface.getDeviceById(connection.deviceId);
		if (device != nullptr)
			device->addSubscriber(connection.endpointIndex, subscriber);

		++it;
		++inputIndex;
	}

	// connect outputs
	int outputIndex = 0;
	while (inputIndex + outputIndex < flash.connectionCount) {
		auto &connection = it.getConnection();

		// safety check
		if (connection.plugIndex >= plugInfos.count())
			break;

		// get plug info, break if not an output
		auto &info = plugInfos[connection.plugIndex];
		if (!info.isOutput())
			break;

		// initialize publisher
		auto &publisher = publishers[outputIndex];
		publisher.messageType = info.type;
		//publisher.
		publisher.message = states[connection.plugIndex - inputCount];

		// add publisher to device
		auto &interface = getInterface(connection);
		auto *device = interface.getDeviceById(connection.deviceId);
		if (device != nullptr)
			device->addPublisher(connection.endpointIndex, publisher);

		++it;
		++outputIndex;
	}

	// return number of outputs
	return outputIndex;
}

/*
Coroutine delayedSwitch(RoomControl &control, SystemDuration delay) {
	Barrier<Interface2::Parameters> barrier;

	// subscribe as BUTTON


	bool pressed = false;
	uint8_t state = 0;
	
	while (true) {
		uint8_t plugIndex;
		int length;
		uint8_t data[1];

		int selected = 1;
		if (!pressed)
			co_await waitlist.wait(plugIndex, length, data);
		else
			selected = co_await select(waitlist.wait(plugIndex, length, data), timer::delay(delay));

		uint8_t oldState = state;
		if (selected = 1) {
			if ((plugIndex ^ 0x80) < inEndpointCount)
				if (*data == 0) {
					// released
					pressed = false;
					state = 0;
				} else {
					// pressed
					pressed = true;
				}
			}
		} else {
			// long press timeout
			state = 1;
		}

		if (state != oldState) {
			// notify device endpoints
			control.sendEndpoint(outEndpointIds[i], &state, 1);
		}
	}
}

Coroutine simpleBlind(RoomControl &control, SwitchDevice &device) {
	Barrier<Interface2::Parameters> barrier;

	// subscribe button inputs as type ROCKER
	
	// subscribe relay outputs as type ROCKER
	
	uint8_t state = 0;
	
	while (true) {
		uint8_t plugIndex;
		int length;
		uint8_t data[1];

		SystemTime time = timer::now();

		co_await waitlist.wait(plugIndex, length, data);

		uint8_t oldState = state;
		if ((plugIndex ^ 0x80) < inEndpointCount)
			if (*data == 0) {
				// released: check if hold time elapsed
				if (timer::now > time + 1s) {
					// stop
					state = 0;
				}
			} else if (*data <= 2) {
				// pressed
				if (state == 0) {
					// currently stopped: up or down
					state = *data;
				} else {
					// currently movong: stop
					state = 0;
				}
			}
		}
		
		if (state != oldState) {
			// notify device endpoints
			control.sendEndpoint(outEndpointIds[i], &state, 1);
		}
	}
}

Coroutine RoomControl::trackedBlind(RoomControl &control, SwitchDevice &device) {
	Barrier<Interface2::Parameters> barrier;

	// subscribe button inputs as type ROCKER
	// subscriber percentage inputs as type PERCENTAGE_F
	
	// subscribe relay outputs as type ROCKER
	
	uint8_t state = 0;
	
	// current and end positions is given as durations
	SystemDuration current;
	SystemDuration end;
	
	while (true) {
		uint8_t plugIndex;
		int length;
		uint8_t data[4];

		int selected = 1;
		if (state == 0) {
			// stopped: wait for command
			co_await waitlist.wait(plugIndex, length, data);
		} else {
			// moving: calc duration until end
			SystemTime time = timer::now();
			SystemDuration d;
			if (state == 1) {
				// up
				d = end - current;
			} else {
				// down
				d = current - end;
			}
			
			// wait for command or hitting of end position
			selected = co_await select(waitlist.wait(plugIndex, length, data), timer::wait(time + d));
			
			// calc actual delay
			d = timer::now() - time;
			if (state == 1) {
				// up
				current += d;
				if (current > end)
					current = end;
				if (current > device.end)
					current = device.end;
			} else {
				// down
				current -= d;
				if (current < end)
					current = end;
				if (current < 0s)
					current = 0s;
			}
		}


		uint8_t oldState = state;
		if (selected == 1) {
			if ((plugIndex ^ 0x80) < inEndpointCount)
				if () {
					// rocker input
					if (*data == 0) {
						// released: check if hold time elapsed
						if (timer::now > time + 1s) {
							// stop
							state = 0;
						}
					} else if (*data <= 2) {
						// pressed
						if (state == 0) {
							// currently stopped: up or down
							state = *data;
							end = state == 1 ? -1s : device.end + 1s;
						} else {
							// currently movong: stop
							state = 0;
						}
					}
				} else {
					// percentage input
					float percentage = reinterpret_cast<float *>(data);
					
					if (percentage <= 0.0f)
						end = -1s;
					else if (percentage >= 1.0f)
						end = device.end + 1s;
					else
						end = device.end * percentage;
					
					if (end > current) {
						// need to move up
						state = 1;
					} else if (end < current) {
						// need to move down
						state = 2;
					} else {
						state = 0;
					}
				}
			}
		} else {
			// reached end: stop
			state = 0;
		}
		
		if (state != oldState) {
			// notify device endpoints
			control.sendEndpoint(outEndpointIds[i], &state, 1);
		}
	}
	
}

Coroutine RoomControl::heating() {

	// subscribe temperature input as type CELSIUS_F
	// subscribe standby inputs as type SWITCH
	
	// subscribe relay output as type SWITCH

	uint8_t state;
	float temperature = std::numeric_limits<float>::inf();

	while (true) {
		uint8_t plugIndex;
		int length;
		uint8_t data[4];
	
		
		int selected = co_await select(waitlist.wait(plugIndex, length, data), timer::wait(600s));

		if () {
			// temperature input
			
			temperature = *reinterpret_cast<float *>(data);
		} else if () {
			// standby input
		}

		uint8_t oldState = state;
		if (standby != 0) {
			if (state == 0) {
				if (temperature < device.temperature - 0.1f)
					state = 1;
			} else {
				if (temperature > device.temperature + 0.1f)
					state = 0;
			}
		} else {
			state = 0;
		}
		
		if (state != oldState) {
			// notify device endpoints
			control.sendEndpoint(outEndpointIds[i], &state, 1);
		}
	}
}
*/

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

static String getTypeName(EndpointType endpointType) {
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
		case Interface::EndpointType::ON_OFF:
			return "On/Off";
		case Interface::EndpointType::TRIGGER:
			return "Trigger";
		case Interface::EndpointType::UP_DOWN:
			return "Up/Down";

		case Interface::EndpointType::LEVEL:
			return "Level";

		case Interface::EndpointType::TEMPERATURE:
			return "Temperature";
		case Interface::EndpointType::AIR_PRESSURE:
			return "Air Pressure";
		case Interface::EndpointType::AIR_HUMIDITY:
			return "Air Humidity";
		case Interface::EndpointType::AIR_VOC:
			return "Air VOC";
		case Interface::EndpointType::ILLUMINANCE:
			return "Illuminance";

		case Interface::EndpointType::VOLTAGE:
			return "Voltage";
		case Interface::EndpointType::CURRENT:
			return "Current";
		case Interface::EndpointType::BATTERY_LEVEL:
			return "Battery Level";
		case Interface::EndpointType::ENERGY_COUNTER:
			return "Energy Counter";
		case Interface::EndpointType::POWER:
			return "Power";

		default:
			return "<unknown>";
	}
}


static int getComponentCount(EndpointType endpointType) {
	return endpointType == EndpointType::LEVEL ? 2 : 1;
}

static Interface::EndpointType nextIn(EndpointType endpointType, int delta) {
	static Interface::EndpointType const endpointTypes[] {
		EndpointType::ON_OFF_IN,
		EndpointType::TRIGGER_IN,
		EndpointType::UP_DOWN_IN,
		EndpointType::LEVEL_IN,

		EndpointType::TEMPERATURE_IN,
	};

	int index = array::binaryLowerBound(endpointTypes, [endpointType](Interface::EndpointType a) {return a < endpointType;});
	index = (index + array::count(endpointTypes) * 256 + delta) % array::count(endpointTypes);
	return endpointTypes[index];
}

void RoomControl::printPlug(Menu::Stream &stream, Connection const &plug) {
	auto &interface = getInterface(plug);
	auto device = interface.getDeviceById(plug.deviceId);
	if (device != nullptr) {
		String deviceName = device->getName();
		stream << deviceName << ':' << dec(plug.endpointIndex);
		auto endpoints = device->getEndpoints();
		if (plug.endpointIndex < endpoints.count()) {
			String endpointName = getTypeName(endpoints[plug.endpointIndex]);
			stream << " (" << endpointName << ')';
		}
	} else {
		stream << "Connect...";
	}
}

static MessageType getDefaultMessageType(Interface::EndpointType endpointType) {
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
		case Interface::EndpointType::ON_OFF:
		case Interface::EndpointType::TRIGGER:
		case Interface::EndpointType::UP_DOWN:
			return MessageType::COMMAND;
		case Interface::EndpointType::LEVEL:
			return MessageType::MOVE_TO_LEVEL;
		case Interface::EndpointType::TEMPERATURE:
			return MessageType::TEMPERATURE;
		case Interface::EndpointType::AIR_PRESSURE:
			return MessageType::PRESSURE;
		case Interface::EndpointType::AIR_HUMIDITY:
			return MessageType::AIR_HUMIDITY;
		case Interface::EndpointType::AIR_VOC:
			return MessageType::AIR_VOC;
		default:
			break;
	}
	return MessageType::UNKNOWN;
}

static ConvertOptions getDefaultConvertOptions(Interface::EndpointType endpointType) {
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
		case Interface::EndpointType::ON_OFF:
			return {.i32 = int(Command::OFF) | (int(Command::ON) << 3) | (int(Command::TOGGLE) << 6)};
		case Interface::EndpointType::TRIGGER:
			return {.i32 = int(Command::RELEASE) | (int(Command::TRIGGER) << 3)};
		case Interface::EndpointType::UP_DOWN:
			return {.i32 = int(Command::RELEASE) | (int(Command::UP) << 3) | (int(Command::DOWN) << 6)};
		default:
			break;
	}
	return {};
}

static Message getDefaultMessage(Interface::EndpointType endpointType) {
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
		case Interface::EndpointType::ON_OFF:
			return {.onOff = 0};
		case Interface::EndpointType::TRIGGER:
			return {.trigger = 0};
		case Interface::EndpointType::UP_DOWN:
			return {.upDown = 0};
		case Interface::EndpointType::TEMPERATURE:
			return {.temperature = 20.0f};
		case Interface::EndpointType::AIR_PRESSURE:
			return {.pressure = 1000.0f * 100.0f};
		default:
			break;
	}
	return {};
}
static Message getDefaultMessage(MessageType messageType) {
	switch (messageType) {
		case MessageType::COMMAND:
			return {.command = Command::ON};
		case MessageType::TEMPERATURE:
			return {.temperature = 20.0f};
		case MessageType::SET_TEMPERATURE:
			return {.setTemperature = 20.0f};
		case MessageType::PRESSURE:
			return {.pressure = 1000.0f * 100.0f};
		default:
			break;
	}
	return {};
}

float RoomControl::stepTemperature(float value, int delta) {
	return std::round(value * 2.0f + float(delta)) * 0.5f;
}

Flt RoomControl::displayTemperature(float kelvin) {
	return flt(kelvin - 273.15f, -1);
}

String RoomControl::getTemperatureUnit() {
	return "oC";
}

void RoomControl::editMessage(Menu::Stream &stream, Interface::EndpointType endpointType,
	Message &message, bool editMessage1, bool editMessage2, int delta)
{
	// edit and display message values
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
		case Interface::EndpointType::ON_OFF:
			if (editMessage1)
				message.onOff = (message.onOff + 30 + delta) % 3;
			stream << underline(onOffCommands[message.onOff], editMessage1);
			break;
		case Interface::EndpointType::TRIGGER:
			if (editMessage1)
				message.trigger = (message.trigger + delta) & 1;
			stream << underline(triggerCommands[message.trigger], editMessage1);
			break;
		case Interface::EndpointType::UP_DOWN:
			if (editMessage1)
				message.upDown = (message.upDown + 30 + delta) % 3;
			stream << underline(upDownCommands[message.upDown], editMessage1);
			break;
		case Interface::EndpointType::LEVEL:
			if (editMessage1)
				message.moveToLevel.level = clamp(float(message.moveToLevel.level) + delta * 0.05f, 0.0f, 1.0f);
			if (editMessage2)
				message.moveToLevel.move = clamp(float(message.moveToLevel.move) + delta * 0.5f, 0.0f, 100.0f);
			stream << underline(flt(message.moveToLevel.level, 2), editMessage1) << ' ';
			stream << underline(flt(message.moveToLevel.move, 1), editMessage2) << 's';
			break;
		case Interface::EndpointType::TEMPERATURE:
			if (editMessage1)
				message.temperature = stepTemperature(message.temperature, delta);
			stream << underline(displayTemperature(message.temperature), editMessage1) << getTemperatureUnit();
			break;
		case Interface::EndpointType::AIR_PRESSURE:
			if (editMessage1)
				message.pressure = message.pressure + delta;
			stream << underline(flt(message.pressure, 0), editMessage1) << "hPa";
			break;
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
		if (menu.entry("Actions"))
			co_await alarmActionsMenu(flash);

		if (index < alarms.getDeviceCount()) {
			auto &device = alarms.getDeviceByIndex(index);

			//if (menu.entry("Endpoints"))
			//	co_await endpointsMenu(device);

			if (menu.entry("Message Logger"))
				co_await messageLogger(device);

			//if (menu.entry("Message Generator"))
			//	co_await messageGenerator(device);

			// test alarm
			menu.stream() << "Test (-> " << dec(alarms.getSubscriberCount(index)) << ')';
			if (menu.entry()) {
				alarms.test(index, flash);
			}

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

AwaitableCoroutine RoomControl::alarmActionsMenu(AlarmInterface::AlarmFlash &flash) {
	Menu menu(this->swapChain);
	while (true) {
		// actions: messages to send when alarm goes off
		for (int i = 0; i < flash.endpointCount; ++i) {
			auto endpointType = flash.endpoints[i];

			// get component to edit
			int edit = menu.getEdit(1 + getComponentCount(endpointType));
			bool editEndpointType = edit == 1;
			bool editMessage1 = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			auto stream = menu.stream();

			// edit endpoint type
			if (editEndpointType) {
				endpointType = nextIn(endpointType, delta);
				flash.endpoints[i] = endpointType;
			}
			stream << underline(getTypeName(endpointType), editEndpointType);
			stream << ": ";

			// edit message
			editMessage(stream, endpointType, flash.messages[i], editMessage1, editMessage2, delta);
			menu.entry();
		}

		// add action
		if (flash.endpointCount < AlarmInterface::AlarmFlash::MAX_ENDPOINT_COUNT && menu.entry("Add")) {
			int i = flash.endpointCount;
			auto endpointType = Interface::EndpointType::ON_OFF_IN;
			flash.endpoints[i] = endpointType;
			flash.messages[i] = getDefaultMessage(endpointType);
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

AwaitableCoroutine RoomControl::endpointsMenu(Interface::Device &device) {
	Menu menu(this->swapChain);
	while (true) {
		Array<Interface::EndpointType const> endpoints = device.getEndpoints();
		for (int i = 0; i < endpoints.count(); ++i) {
			auto endpointType = endpoints[i];
			auto stream = menu.stream();
			stream << dec(i) << ": " << getTypeName(endpointType);
			if ((endpointType & Interface::EndpointType::DIRECTION_MASK) == Interface::EndpointType::IN)
				stream << " In";
			else
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
	Subscriber::Barrier barrier;

	Subscriber subscribers[32];

	// subscribe to all endpoints
	for (int endpointIndex = 0; endpointIndex < array::count(subscribers); ++endpointIndex) {
		auto &subscriber = subscribers[endpointIndex];
		subscriber.subscriptionIndex = endpointIndex;
		subscriber.barrier = &barrier;
		device.addSubscriber(endpointIndex, subscriber);
	}

	// event queue
	struct Event {
		uint8_t endpointIndex;
		EndpointType endpointType;
		MessageType messageType;
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
			stream << dec(event.endpointIndex) << ": ";
			switch (event.messageType) {
				case MessageType::COMMAND:
					stream << getCommandName(event.endpointType, event.message.command);
					break;

				case MessageType::TEMPERATURE:
					stream << flt(event.message.temperature, 1) << "oC";
					break;
				case MessageType::PRESSURE:
					stream << flt(event.message.temperature, 1) << "hPa";
					break;
				default:
					break;
			}
			menu.entry();
		}
		
		if (menu.entry("Exit"))
			break;

		// update subscribers
		Array<Interface::EndpointType const> endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < min(endpoints.count(), array::count(subscribers)); ++endpointIndex) {
			auto &subscriber = subscribers[endpointIndex];
			auto endpointType = endpoints[endpointIndex];
			subscriber.messageType = getDefaultMessageType(endpointType);
			subscriber.convertOptions = getDefaultConvertOptions(endpointType);
		}

		// get the empty event at the back of the queue
		Event &event = queue.getBack();

		// show menu or receive event (event gets filled in)
		int selected = co_await select(menu.show(), barrier.wait(event.endpointIndex, &event.message), Timer::sleep(250ms));
		if (selected == 2) {
			// received an event: add new empty event at the back of the queue
			event.endpointType = endpoints[event.endpointIndex];
			event.messageType = subscribers[event.endpointIndex].messageType;
			queue.addBack();
		}
	}
}

AwaitableCoroutine RoomControl::messageGenerator(Interface::Device &device) {
	//! skip "in" endpoints -> or forward to subscriptions
	Message message;

	uint8_t endpointIndex = 0;
	Publisher publisher;
	publisher.message = &message;
	
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		Array<Interface::EndpointType const> endpoints = device.getEndpoints();
		if (endpoints.count() > 0) {
			// get endpoint type
			endpointIndex = clamp(endpointIndex, 0, endpoints.count() - 1);
			auto endpointType = endpoints[endpointIndex];

			// get message component to edit (some messages such as MOVE_TO_LEVEL have two components)
			int edit = menu.getEdit(1 + getComponentCount(endpointType));
			bool editIndex = edit == 1;
			bool editMessage1 = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			// edit endpoint index
			if (editIndex) {
				endpointIndex = clamp(endpointIndex + delta, 0, endpoints.count() - 1);
				endpointType = endpoints[endpointIndex];
			}

			// update message type and reset message if type has changed
			auto messageType = getDefaultMessageType(endpointType);
			if (messageType != publisher.messageType) {
				publisher.messageType = messageType;
				message = getDefaultMessage(endpointType);
			}

			// edit message
			auto stream = menu.stream();
			stream << underline(dec(endpointIndex), editIndex) << ": ";
			editMessage(stream, endpointType, /*messageType,*/ message, editMessage1, editMessage2, delta);
			menu.entry();

			// send message
			if (menu.entry("Send")) {
				device.addPublisher(endpointIndex, publisher);
				publisher.publish();
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
		if (editType && delta != 0) {
			int typeCount = int(FunctionFlash::Type::TYPE_COUNT);
			flash.setType(FunctionFlash::Type((int(flash.type) + typeCount * 256 + delta) % typeCount));
		}

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
				auto &config = flash.getConfig<TimeoutSwitchConfig>();

				auto stream = menu.stream();
				stream << "Timeout: ";
				editDuration(menu, stream, config.duration);
				break;
			}
			case FunctionFlash::Type::TIMED_BLIND: {
				auto &config = flash.getConfig<TimedBlindConfig>();

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
						auto &info = functionInfo.plugs[connection.plugIndex];
						if (info.isOutput() && info.type == MessageType::UP_DOWN && !connection.isMqtt()) {
							auto &interface = getInterface(connection);
							auto *device = interface.getDeviceById(connection.deviceId);
							if (device != nullptr)
								co_await measureRunTime(*device, connection.endpointIndex, config.runTime);
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
			auto &info = functionInfo.plugs[plugIndex];
			menu.line();
			menu.stream() << (info.isInput() ? "Input:" : "Output:") << ' ' << info.name;
			menu.label();

			while (i < flash.connectionCount) {
				auto &plug = it.getConnection();
				if (plug.plugIndex != plugIndex)
					break;

				auto stream = menu.stream();
				printPlug(stream, plug);
				if (menu.entry())
					co_await editFunctionPlug(it, plug, info, false);

				++i;
				++it;
			}
			if (((info.isInput() && inputCount < MAX_INPUT_COUNT) || (info.isOutput() && outputCount < MAX_OUTPUT_COUNT))
				&& menu.entry("Connect..."))
			{
				Connection plug;
				plug.plugIndex = plugIndex;
				co_await editFunctionPlug(it, plug, info, true);
			}
			if (info.isInput())
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

AwaitableCoroutine RoomControl::measureRunTime(Interface::Device &device, uint8_t endpointIndex, uint16_t &runTime) {
	uint8_t state = 0;

	Publisher publisher;
	publisher.messageType = MessageType::UP_DOWN;
	publisher.message = &state;

	device.addPublisher(endpointIndex, publisher);

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
			publisher.publish();
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

static void editConvertOptions(Menu &menu, ConvertOptions &convertOptions, EndpointType endpointType,
	RoomControl::PlugInfo const &info, Array<const String> commandNames)
{
	int delta = menu.getDelta();
	switch (info.type) {
		case MessageType::COMMAND:
		case MessageType::INDEXED_COMMAND:
			for (int i = 0; i < commandNames.count(); ++i) {
				int supportedFlags = info.flags /*flag(Command::OFF) | flag(Command::ON) | flag(Command::TOGGLE)*/ | 1 << 7;
				auto stream = menu.stream();
				stream << commandNames[i] << " -> ";

				int offset = i * 3;
				int command = (convertOptions.i32 >> offset) & 7;
				bool editCommand = menu.getEdit(1) == 1;
				while (editCommand && delta != 0) {
					if (delta > 0) {
						command = (command + 1) & 7;
						if ((supportedFlags & (1 << command)) != 0)
							--delta;
					} else {
						command = (command - 1) & 7;
						if ((supportedFlags & (1 << command)) != 0)
							++delta;
					}
				}
				convertOptions.i32 = (convertOptions.i32 & ~(7 << offset))
					| command << offset;

				stream << underline(command < 7 ? getCommandName(endpointType, Command(command)) : "(discard)", editCommand);
				menu.entry();
			}
			break;
		//case MessageType::SET_TEMPERATURE:
		//	return 1;
		default:
			;
	}
}

AwaitableCoroutine RoomControl::editFunctionPlug(ConnectionIterator &it, Connection &connection, PlugInfo const &info,
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
			printPlug(stream, connection);
			if (menu.entry())
				co_await selectFunctionDevice(connection, info);
		}

		// select convert options
		menu.line();
		auto *device = getInterface(connection).getDeviceById(connection.deviceId);
		if (device != nullptr) {
			auto endpoints = device->getEndpoints();
			auto endpointType = endpoints[connection.endpointIndex];

			if (info.isInput()) {
				switch (endpointType) {
					case EndpointType::ON_OFF_IN:
						editConvertOptions(menu, connection.convertOptions, endpointType, info, onOffCommands);
						break;
					case EndpointType::TRIGGER_IN:
						editConvertOptions(menu, connection.convertOptions, endpointType, info, triggerCommands);
						break;
					case EndpointType::UP_DOWN_IN:
						editConvertOptions(menu, connection.convertOptions, endpointType, info, upDownCommands);
						break;
						/*
						switch (info.type) {
							case MessageType::COMMAND:
							case MessageType::INDEXED_COMMAND:
								for (int i = 0; i < 3; ++i) {
									int supportedFlags = flag(Command::OFF) | flag(Command::ON) | flag(Command::TOGGLE) | 1 << 7;
									auto stream = menu.stream();
									stream << upDownCommands[i] << " -> ";

									int offset = i * 3;
									int command = (connection.convertOptions.i32 >> offset) & 7;
									bool editCommand = menu.getEdit(1) == 1;
									while (editCommand && delta != 0) {
										if (delta > 0) {
											command = (command + 1) & 7;
											if ((supportedFlags & (1 << command)) != 0)
												--delta;
										} else {
											command = (command - 1) & 7;
											if ((supportedFlags & (1 << command)) != 0)
												++delta;
										}
									}
									connection.convertOptions.i32 = (connection.convertOptions.i32 & ~(7 << offset))
										| command << offset;

									String c;
									switch (command) {
										case 0:
											c = "Off";
											break;
										case 1:
											c = "On";
											break;
										case 2:
											c = "Toggle";
											break;
										case 3:
											c = "Up";
											break;
										case 4:
											c = "Down";
											break;
										case 5:
											c = "Release";
											break;
										default:
											c = "(discard)";
									}
									stream << underline(c, editCommand);
									menu.entry();
								}
								break;
							//case MessageType::SET_TEMPERATURE:
							//	return 1;
							default:
								;
						}*/
						break;
					case EndpointType::TEMPERATURE_IN:
						//return dstType == MessageType::TEMPERATURE ? 1 : 0;
						break;
					default:
						;
				}
			}
			menu.line();


			/*
			int modeCount = isCompatible(info, endpoints[plug.endpointIndex]);
			if (modeCount > 1) {
				// edit mode
				bool editMode = menu.getEdit(1) == 1;
				if (editMode)
					plug.setMode((plug.getMode() + modeCount * 256 + delta) % modeCount);

				// mode
				int mode = plug.getMode();
				static String const modes[4] = {"Default", "Inverse Default", "Alternate", "Inverse Alternate"};
				menu.stream() << "Mode: " << underline(modes[mode], editMode);
				menu.entry();
			}*/
		}

		if (add) {
			if (menu.entry("Cancel"))
				break;
		} else {
			if (menu.entry("Erase")) {
				it.erase();
				break;
			}
		}

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

AwaitableCoroutine RoomControl::selectFunctionDevice(Connection &plug, PlugInfo const &info) {
	auto &interface = getInterface(plug);

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
				if (isCompatible(info, endpointType) != 0) {
					if (menu.entry(device.getName())) {
						co_await selectFunctionEndpoint(device, plug, info);
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
	PlugInfo const &info)
{
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		int delta = menu.getDelta();

		// list all compatible endpoints of device
		auto endpoints = device.getEndpoints();
		for (int i = 0; i < endpoints.count(); ++i) {
			auto endpointType = endpoints[i];
			if (isCompatible(info, endpointType) != 0) {
				// endpoint is compatible
				String endpointName = getTypeName(endpointType);
				menu.stream() << dec(i) << " (" << endpointName << ')';
				if (menu.entry()) {
					// endpoint selected
					connection.deviceId = device.getId();
					connection.endpointIndex = i;

					// set default convert options
					switch (endpointType) {
						case EndpointType::ON_OFF_IN:
							connection.convertOptions.i32 =
								selectCommand(info.flags, Command::OFF, Command::DOWN)
								| selectCommand(info.flags, Command::ON, Command::UP) << 3
								| selectCommand(info.flags, Command::TOGGLE) << 6;
							break;
						case EndpointType::TRIGGER_IN:
							connection.convertOptions.i32 =
								selectCommand(info.flags, Command::RELEASE)
									| selectCommand(info.flags, Command::TRIGGER, Command::ON) << 3;
							break;
						case EndpointType::UP_DOWN_IN: {
							connection.convertOptions.i32 =
								selectCommand(info.flags, Command::RELEASE)
									| selectCommand(info.flags, Command::UP, Command::ON) << 3
									| selectCommand(info.flags, Command::DOWN, Command::OFF) << 6;
							break;
						}
					}

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
	switch (type) {
		case RoomControl::FunctionFlash::Type::SIMPLE_SWITCH:
			return 0;
		case RoomControl::FunctionFlash::Type::TIMEOUT_SWITCH:
			return sizeof(RoomControl::TimeoutSwitchConfig);
		case RoomControl::FunctionFlash::Type::TIMED_BLIND:
			return sizeof(RoomControl::TimedBlindConfig);
	}
	assert(false);
	return 0;
}

void RoomControl::FunctionFlash::setType(Type type) {
	int oldSize = (getSize(this->type) + 3) / 4;
	int newSize = (getSize(type) + 3) / 4;
	this->type = type;

	// resize config
	int d = newSize - oldSize;
	if (d > 0)
		array::insert(this->bufferSize + d - this->configOffset, this->buffer + this->configOffset, d);
	else if (d < 0)
		array::erase(this->bufferSize - this->configOffset, this->buffer + this->configOffset, -d);

	// clear config
	array::fill(newSize, this->buffer + this->configOffset, 0);

	// adjust offsets
	this->connectionsOffset += d;
	this->bufferSize += d;
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
	}
	assert(false);
	return nullptr;
}


// Function

RoomControl::Function::~Function() {
	this->coroutine.destroy();
}


// SimpleSwitch

RoomControl::SimpleSwitch::~SimpleSwitch() {
}

Coroutine RoomControl::SimpleSwitch::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	Subscriber::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	uint8_t state = 0;
	void const *states[] = {&state};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers, states);

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		uint8_t inputIndex;
		uint8_t message;
		co_await barrier.wait(inputIndex, &message);

		// process message
		if (inputIndex == 0) {
			// on/off
			if (message < 2)
				state = message;
			else if (message == 2)
				state ^= 1;
		}

		// publish
		for (int i = 0; i < outputCount; ++i)
			publishers[i].publish();
	}
}


// TimeoutSwitch

RoomControl::TimeoutSwitch::~TimeoutSwitch() {
}

Coroutine RoomControl::TimeoutSwitch::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	Subscriber::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	uint8_t state = 0;
	void const *states[] = {&state};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers, states);

	auto duration = (*this)->getConfig<TimeoutSwitchConfig>().duration;
	//SystemTime time;
	//uint32_t duration;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		uint8_t inputIndex;
		uint8_t message;
		if (state == 0) {
			co_await barrier.wait(inputIndex, &message);
		} else {
			auto timeout = Timer::now() + (duration / 100) * 1s + ((duration % 100) * 1s) / 100;
			int s = co_await select(barrier.wait(inputIndex, &message), Timer::sleep(timeout));
			if (s == 2) {
				// switch off
				state = 0;
				inputIndex = -1;
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
		if (inputIndex == 0) {
			// on/off
			if (message < 2)
				state = message;
			else if (message == 2)
				state ^= 1;
		}
/*
		// start timeout
		if (state == 1) {
			time = Timer::now();
			duration = config.duration;
		}
*/
		// publish
		for (int i = 0; i < outputCount; ++i)
			publishers[i].publish();
	}
}


// TimedBlind

RoomControl::TimedBlind::~TimedBlind() {
}

Coroutine RoomControl::TimedBlind::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	Subscriber::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	uint8_t state = 0;
	FloatWithFlag outPosition = 0.0f;
	void const *states[] = {&state, &outPosition};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, blindPlugs, subscribers, barrier, publishers, states);

	// rocker timeout
	SystemDuration const holdTime = ((*this)->getConfig<TimedBlindConfig>().holdTime * 1s) / 100;

	// position
	SystemDuration const maxPosition = ((*this)->getConfig<TimedBlindConfig>().runTime * 1s) / 100;
	SystemDuration position = maxPosition / 2;
	SystemDuration targetPosition = 0s;
	SystemTime startTime;
	bool up = false;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		uint8_t inputIndex;
		union {
			Command command;
			FloatWithFlag level;
		} message;
		if (state == 0) {
			co_await barrier.wait(inputIndex, &message);
		} else {
			bool up = state == 1;
			auto d = targetPosition - position;
			int s = co_await select(barrier.wait(inputIndex, &message), Timer::sleep(up ? -d : d));
			if (s == 1) {
				// new event while moving
				d = Timer::now() - startTime;
				if (up) {
					position -= d;
					if (position <= targetPosition)
						position = targetPosition;
				} else {
					position += d;
					if (position >= targetPosition)
						position = targetPosition;
				}
			} else if (s == 2) {
				// target position reached
				position = targetPosition;
				inputIndex = -1;
			}
		}

		// process message
		if (inputIndex == 0) {
			// up/down
			if (message.command == Command::RELEASE) {
				// released: stop if timeout elapsed
				if (Timer::now() > startTime + holdTime)
					targetPosition = position;
			} else {
				// up or down pressed
				if (state == 0) {
					// start if currently stopped
					if (message.command == Command::UP) {
						// up
						targetPosition = 0s;
					} else if (message.command == Command::DOWN) {
						// down
						targetPosition = maxPosition;
					} else {
						// trigger
						targetPosition = ((up || (targetPosition == 0s)) && targetPosition < maxPosition) ? maxPosition : 0s;
					}
				} else {
					// stop if currently moving
					targetPosition = position;
				}
			}
		} else if (inputIndex == 1) {
			// position
			targetPosition = maxPosition * message.level;
		}

		// check if target position already reached
		if (position == targetPosition) {
			state = 0;
		} else {
			state = (up = targetPosition < position) ? 1 : 2;
			startTime = Timer::now();
		}

		// publish
		for (int i = 0; i < outputCount; ++i)
			publishers[i].publish();
	}
}


// HeatingRegulator

RoomControl::HeatingControl::~HeatingControl() {
}

Coroutine RoomControl::HeatingControl::start(RoomControl &roomControl) {
	Subscriber subscribers[MAX_INPUT_COUNT];
	Subscriber::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	uint8_t valve = 0;
	void const *states[] = {&valve};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, blindPlugs, subscribers, barrier, publishers, states);

	uint8_t onOff = 0;
	uint8_t night = 0;
	uint8_t summer = 0;
	float setTemperature = 20.0f + 273.15f;
	float temperature = 20.0f + 273.15f;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		uint8_t inputIndex;
		union {
			// main on/off switch
			uint8_t onOff;

			// night setback
			uint8_t night;

			// summer mode
			uint8_t summer;

			// current (measured) temperature
			float temperature;

			// set value of temperature (absolute or incremental)
			FloatWithFlag setTemperature;
		} message;

		co_await select(barrier.wait(inputIndex, &message), Timer::sleep(1s));


		switch (inputIndex) {
			case 0: // on/off
				onOff = message.onOff;
				break;
			case 1: // night
				night = message.night;
				break;
			case 2: // summer
				summer = message.summer;
				break;
			case 3:
				// set temperature
				if (message.setTemperature.getFlag())
					setTemperature += message.setTemperature;
				else
					setTemperature = message.setTemperature;
				break;
			case 4:
				// temperature
				temperature = message.temperature;
				break;
		}

		// determine state of valve (simple two-position controller)
		if (valve == 0) {
			// switch on when current temperature below set temperature
			if (temperature < setTemperature - 0.2f)
				valve = 1;
		} else {
			if (temperature > setTemperature + 0.2f)
				valve = 0;
		}

	}
}

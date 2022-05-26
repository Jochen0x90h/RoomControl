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

// functions

// function plugs
static RoomControl::Plug const switchPlugs[] = {
	{"Switch", MessageType::OFF_ON_TOGGLE_IN},
	{"Switch", MessageType::OFF_ON_OUT}};
static RoomControl::Plug const blindPlugs[] = {
	{"Up/Down", MessageType::UP_DOWN_IN},
	{"Trigger", MessageType::TRIGGER_IN},
	{"Position", MessageType::SET_LEVEL_IN},
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

static bool isCompatible(RoomControl::Plug const &plug, MessageType messageType) {
	if (plug.isInput() && isInput(messageType))
		return isCompatible(plug.messageType, messageType);
	else if (plug.isOutput() && isOutput(messageType))
		return isCompatible(messageType, plug.messageType);
	return false;
}


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
		Array<MessageType const> endpoints = device.getEndpoints();
		for (auto endpoint : endpoints) {
			Terminal::out << '\t' << getTypeName(endpoint);
			/*switch (endpoint & MessageType::TYPE_MASK) {
			case MessageType::ON_OFF:
				Terminal::out << "ON_OFF";
				break;
			case MessageType::ON_OFF_TOGGLE:
				Terminal::out << "ON_OFF_TOGGLE";
				break;
			case MessageType::TRIGGER:
				Terminal::out << "TRIGGER";
				break;
			case MessageType::UP_DOWN:
				Terminal::out << "UP_DOWN";
				break;
			case MessageType::UP_DOWN_TRIGGER:
				Terminal::out << "UP_DOWN_TRIGGER";
				break;
			case MessageType::BATTERY_LEVEL:
				Terminal::out << "BATTERY_LEVEL";
				break;
			default:
				Terminal::out << "unknown";
				break;
			}*/
			if ((endpoint & MessageType::DIRECTION_MASK) == MessageType::IN)
				Terminal::out << " In";
			else
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

int RoomControl::connectFunction(RoomControl::FunctionFlash const &flash, Array<Plug const> plugs,
	Array<Subscriber, MAX_INPUT_COUNT> subscribers, Subscriber::Barrier &barrier,
	Array<Publisher, MAX_OUTPUT_COUNT> publishers, Array<void const *> states)
{
	// count number of inputs and outputs
	int inputCount;
	for (inputCount = 0; plugs[inputCount].isInput(); ++inputCount);
	int outputCount = plugs.count() - inputCount;

	// number of states must be equal to number of outputs
	assert(outputCount == states.count());

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
		auto &info = plugs[connection.plugIndex];
		if (!info.isInput())
			break;

		if (connection.plugIndex != lastPlugIndex) {
			lastPlugIndex = connection.plugIndex;
			connectionIndex = 0;
		}

		// initialize subscriber
		auto &subscriber = subscribers[inputIndex];
		subscriber.source.plugIndex = connection.plugIndex;
		subscriber.source.connectionIndex = connectionIndex;
		subscriber.messageType = info.messageType;
		subscriber.convertOptions = connection.convertOptions;
		subscriber.barrier = &barrier;

		// subscribe to device
		auto &interface = getInterface(connection);
		auto *device = interface.getDeviceById(connection.deviceId);
		if (device != nullptr)
			device->addSubscriber(connection.endpointIndex, subscriber);

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
		auto &info = plugs[connection.plugIndex];
		if (!info.isOutput())
			break;

		// initialize publisher
		auto &publisher = publishers[outputIndex];
		publisher.messageType = info.messageType;
		publisher.message = states[connection.plugIndex - inputCount];
		publisher.convertOptions = connection.convertOptions;

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

static int getComponentCount(MessageType messageType) {
	return (messageType & MessageType::TYPE_MASK) == MessageType::MOVE_TO_LEVEL ? 2 : 1;
}

// next message type for alarms
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
}

void RoomControl::printConnection(Menu::Stream &stream, Connection const &connection) {
	auto &interface = getInterface(connection);
	auto device = interface.getDeviceById(connection.deviceId);
	if (device != nullptr) {
		String deviceName = device->getName();
		stream << deviceName << ':' << dec(connection.endpointIndex);
		auto endpoints = device->getEndpoints();
		if (connection.endpointIndex < endpoints.count()) {
			String endpointName = getTypeName(endpoints[connection.endpointIndex]);
			stream << " (" << endpointName << ')';
		}
	} else {
		stream << "Connect...";
	}
}

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

inline ConvertOptions makeConvertOptions(int16_t commands, float value1, float value2) {
	return {.commands = uint16_t(commands), .value = {.f = {value1, value2}}};
}

// default convert options for message logger and generator
static ConvertOptions getDefaultConvertOptions(MessageType messageType) {
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
	}
	return {};
}

// default convert options for function connection
static ConvertOptions getDefaultConvertOptions(MessageType dstType, MessageType srcType) {
	dstType = dstType & MessageType::TYPE_MASK;
	srcType = srcType & MessageType::TYPE_MASK;

	// check if both are a switch command
	auto d = uint(dstType) - uint(MessageType::OFF_ON);
	auto s = uint(srcType) - uint(MessageType::OFF_ON);
	bool dstIsCommand = d < SWITCH_COMMAND_COUNT;
	bool srcIsCommand = s < SWITCH_COMMAND_COUNT;
	if (dstIsCommand && srcIsCommand)
		return {.commands = uint16_t(command2command[s][d])};

	// check if source is a switch command and destination is a set command
	if (srcIsCommand) {
		switch (dstType) {
		case MessageType::SET_LEVEL:
			return makeConvertOptions(command2setValue[s], 0.5f, 0.1f);
		case MessageType::SET_AIR_TEMPERATURE:
			return makeConvertOptions(command2setValue[s], 293.15f, 0.5f);
		default:;
		}
	}

	// check if source is a simple value and destination is a switch command
	// todo units
	if (dstIsCommand) {
		switch (srcType) {
		case MessageType::LEVEL:
			return makeConvertOptions(value2command[d], 0.5f, 0.5f);
		case MessageType::AIR_TEMPERATURE:
			return makeConvertOptions(value2command[d], 293.15f + 0.5f, 293.15f - 0.5f);
		case MessageType::AIR_HUMIDITY:
			return makeConvertOptions(value2command[d], 0.6f, 0.01f);
		case MessageType::AIR_PRESSURE:
			return makeConvertOptions(value2command[d], 1000.0f * 100.0f, 100.0f);
		case MessageType::AIR_VOC:
			return makeConvertOptions(value2command[d], 10.0f, 1.0f);
		default:;
		}
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

static void editCommand(Menu::Stream &stream, Message &message, bool editMessage, int delta, Array<uint8_t const> commands) {
	if (editMessage)
		message.command = (message.command + 30 + delta) % commands.count();
	stream << underline(commandNames[commands[message.command]], editMessage);
}

void RoomControl::editMessage(Menu::Stream &stream, MessageType messageType,
	Message &message, bool editMessage1, bool editMessage2, int delta)
{
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

			if (menu.entry("Message Logger"))
				co_await messageLogger(device);

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

AwaitableCoroutine RoomControl::endpointsMenu(Interface::Device &device) {
	Menu menu(this->swapChain);
	while (true) {
		Array<MessageType const> endpoints = device.getEndpoints();
		for (int i = 0; i < endpoints.count(); ++i) {
			auto messageType = endpoints[i];
			auto stream = menu.stream();
			stream << dec(i) << ": " << getTypeName(messageType);
			if ((messageType & MessageType::DIRECTION_MASK) == MessageType::IN)
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
		subscriber.source = {uint8_t(endpointIndex), 0};
		subscriber.barrier = &barrier;
		device.addSubscriber(endpointIndex, subscriber);
	}

	// event queue
	struct Event {
		Subscriber::Source source;
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
			stream << dec(event.source.plugIndex) << ": ";
			switch (event.messageType & MessageType::TYPE_MASK) {
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
				default:
					break;
			}
			menu.entry();
		}
		
		if (menu.entry("Exit"))
			break;

		// update subscribers
		Array<MessageType const> endpoints = device.getEndpoints();
		for (int endpointIndex = 0; endpointIndex < min(endpoints.count(), array::count(subscribers)); ++endpointIndex) {
			auto &subscriber = subscribers[endpointIndex];
			auto messageType = endpoints[endpointIndex];
			subscriber.messageType = messageType;
			subscriber.convertOptions = getDefaultConvertOptions(messageType);
		}

		// get the empty event at the back of the queue
		Event &event = queue.getBack();

		// show menu or receive event (event gets filled in)
		int selected = co_await select(menu.show(), barrier.wait(event.source, &event.message), Timer::sleep(250ms));
		if (selected == 2) {
			// received an event: add new empty event at the back of the queue
			event.messageType = subscribers[event.source.plugIndex].messageType;
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
		Array<MessageType const> endpoints = device.getEndpoints();
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

			// update message type and reset message if type has changed
			if (messageType != publisher.messageType) {
				publisher.messageType = messageType;
				message = getDefaultMessage(messageType);
				publisher.convertOptions = getDefaultConvertOptions(messageType);
			}

			// edit message
			auto stream = menu.stream();
			stream << underline(dec(endpointIndex), editIndex) << ": ";
			editMessage(stream, messageType, message, editMessage1, editMessage2, delta);
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
						auto &info = functionInfo.plugs[connection.plugIndex];
						if (info.messageType == MessageType::UP_DOWN_OUT && !connection.isMqtt()) {
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
				if (menu.entry())
					co_await editFunctionConnection(it, connection, plug, false);

				++i;
				++it;
			}

			// add new connection if maximum connection count not reached
			if (((plug.isInput() && inputCount < MAX_INPUT_COUNT) || (plug.isOutput() && outputCount < MAX_OUTPUT_COUNT))
				&& menu.entry("Connect..."))
			{
				Connection connection;
				connection.plugIndex = plugIndex;
				co_await editFunctionConnection(it, connection, plug, true);
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

AwaitableCoroutine RoomControl::measureRunTime(Interface::Device &device, Connection &connection, uint16_t &runTime) {
	uint8_t state = 0;

	Publisher publisher;
	publisher.messageType = MessageType::UP_DOWN_OUT;
	publisher.message = &state;
	publisher.convertOptions = connection.convertOptions;

	device.addPublisher(connection.endpointIndex, publisher);

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
static int editDestinationCommand(Menu::Stream &stream, ConvertOptions &convertOptions, int index,
	Array<uint8_t const> const &dstCommands, bool editCommand, int delta)
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

	stream << underline(command < commandCount ? commandNames[dstCommands[command]] : "(ignore)", editCommand);
	return command;
}

static void editConvertSwitch2Switch(Menu &menu, ConvertOptions &convertOptions,
	Array<uint8_t const> const &dstCommands, Array<uint8_t const> const &srcCommands)
{
	int delta = menu.getDelta();

	// one menu entry per source command
	for (int i = 0; i < srcCommands.count(); ++i) {
		auto stream = menu.stream();

		// source command
		stream << commandNames[srcCommands[i]] << " -> ";

		// edit destination command
		bool editCommand = menu.getEdit(1) == 1;
		editDestinationCommand(stream, convertOptions, i, dstCommands, editCommand, delta);

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << srcCommands.count() * 3;
}

static void editConvertSwitch2SetFloat(Menu &menu, ConvertOptions &convertOptions,
	Array<uint8_t const> const &srcCommands, float scale, float offset, float precision, int decimalCount, float maxValue, String unit)
{
	static_assert(array::count(setCommands) <= ConvertOptions::MAX_VALUE_COUNT);

	int delta = menu.getDelta();

	// one menu entry per source command
	for (int i = 0; i < srcCommands.count(); ++i) {

		auto stream = menu.stream();

		// source command
		stream << commandNames[srcCommands[i]] << " -> ";
		//menu.label();

		//auto stream = menu.stream();

		// edit destination command
		bool editCommand = menu.getEdit(2) == 1;
		int lastCommand = convertOptions.getCommand(i);
		int command = editDestinationCommand(stream, convertOptions, i, setCommands, editCommand, delta);
		if (command != lastCommand) {
			if (command == 0 && lastCommand != 0)
				convertOptions.value.f[i] += offset;
			if (command != 0 && lastCommand == 0)
				convertOptions.value.f[i] -= offset;
		}

		if (command < array::count(setCommands)) {
			float o = command == 0 ? offset : 0.0f;

			// edit set-value
			bool editValue = menu.getEdit(2) == 2;
			if (editValue) {
				convertOptions.value.f[i] = clamp(
					(float(iround((convertOptions.value.f[i] * scale + o) * precision) + delta)
						/ precision - o) / scale, 0.0f, maxValue);
			}

			// set-value
			stream << ' ' << underline(flt(convertOptions.value.f[i] * scale + o, decimalCount), editValue)
				<< ' ' << unit;
		} else {
			// ignore command: no value to edit
			menu.getEdit(1);
		}

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << srcCommands.count() * 3;
}

static void editConvertFloat2Switch(Menu &menu, ConvertOptions &convertOptions,
	Array<uint8_t const> const &dstCommands, float scale, float offset, float precision, int decimalCount, float maxValue, String unit) {

	int delta = menu.getDelta();

	// one menu entry per source command
	for (int i = 0; i < array::count(compareCommands); ++i) {
		auto stream = menu.stream();

		// source command
		stream << commandNames[compareCommands[i]];

		if (i <= 1) {
			// edit value
			bool editValue = menu.getEdit(2) == 1;
			if (editValue) {
				convertOptions.value.f[i] = clamp(
					(float(iround((convertOptions.value.f[i] * scale + offset) * precision) + delta)
						/ precision - offset) / scale, 0.0f, maxValue);
			}

			// value
			stream << ' ' << underline(flt(convertOptions.value.f[i] * scale + offset, decimalCount), editValue)
				<< ' ' << unit;
		}
		stream << " -> ";

		// edit destination command
		bool editCommand = menu.getEdit(2) == 2;
		editDestinationCommand(stream, convertOptions, i, dstCommands, editCommand, delta);

		menu.entry();
	}

	// set remaining commands to invalid
	convertOptions.commands |= -1 << array::count(compareCommands) * 3;
}

static void editConvertOptions(Menu &menu, ConvertOptions &convertOptions, MessageType dstType, MessageType srcType) {
	dstType = dstType & MessageType::TYPE_MASK;
	srcType = srcType & MessageType::TYPE_MASK;

	// check if both are a switch command
	auto d = uint(dstType) - uint(MessageType::OFF_ON);
	auto s = uint(srcType) - uint(MessageType::OFF_ON);
	bool dstIsCommand = d < SWITCH_COMMAND_COUNT;
	bool srcIsCommand = s < SWITCH_COMMAND_COUNT;
	if (dstIsCommand && srcIsCommand) {
		editConvertSwitch2Switch(menu, convertOptions, switchCommands[d], switchCommands[s]);
		return;
	}

	// check if source is a switch command and destination is a set command
	if (srcIsCommand) {
		switch (dstType) {
		case MessageType::SET_LEVEL:
			editConvertSwitch2SetFloat(menu, convertOptions, switchCommands[s], 1.0f, 0.0f, 100.0f, -2, 1.0f, String());
			return;
		case MessageType::MOVE_TO_LEVEL:
			return;
		case MessageType::SET_AIR_TEMPERATURE:
			editConvertSwitch2SetFloat(menu, convertOptions, switchCommands[s], 1.0f, -273.15f, 2.0f, -1, 10000.0f, "oC");
			return;
		default:;
		}
	}

	// check if source is a simple value and destination is a switch command
	if (dstIsCommand) {
		switch (srcType) {
		case MessageType::LEVEL:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, 0.0f, 100.0f, -2, 1.0f, String());
			return;
		case MessageType::AIR_TEMPERATURE:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, -273.15f, 2.0f, -1, 10000.0f, "oC");
			return;
		case MessageType::AIR_HUMIDITY:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 100.0f, 0.0f, 1.0f, 0, 1.0f, "%");
			return;
		case MessageType::AIR_PRESSURE:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 0.01f, 0.0f, 1.0f, 0, 2000.0f * 100.0f, "hPa");
			return;
		case MessageType::AIR_VOC:
			editConvertFloat2Switch(menu, convertOptions, switchCommands[d], 1.0f, 0.0f, 0.1f, 1, 100.0f, "O");
			return;
		default:;
		}
	}
}

AwaitableCoroutine RoomControl::editFunctionConnection(ConnectionIterator &it, Connection &connection,
	Plug const &plug, bool add)
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
				String typeName = getTypeName(messageType);
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
	Subscriber::Barrier barrier;

	Publisher publishers[MAX_OUTPUT_COUNT];
	OffOn state;
	void const *states[] = {&state.state};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers, states);

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		Subscriber::Source source;
		uint8_t message;
		co_await barrier.wait(source, &message);

		// process message
		if (source.plugIndex == 0) {
			// on/off
			state.apply(message);
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
	OffOn state;
	void const *states[] = {&state.state};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, switchPlugs, subscribers, barrier, publishers, states);

	auto duration = (*this)->getConfig<Config>().duration;
	//SystemTime time;
	//uint32_t duration;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		Subscriber::Source source;
		uint8_t message;
		if (state == 0) {
			co_await barrier.wait(source, &message);
		} else {
			auto timeout = Timer::now() + (duration / 100) * 1s + ((duration % 100) * 1s) / 100;
			int s = co_await select(barrier.wait(source, &message), Timer::sleep(timeout));
			if (s == 2) {
				// switch off
				state.state = 0;
				source.plugIndex = -1;
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
		if (source.plugIndex == 0) {
			// on/off
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
	Message outPosition = {.value = {.f = 0.0f}};
	void const *states[] = {&state, &outPosition};

	// connect inputs and outputs
	int outputCount = roomControl.connectFunction(**this, blindPlugs, subscribers, barrier, publishers, states);

	// rocker timeout
	SystemDuration const holdTime = ((*this)->getConfig<Config>().holdTime * 1s) / 100;

	// position
	SystemDuration const maxPosition = ((*this)->getConfig<Config>().runTime * 1s) / 100;
	SystemDuration position = maxPosition / 2;
	SystemDuration targetPosition = 0s;
	SystemTime startTime;
	bool up = false;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		Subscriber::Source source;
		Message message;
		if (state == 0) {
			co_await barrier.wait(source, &message);
		} else {
			auto d = targetPosition - position;

			// set invalid plug index in case timeout occurs
			source.plugIndex = 255;

			// wait for event or timeout with a maximum to regularly report the current position
			int s = co_await select(barrier.wait(source, &message), Timer::sleep(min(up ? -d : d, 200ms)));

			// update position
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
		}

		// process message
		if (source.plugIndex <= 1) {
			// up/down or trigger
			if (message.command == 0) {
				// released: stop if timeout elapsed
				if (Timer::now() > startTime + holdTime)
					targetPosition = position;
			} else {
				// up or down pressed
				if (state == 0) {
					// start if currently stopped
					if (source.plugIndex == 1) {
						// trigger
						targetPosition = ((up || (targetPosition == 0s)) && targetPosition < maxPosition) ? maxPosition : 0s;
					} else if (message.command == 1) {
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
		} else if (source.plugIndex == 2) {
			// position
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
		}

		// check if target position already reached
		if (position == targetPosition) {
			state = 0;
		} else {
			state = (up = targetPosition < position) ? 1 : 2;
			startTime = Timer::now();
		}

		// update output position
		outPosition.value.f = position / maxPosition;
		Terminal::out << "pos " << flt(outPosition.value.f) << '\n';

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
	int outputCount = roomControl.connectFunction(**this, heatingControlPlugs, subscribers, barrier, publishers, states);

	OffOn on;
	OffOn night;
	OffOn summer;
	uint32_t windows = 0xffffffff;
	FloatValue setTemperature = {20.0f + 273.15f};
	float temperature = 20.0f + 273.15f;

	// no references to flash allowed beyond this point as flash may be reallocated
	while (true) {
		// wait for message
		Subscriber::Source source;
		Message message;

		co_await barrier.wait(source, &message);

		switch (source.plugIndex) {
			case 0:
				// on/off
				on.apply(message.command);
				break;
			case 1:
				// night
				night.apply(message.command);
				break;
			case 2:
				// summer
				summer.apply(message.command);
				break;
			case 3:
				// windows
				Terminal::out << "window " << dec(source.connectionIndex) << (message.command ? " close\n" : " open\n");
				if (message.command == 0)
					windows &= ~(1 << source.connectionIndex);
				else
					windows |= 1 << source.connectionIndex;
				break;
			case 4:
				// set temperature
				setTemperature.apply(message);
				Terminal::out << "set temperature " << flt(float(setTemperature) - 273.15f) + '\n';
				break;
			case 5:
				// measured temperature
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
		for (int i = 0; i < outputCount; ++i)
			publishers[i].publish();
	}
}

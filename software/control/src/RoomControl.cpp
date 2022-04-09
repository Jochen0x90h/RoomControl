//#include <iostream>
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


constexpr String weekdays[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr String weekdaysShort[7] = {"M", "T", "W", "T", "F", "S", "S"};

constexpr String switchStatesLong[] = {"off", "on", "toggle", String()};
constexpr String triggerStatesLong[] = {"inactive", "active"};
constexpr String upDownStatesLong[] = {"inactive", "up", "down", String()};

constexpr String buttonStatesLong[] = {"release", "press"};
constexpr String rockerStatesLong[] = {"release", "up", "down", String()};
constexpr String blindStatesLong[] = {"stopped", "up", "down", String()};

constexpr String buttonStates[] = {"#", "^", "!"};
constexpr String switchStates[] = {"0", "1"};
constexpr String rockerStates[] = {"#", "+", "-", "!"};

constexpr int8_t QOS = 1;


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
	// component has an element index
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
/*
static Configuration &initConfiguration(Storage::Array<Configuration> &configurations,
	PersistentStateManager &stateManager)
{
	// set default configuration if necessary
	if (configurations.isEmpty()) {
		ConfigurationFlash configuration;
		
		// name
		assign(configuration.name, "room");
		
		// generate a random 64 bit address
		configuration.ieeeLongAddress = rng::int64();
		
		// set default pan
		configuration.zbeePanId = 0x1a62;
		
		// generate random network key
		for (uint8_t &b : configuration.key) {
			b = rng::int8();
		}
		setKey(configuration.aesKey, configuration.key);
		
		
		// state offsets for interfaces
		configuration.busSecurityCounterOffset = stateManager.allocate<uint32_t>();
		configuration.zbeeSecurityCounterOffset = stateManager.allocate<uint32_t>();

		
		// write to flash
		configurations.write(0, new Configuration(configuration));
	}

	return configurations[0];
}
*/
RoomControl::RoomControl()
	//: storage(FLASH_PAGE_COUNT/2, FLASH_PAGE_COUNT/2, localDevices, busDevices, radioDevices, routes, timers)
	: storage(0, FLASH_PAGE_COUNT, configurations, busInterface.devices, radioInterface.gpDevices, radioInterface.zbDevices)
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
		configuration.ieeeLongAddress = Random::int64();
		
		// set default pan
		configuration.zbeePanId = 0x1a62;
		
		// generate random network key
		for (uint8_t &b : configuration.key) {
			b = Random::int8();
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
	
	// subscribe to local devices
	//subscribeInterface(this->localInterface, this->localDevices);

	// subscribe to mqtt broker (has to be repeated when connection to gateway is established because the broker does
	// not store the topic names)
	//subscribeAll();
/*
	// start device update
	auto now = timer::now();
	this->lastUpdateTime = now;
	this->nextReportTime = now;
	onTimeout();
*/
	
	// start coroutines
	idleDisplay();
	//doMenu();
	//doTimers();


//	testSwitch();
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
// UpLink
// ------

void RoomControl::onUpConnected() {
	connect("MyClient");
}


// MqttSnClient
// ------------

void RoomControl::onConnected() {
	std::cout << "onConnected" << std::endl;

	// subscribe devices at gateway
	subscribeAll();
}

void RoomControl::onDisconnected() {

}

void RoomControl::onSleep() {

}

void RoomControl::onWakeup() {

}

void RoomControl::onError(int error, mqttsn::MessageType messageType) {

}


// MqttSnBroker
// ------------

//todo: prevent recursion by enqueuing also messages to local client
void RoomControl::onPublished(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) {
	String message(length, data);
/ *
	std::cout << "onPublished " << topicId << " message " << message << " qos " << int(qos);
	if (retain)
		std::cout << " retain";
	std::cout << std::endl;
* /

	// topic list of house, room or device (depending on topicDepth)
	if (topicId == this->selectedTopicId && !message.isEmpty()) {
		int space = message.indexOf(' ', 0, message.length);
		if (this->topicDepth < 2 || !this->onlyCommands || message.substring(space + 1) == "c")
			this->topicSet.add(message.substring(0, space));
		return;
	}

	// global commands
	//ConfigurationState const &configurationState = *this->room[0].ram;

	// check for request to list rooms in the house
	if (topicId == this->houseTopicId && message.isEmpty()) {
		// publish room name
		publish(this->houseTopicId, getRoomName(), QOS);
		return;
	}

	// check for request to list devices in this room
	if (topicId == this->roomTopicId && message.isEmpty()) {
		// publish device names
		// todo: do async
		for (int i = 0; i < this->localDevices.size(); ++i) {
			auto e = this->localDevices[i];
			publish(this->roomTopicId, e.flash->getName(), QOS);
		}
		for (int i = 0; i < this->busDevices.size(); ++i) {
			auto e = this->busDevices[i];
			publish(this->roomTopicId, e.flash->getName(), QOS);
		}
		for (int i = 0; i < this->radioDevices.size(); ++i) {
			auto e = this->radioDevices[i];
			publish(this->roomTopicId, e.flash->getName(), QOS);
		}
		return;
	}

	// check for routing
	for (auto e : this->routes) {
		RouteState const &routeState = *e.ram;
		if (topicId == routeState.srcTopicId) {
			//std::cout << "route " << topicId << " -> " << routeState.dstTopicId << std::endl;
			publish(routeState.dstTopicId, data, length, qos);
		}
	}
	
	// update devices and set next timeout
	auto now = timer::now();
	auto nextTimeout = updateDevices(now, 0, 0, 0, nullptr, topicId, message);
	timer::start(this->timerIndex, nextTimeout);
}
*/

// LocalInterface
// --------------
/*
void RoomControl::onLocalReceived(uint8_t endpointId, uint8_t const *data, int length) {
	// update devices and set next timeout
	auto now = timer::now();
	auto nextTimeout = updateDevices(now, endpointId, 0, 0, data, 0, String());
	timer::start(this->timerIndex, nextTimeout);
}
*/

// BusInterface
// ------------
/*
void RoomControl::onBusReady() {
	subscribeInterface(this->busInterface, this->busDevices);
	
	this->busInterface.setReceivedHandler([this](uint8_t endpointId, uint8_t const *data, int length) {
		onBusReceived(endpointId, data, length);
	});
}

void RoomControl::onBusSent() {
	// check if more devices have to be subscribed
	while (!this->busInterface.isBusy() && this->busSubscribeIndex < this->busDevices.size()) {
		//std::cout << "subscribe bus device " << this->busSubscribeIndex << std::endl;
		subscribeInterface(this->busInterface, this->busDevices[this->busSubscribeIndex]);
		++this->busSubscribeIndex;
	}
}

void RoomControl::onBusReceived(uint8_t endpointId, uint8_t const *data, int length) {
	// update devices and set next timeout
	auto now = timer::now();
	auto nextTimeout = updateDevices(now, 0, endpointId, 0, data, 0, String());
	timer::start(this->timerIndex, nextTimeout);
}
 */

/*
// RadioInterface
// --------------

void RoomControl::onRadioReceived(uint8_t endpointId, uint8_t const *data, int length) {
	// update devices and set next timeout
	auto now = timer::now();
	auto nextTimeout = updateDevices(now, 0, 0, endpointId, data, 0, String());
	timer::start(this->timerIndex, nextTimeout);
}


	
// SystemTimer
// -----------
		
void RoomControl::onTimeout() {
	auto now = timer::now();
	//std::cout << "time: " << time.value << std::endl;
	SystemTime nextTimeout = updateDevices(now, 0, 0, 0, nullptr, 0, String());
	//std::cout << "next: " << nextTimeout.value << std::endl;
	timer::start(this->timerIndex, nextTimeout);
}
*/


// Menu
// ----
/*
Coroutine RoomControl::doMenu() {
	SSD1309 display;
	co_await display.init();
	co_await display.enable();

	updateMenu(0, false);
	while (true) {
//		co_await display.set(this->bitmap);

		int8_t delta;
		bool activated;
		switch (co_await select(poti::change(delta, activated), calendar::secondTick())) {
		case 1:
			if (updateMenu(delta, activated))
				updateMenu(0, false);
			break;
		case 2:
			updateMenu(0, false);
			break;
		}
	}
}

bool RoomControl::updateMenu(int delta, bool activated) {
	// if menu entry was activated, read menu state from stack
	if (this->stackHasChanged) {
		this->stackHasChanged = false;
		this->menuState = this->stack[this->stackIndex].menuState;
		this->selected = this->stack[this->stackIndex].selected;
		this->selectedY = this->stack[this->stackIndex].selectedY;
		this->yOffset = this->stack[this->stackIndex].yOffset;

		// set entryIndex to a large value so that the value of this->selected "survives" first call to menu()
		this->entryIndex = 0xffff;
	}

	// clear bitmap
	this->bitmap.clear();

	// toast
	if (!this->buffer.isEmpty() && timer::now() - this->toastTime < 3s) {
		String text = this->buffer;
		int y = 10;
		int len = tahoma_8pt.calcWidth(text, 1);
		this->bitmap.drawText((bitmap.WIDTH - len) >> 1, y, tahoma_8pt, text, 1);
		return false;
	}

	// draw menu
	switch (this->menuState) {
	case IDLE:
		{
			// get current clock time
			auto now = calendar::now();
						
			// display weekday and clock time
			StringBuffer<16> b = weekdays[now.getWeekday()] + "  "
				+ dec(now.getHours()) + ':'
				+ dec(now.getMinutes(), 2) + ':'
				+ dec(now.getSeconds(), 2);
			bitmap.drawText(20, 10, tahoma_8pt, b, 1);
/ *
			// update target temperature
			int targetTemperature = this->targetTemperature = clamp(this->targetTemperature + delta, 10 << 1, 30 << 1);
			this->temperature.setTargetValue(targetTemperature);

			// get current temperature
			int currentTemperature = this->temperature.getCurrentValue();

			this->buffer = decimal(currentTemperature >> 1), (currentTemperature & 1) ? ".5" : ".0" , " oC";
			bitmap.drawText(20, 30, tahoma_8pt, this->buffer, 1);
			this->buffer = decimal(targetTemperature >> 1), (targetTemperature & 1) ? ".5" : ".0" , " oC";
			bitmap.drawText(70, 30, tahoma_8pt, this->buffer, 1);
* /
			// enter menu if poti-switch was pressed
			if (activated) {
				this->stack[0] = {MAIN, 0, 0, 0};
				this->stackHasChanged = true;
			}
		}
		break;
	case MAIN:
		menu(delta, activated);

		if (entry("Local Devices"))
			push(LOCAL_DEVICES);
		if (entry("Bus Devices")) {
			this->busInterface.setCommissioning(true);
			push(BUS_DEVICES);
		}
		if (entry("Radio Devices")) {
			this->radioInterface.setCommissioning(true);
			push(RADIO_DEVICES);
		}
		if (entry("Routes"))
			push(ROUTES);
		if (entry("Timers"))
			push(TIMERS);
		if (entry("Exit")) {
			this->stack[0] = {IDLE, 0, 0, 0};
			this->stackHasChanged = true;
		}
		break;
/ *
	case LOCAL_DEVICES:
		menu(delta, activated);
		listDevices(this->localInterface, this->localDevices, EDIT_LOCAL_DEVICE, ADD_LOCAL_DEVICE);
		break;
	case EDIT_LOCAL_DEVICE:
	case ADD_LOCAL_DEVICE:
		menu(delta, activated);
		editDevice(this->localInterface, this->localDevices, this->menuState == ADD_LOCAL_DEVICE,
			EDIT_LOCAL_COMPONENT, ADD_LOCAL_COMPONENT);
		break;
	case EDIT_LOCAL_COMPONENT:
	case ADD_LOCAL_COMPONENT:
		menu(delta, activated);
		editComponent(delta, this->localInterface, this->menuState == ADD_LOCAL_COMPONENT);
		break;

	case BUS_DEVICES:
		menu(delta, activated);
		listDevices(this->busInterface, this->busDevices, EDIT_BUS_DEVICE, ADD_BUS_DEVICE);
		break;
	case EDIT_BUS_DEVICE:
	case ADD_BUS_DEVICE:
		menu(delta, activated);
		editDevice(this->busInterface, this->busDevices, this->menuState == ADD_BUS_DEVICE,
			EDIT_BUS_COMPONENT, ADD_BUS_COMPONENT);
		break;
	case EDIT_BUS_COMPONENT:
	case ADD_BUS_COMPONENT:
		menu(delta, activated);
		editComponent(delta, this->busInterface, this->menuState == ADD_BUS_COMPONENT);
		break;

	case RADIO_DEVICES:
		menu(delta, activated);
		listDevices(this->radioInterface, this->radioDevices, EDIT_RADIO_DEVICE, ADD_RADIO_DEVICE);
		break;
	case EDIT_RADIO_DEVICE:
	case ADD_RADIO_DEVICE:
		menu(delta, activated);
		editDevice(this->radioInterface, this->radioDevices, this->menuState == ADD_RADIO_DEVICE,
			EDIT_RADIO_COMPONENT, ADD_RADIO_COMPONENT);
		break;
	case EDIT_RADIO_COMPONENT:
	case ADD_RADIO_COMPONENT:
		menu(delta, activated);
		editComponent(delta, this->radioInterface, this->menuState == ADD_RADIO_COMPONENT);
		break;
* /
	case ROUTES:
		menu(delta, activated);
		{
			// list routes
			for (auto e : this->routes) {
				Route const &route = *e.flash;
				String srcTopic = route.getSrcTopic();
				String dstTopic = route.getDstTopic();
				if (entry(srcTopic)) {
					// edit route
					clone(this->temp.route, this->tempState.route, route, *e.ram);
					push(EDIT_ROUTE);
				}
				//label(dstTopic);
				//line();
			}
			
			// add route
			if (entry("Add Route")) {
				// check if list of routes is full
				if (this->routes.size() < MAX_ROUTE_COUNT) {
					// check if memory for a new route is available
					Route &route = this->temp.route;
					route.srcTopicLength = TopicBuffer::MAX_TOPIC_LENGTH * 3;
					route.dstTopicLength = 0;
					if (this->routes.hasSpace(&route)) {
						// clea
						route.srcTopicLength = 0;
						this->tempState.route.srcTopicId = 0;
						this->tempState.route.dstTopicId = 0;
						push(ADD_ROUTE);
					} else {
						// error: out of memory
						// todo
					}
				} else {
					// error: maximum number of routes reached
					// todo: inform user
				}
			}
			
			// exit
			if (entry("Exit"))
				pop();
		}
		break;
	case EDIT_ROUTE:
	case ADD_ROUTE:
		menu(delta, activated);
		{
			// get route
			Route &route = this->temp.route;
			RouteState &routeState = this->tempState.route;

			String srcTopic = route.getSrcTopic();
			String dstTopic = route.getDstTopic();
			if (entry(this->tempState.route.srcTopicId != 0 ? srcTopic : "Select Source Topic")) {
				enterTopicSelector(srcTopic, false, 0);
			}
			if (entry(this->tempState.route.dstTopicId != 0 ? dstTopic : "Select Destination Topic")) {
				enterTopicSelector(dstTopic, true, 1);
			}
			
			// save, delete, cancel
			if (routeState.srcTopicId != 0 && routeState.dstTopicId != 0) {
				if (entry("Save Route")) {
					int index = getThisIndex();
					
					// unsubscribe from old source topic
					if (index < this->routes.size()) {
						auto e = this->routes[index];
						TopicBuffer topic = e.flash->getSrcTopic();
						unsubscribeTopic(e.ram->srcTopicId, topic.state());
					}
					this->routes.write(index, &route, &routeState);
					pop();
				}
			}
			if (this->menuState == EDIT_ROUTE) {
				if (entry("Delete Route")) {
					int index = getThisIndex();

					// unsubscribe from new source topic
					TopicBuffer topic = route.getSrcTopic();
					unsubscribeTopic(routeState.srcTopicId, topic.state());

					// unsubscribe from old source topic
					auto e = this->routes[index];
					topic = e.flash->getSrcTopic();
					unsubscribeTopic(e.ram->srcTopicId, topic.state());
					
					this->routes.erase(index);

					pop();
				}
			}
			if (entry("Cancel")) {
				// unsubscribe from new source topic
				TopicBuffer topic = route.getSrcTopic();
				unsubscribeTopic(routeState.srcTopicId, topic.state());
				pop();
			}
		}
		break;
	case TIMERS:
		menu(delta, activated);
		{
			// list timers
			for (auto e : this->timers) {
				Timer const &timer = *e.flash;
				
				int minutes = timer.time.getMinutes();
				int hours = timer.time.getHours();
				StringBuffer<16> b = dec(hours) + ':' + dec(minutes, 2) + "    ";
				if (entryWeekdays(b, timer.time.getWeekdays())) {
					// edit timer
					clone(this->temp.timer, this->tempState.timer, timer, *e.ram);
					push(EDIT_TIMER);
				}
			}
			
			// add timer
			if (entry("Add Timer")) {
				// check if list of timers is full
				if (this->timers.size() < MAX_TIMER_COUNT) {
					// check if memory for a new timer is available
					Timer &timer = this->temp.timer;
					timer.time = {};
					timer.commandCount = 1;
					Command &command = *reinterpret_cast<Command *>(timer.commands);
					command.topicLength = TopicBuffer::MAX_TOPIC_LENGTH * 3;
					command.valueType = Command::ValueType::BUTTON;
					if (this->timers.hasSpace(&timer)) {
						// clear
						timer.commandCount = 0;
						push(ADD_TIMER);
					} else {
						// error: out of memory
						// todo
					}
				} else {
					// error: maximum number of timers reached
					// todo: inform user
				}
			}

			// exit
			if (entry("Exit"))
				pop();
		}
		break;
	case EDIT_TIMER:
	case ADD_TIMER:
		menu(delta, activated);
		{
			int edit;
			uint8_t editBegin[6];
			uint8_t editEnd[6];
			
			// get timer
			Timer &timer = this->temp.timer;
			TimerState &timerState = this->tempState.timer;
			
			// get time
			int hours = timer.time.getHours();
			int minutes = timer.time.getMinutes();
			int weekdays = timer.time.getWeekdays();

			// edit time
			edit = getEdit(2);
			if (edit > 0) {
				if (edit == 1)
					hours = (hours + delta + 24) % 24;
				else
					minutes = (minutes + delta + 60) % 60;
			}

			// time menu entry
			StringBuffer<16> b = "Time: ";
			editBegin[1] = this->buffer.length();
			b += dec(hours);
			editEnd[1] = this->buffer.length();
			b += ':';
			editBegin[2] = this->buffer.length();
			b += dec(minutes, 2);
			editEnd[2] = this->buffer.length();
			entry(b, edit > 0, editBegin[edit], editEnd[edit]);

			// edit weekdays
			edit = getEdit(7);
			if (edit > 0 && delta != 0)
				weekdays ^= 1 << (edit - 1);

			// weekdays menu entry
			b = "Days: ";
			entryWeekdays(b, weekdays, edit > 0, edit - 1);

			// write back
			timer.time = ClockTime(weekdays, hours, minutes);

			// commands
			line();
			{
				CommandEditor editor(timer, timerState);
				int commandCount = timer.commandCount;
				for (int commandIndex = 0; commandIndex < commandCount; ++commandIndex) {
					Command &command = editor.getCommand();
					auto valueType = command.valueType;
					
					
					// menu entry for topic
					{
						String topic = editor.getTopic();
						if (entry(!topic.isEmpty() ? topic : "Select Topic")) {
							enterTopicSelector(topic, true, commandIndex);
						}
					}
					
					// edit value type
					edit = getEdit(1 + valueInfos[int(valueType)].elementCount);
					if (edit == 1 && delta != 0) {
						// edit value type
						int typeCount = array::size(valueInfos);
						valueType = Command::ValueType((int(valueType) + typeCount + delta) % typeCount);
						editor.setValueType(valueType);
					}

					// menu entry for type and value
					editBegin[1] = 0;
					b = valueInfos[int(valueType)].name;
					editEnd[1] = b.length();
					b += ": ";
					uint8_t *value = editor.getValue();
					switch (valueType) {
					case Command::ValueType::BUTTON:
						// button state is release or press, one data byte
						b += buttonStatesLong[1];
						break;
					case Command::ValueType::SWITCH:
						// binary state is off or on, one data byte
						if (edit == 2 && delta != 0)
							value[0] ^= 1;

						editBegin[2] = b.length();
						b += switchStatesLong[value[0] & 1];
						editEnd[2] = b.length();
						break;
					case Command::ValueType::ROCKER:
						// rocker state is release, up or down, one data byte
						if (edit == 2)
							value[0] = (value[0] + delta) & 1;
						
						editBegin[2] = b.length();
						b += rockerStatesLong[1 + (value[0] & 1)];
						editEnd[2] = b.length();
						break;
					case Command::ValueType::VALUE8:
						// value 0-255, one data byte
						if (edit == 2)
							value[0] += delta;
						
						editBegin[2] = b.length();
						b += dec(value[0]);
						editEnd[2] = b.length();
						break;
					case Command::ValueType::PERCENTAGE:
						// percentage 0-100, one data byte
						if (edit == 2)
							value[0] = (value[0] + delta + 100) % 100;
						
						editBegin[2] = b.length();
						b += dec(value[0]) + '%';
						editEnd[2] = b.length();
						break;
					case Command::ValueType::CELSIUS:
						// room temperature 8° - 33,5°
						if (edit == 2)
							value[0] += delta;
						
						editBegin[2] = b.length();
						b += dec(8 + value[0] / 10) + '.' + dec(value[0] % 10);
						editEnd[2] = b.length();
						b += "oC";
						break;
						
					case Command::ValueType::COLOR_RGB:
						if (edit == 2)
							value[0] += delta;
						if (edit == 3)
							value[1] += delta;
						if (edit == 4)
							value[2] += delta;

						editBegin[2] = b.length();
						b += dec(value[0]);
						editEnd[2] = b.length();
						b += ' ';
						
						editBegin[3] = b.length();
						b += dec(value[1]);
						editEnd[3] = b.length();
						b += ' ';

						editBegin[4] = b.length();
						b += dec(value[2]);
						editEnd[4] = b.length();
						
						break;
					}
					entry(b, edit > 0, editBegin[edit], editEnd[edit]);
										
					// menu entry for testing the command
					if (entry("Test")) {
						publishCommand(editor.getState().topicId, command.valueType, value);
					}
					
					// menu entry for deleting the command
					if (entry("Delete Command")) {
						// erase command
						editor.erase();
						--timer.commandCount;

						// select next menu entry (-4 + 1)
						this->selected -= 3;
					} else {
						// next command
						editor.next();
					}
					line();
				}
				
				// add command
				if (timer.commandCount < Timer::MAX_COMMAND_COUNT) {
					if (entry("Add Command")) {
						editor.insert();
						++timer.commandCount;
					}
					line();
				}
			}
			
			// save, delete, cancel
			if (entry("Save Timer")) {
				int index = getThisIndex();
				this->timers.write(index, &this->temp.timer);
				pop();
			}
			if (this->menuState == EDIT_TIMER) {
				if (entry("Delete Timer")) {
					int index = getThisIndex();
					this->timers.erase(index);
					pop();
				}
			}
			if (entry("Cancel")) {
				pop();
			}
		}
		break;
	case SELECT_TOPIC:
		menu(delta, activated);
		
		// menu entry for "parent directory" (select device or room)
		if (this->topicDepth > 0) {
			constexpr String components[] = {"Room", "Device"};
			StringBuffer<16> b = "Select " + components[this->topicDepth - 1];
			if (entry(b)) {
				// unsubscribe from current topic
				unsubscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration());

				// go to "parent directory"
				this->selectedTopic.removeLast();
				--this->topicDepth;
				
				// subscribe to new topic
				subscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration(), QOS);

				// query topic list by publishing empty message, gets filled in onPublished
				this->topicSet.clear();
				publish(this->selectedTopicId, String(), 1);
			}
		}

		// menu entry for each room/device/attribute
		String selected;
		for (String topic : this->topicSet) {
			if (entry(topic))
				selected = topic;
		}
		
		// cancel menu entry
		if (entry("Cancel")) {
			// unsubscribe from selected topic
			unsubscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration());
			
			// clear
			this->selectedTopic.clear();
			this->selectedTopicId = 0;
			this->topicSet.clear();
			
			// exit menu
			pop();
		}
		
		if (!selected.isEmpty()) {
			// unsubscribe from selected topic
			unsubscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration());
			
			// append new element (room, device or attribute) to selected topic
			this->selectedTopic /= selected;
			++this->topicDepth;

			// clear topic set
			this->topicSet.clear();
			
			if (this->topicDepth < 3) {
				// enter next level
				subscribeTopic(this->selectedTopicId, this->selectedTopic.enumeration(), QOS);
				
				// query topic list by publishing empty message, gets filled in onPublished
				publish(this->selectedTopicId, String(), QOS);
				
				// select first entry in menu
				this->selected = 1;
			} else {
				// exit menu and write topic back to command
				pop();
				
				String topic = this->selectedTopic.string();
				auto menuState = this->stack[this->stackIndex].menuState;
				switch (menuState) {
				case EDIT_ROUTE:
				case ADD_ROUTE:
					{
						Route &route = this->temp.route;
						RouteState &state = this->tempState.route;
						
						if (this->tempIndex == 0) {
							// unsubscribe from old source topic
							TopicBuffer oldTopic = route.getSrcTopic();
							if (!oldTopic.isEmpty())
								unsubscribeTopic(state.srcTopicId, oldTopic.state());

							// set new source topic
							route.setSrcTopic(topic);
							
							// subscribe to new source topic
							subscribeTopic(state.srcTopicId, this->selectedTopic.state(), QOS);
						} else {
							// unregsiter old destination topic
							if (!route.getDstTopic().isEmpty())
								unregisterTopic(state.dstTopicId);
							
							// set new destination topic
							route.setDstTopic(topic);
							
							// register new destination topic
							registerTopic(state.dstTopicId, this->selectedTopic.command());
						}
					}
					break;
				case EDIT_TIMER:
				case ADD_TIMER:
					{
						Timer &timer = this->temp.timer;
						TimerState &timerState = this->tempState.timer;
						
						// set topic to command
						CommandEditor editor(timer, timerState, this->tempIndex);						
						auto &command = editor.getCommand();
						auto &state = editor.getState();

						// unregister old topic for command
						if (command.topicLength > 0)
							unregisterTopic(state.topicId);

						// set topic
						editor.setTopic(topic);
						
						// register new topic for command
						registerTopic(state.topicId, this->selectedTopic.command());
					}
					break;
				default:
					break;
				}

				// clear
				this->selectedTopic.clear();
				this->selectedTopicId = 0;
			}
		}
		break;
	}
	this->buffer.clear();
	
	
	// determine if an immediate redraw is needed
	bool redraw = this->stackHasChanged;
	
	const int lineHeight = tahoma_8pt.height + 4;

	// adjust yOffset so that selected entry is visible
	int upper = this->selectedY;
	int lower = upper + lineHeight;
	if (upper < this->yOffset) {
		this->yOffset = upper;
		redraw = true;
	}
	if (lower > this->yOffset + bitmap.HEIGHT) {
		this->yOffset = lower - bitmap.HEIGHT;
		redraw = true;
	}
	return redraw;
}



// Menu System
// -----------

void RoomControl::menu(int delta, bool activated) {
	// update selected according to delta motion of poti when not in edit mode
	if (this->edit == 0) {
		this->selected += delta;
		if (this->selected < 0) {
			this->selected = 0;

			// also clear yOffset in case the menu has a non-selectable header
			this->yOffset = 0;
		} else if (this->selected >= this->entryIndex) {
			this->selected = this->entryIndex - 1;
		}
	}
	
	// set activated state for selecting the current menu entry
	this->activated = activated;

/ *
	const int lineHeight = tahoma_8pt.height + 4;

	// adjust yOffset so that selected entry is visible
	int upper = this->selectedY;
	int lower = upper + lineHeight;
	if (upper < this->yOffset)
		this->yOffset = upper;
	if (lower > this->yOffset + bitmap.HEIGHT)
		this->yOffset = lower - bitmap.HEIGHT;
* /
	this->entryIndex = 0;
	this->entryY = 0;
}
	
void RoomControl::label(String s) {
	int x = 10;
	int y = this->entryY + 2 - this->yOffset;
	this->bitmap.drawText(x, y, tahoma_8pt, s, 1);
	this->entryY += tahoma_8pt.height + 4;
}

void RoomControl::line() {
	int x = 10;
	int y = this->entryY + 2 - this->yOffset;
	this->bitmap.fillRectangle(x, y, 108, 1);
	this->entryY += 1 + 4;
}

bool RoomControl::entry(String s, bool underline, int begin, int end) {
	int x = 10;
	int y = this->entryY + 2 - this->yOffset;//this->entryIndex * lineHeight + 2 - this->yOffset;
	this->bitmap.drawText(x, y, tahoma_8pt, s);
	
	bool selected = this->entryIndex == this->selected;
	if (selected) {
		this->bitmap.drawText(0, y, tahoma_8pt, ">", 0);
		this->selectedY = this->entryY;
	}
	
	if (underline) {
		int start = tahoma_8pt.calcWidth(s.substring(0, begin));
		int width = tahoma_8pt.calcWidth(s.substring(begin, end)) - 1;
		this->bitmap.hLine(x + start, y + tahoma_8pt.height, width);
	}

	++this->entryIndex;
	this->entryY += tahoma_8pt.height + 4;

	return selected && this->activated;
}

bool RoomControl::entryWeekdays(String s, int weekdays, bool underline, int index) {
	int x = 10;
	int y = this->entryY + 2 - this->yOffset;

	// text (e.g. time)
	int x2 = this->bitmap.drawText(x, y, tahoma_8pt, s, 1) + 1;
	int start = x;
	int width = x2 - x;

	// week days
	for (int i = 0; i < 7; ++i) {
		int x3 = this->bitmap.drawText(x2 + 1, y, tahoma_8pt, weekdaysShort[i], 1);
		if (weekdays & 1)
			this->bitmap.fillRectangle(x2, y, x3 - x2, tahoma_8pt.height - 1, DrawMode::FORE_FLIP);
		if (i == index) {
			start = x2;
			width = x3 - x2;
		}
		x2 = x3 + 4;
		weekdays >>= 1;
	}

	bool selected = this->entryIndex == this->selected;
	if (selected) {
		this->bitmap.drawText(0, y, tahoma_8pt, ">", 1);
		this->selectedY = this->entryY;
	}
	
	if (underline) {
		this->bitmap.hLine(start, y + tahoma_8pt.height, width);
	}

	++this->entryIndex;
	this->entryY += tahoma_8pt.height + 4;

	return selected && this->activated;
}

int RoomControl::getEdit(int editCount) {
	if (this->selected == this->entryIndex) {
		// cycle edit mode if activated
		if (this->activated) {
			if (this->edit < editCount) {
				++this->edit;
			} else {
				this->edit = 0;
				//this->editFinished = true;
			}
		}
		return this->edit;
	}
	return 0;
}

void RoomControl::push(MenuState menuState) {
	this->stack[this->stackIndex] = {this->menuState, this->selected, this->selectedY, this->yOffset};
	++this->stackIndex;
	assert(this->stackIndex < array::size(this->stack));
	this->stack[this->stackIndex] = {menuState, 0, 0, 0};
	this->stackHasChanged = true;
}

void RoomControl::pop() {
	--this->stackIndex;
	this->stackHasChanged = true;
}
*/

// Configuration
// -------------
/*
int RoomControl::Configuration::getFlashSize() const {
	return sizeof(Configuration);
}

int RoomControl::Configuration::getRamSize() const {
	return 0;
}
*/
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
		auto deviceId = interface.getDeviceId(i);
		Terminal::out << (hex(deviceId) + '\n');
		Array<Interface::EndpointType const> endpoints = interface.getEndpoints(deviceId);
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
		auto deviceId = interface.getDeviceId(i);
		Array<Interface::EndpointType const> endpoints = interface.getEndpoints(deviceId);
		for (int endpointIndex = 0; endpointIndex < endpoints.count(); ++endpointIndex) {
			auto endpointType = endpoints[endpointIndex];
			if (!haveIn && (endpointType == Interface::EndpointType::ON_OFF_IN || endpointType == Interface::EndpointType::UP_DOWN_IN)) {
				haveIn = true;
				interface.addSubscriber(deviceId, endpointIndex, subscriber);
			}
			if (!haveOut && endpointType == Interface::EndpointType::ON_OFF_OUT) {
				haveOut = true;
				interface.addPublisher(deviceId, endpointIndex, publisher);
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

/*
Coroutine simpleSwitch(RoomControl &control, ) {
	Barrier<Interface2::Parameters> barrier;
	
	//uint16_t inEndpointIds[16];
	
	// iterate over devices to subscribe
	int inEndpointCount = 0;
	for (int i = 0; i < inEndpointCount; ++i) {
		uint8_t endpointIndex;
		//control.subscribeEndpoint(interfaceIndex, deviceId, endpointIndex, UnitType::SWITCH, 0x80 | i, &barrier);
		control.subscribeIn(interfaceIndex, deviceId, endpointIndex, UnitType::SWITCH, i, &barrier);
	}
	int outEndpointCount = 0;
	for (int i = 0; i < outEndpointCount; ++i) {
		//control.subscribeEndpoint(interfaceIndex, deviceId, endpointIndex, UnitType::SWITCH, i, &barrier);
	}

	uint8_t out = control.registerOut(interfaceIndex, deviceId, endpointIndex, UnitType::SWITCH, i);

	uint8_t state = 0;
	
	while (true) {
		uint8_t index;
		int length;
		uint8_t data[1];

		co_await waitlist.wait(index, length, data);
		
		if ((index ^ 0x80) < inEndpointCount)
			uint8_t oldState = state;
			if (*data <= 1) {
				// set state
				state = *data;
			} else if (*data == 2) {
				// toggle
				state ^= 1;
			}

			if (state != oldState) {
				// notify device endpoints
				control.sendEndpoint(outEndpointIds[i], &state, 1);
			}

		}
	}
}

Coroutine autoOffSwitch(RoomControl &control, SystemDuration delay) {
	Barrier<Interface2::Parameters> barrier;

	// subscribe as SWITCH


	uint8_t state = 0;
	
	while (true) {
		uint8_t index;
		int length;
		uint8_t data[1];

		int selected = 1;
		if (state == 0)
			co_await waitlist.wait(index, length, data);
		else
			selected = co_await select(waitlist.wait(index, length, data), timer::delay(delay));

		uint8_t oldState = state;
		if (selected = 1) {
			if ((index ^ 0x80) < inEndpointCount)
				if (*data <= 1) {
					// set state
					state = *data;
				} else if (*data == 2) {
					// toggle
					state ^= 1;
				}
			}
		} else {
			// timeout
			state = 0;
		}

		if (state != oldState) {
			// notify device endpoints
			control.sendEndpoint(outEndpointIds[i], &state, 1);
		}
	}
}

Coroutine delayedSwitch(RoomControl &control, SystemDuration delay) {
	Barrier<Interface2::Parameters> barrier;

	// subscribe as BUTTON


	bool pressed = false;
	uint8_t state = 0;
	
	while (true) {
		uint8_t index;
		int length;
		uint8_t data[1];

		int selected = 1;
		if (!pressed)
			co_await waitlist.wait(index, length, data);
		else
			selected = co_await select(waitlist.wait(index, length, data), timer::delay(delay));

		uint8_t oldState = state;
		if (selected = 1) {
			if ((index ^ 0x80) < inEndpointCount)
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
		uint8_t index;
		int length;
		uint8_t data[1];

		SystemTime time = timer::now();

		co_await waitlist.wait(index, length, data);

		uint8_t oldState = state;
		if ((index ^ 0x80) < inEndpointCount)
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
		uint8_t index;
		int length;
		uint8_t data[4];

		int selected = 1;
		if (state == 0) {
			// stopped: wait for command
			co_await waitlist.wait(index, length, data);
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
			selected = co_await select(waitlist.wait(index, length, data), timer::wait(time + d));
			
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
			if ((index ^ 0x80) < inEndpointCount)
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
		uint8_t index;
		int length;
		uint8_t data[4];
	
		
		int selected = co_await select(waitlist.wait(index, length, data), timer::wait(600s));

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
		StringBuffer<16> b = weekdays[now.getWeekday()] + "  "
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
			co_await devicesMenu(this->localInterface);
		if (menu.entry("Bus Devices"))
			co_await devicesMenu(this->busInterface);
		if (menu.entry("Radio Devices"))
			co_await devicesMenu(this->radioInterface);
		if (menu.entry("Functions"))
			;
		if (menu.entry("Routes"))
			;//push(ROUTES);
		if (menu.entry("Timers"))
			;//push(TIMERS);
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}
/*
AwaitableCoroutine RoomControl::devicesMenu(Interface &interface) {
	interface.setCommissioning(true);
	Menu menu(this->bitmap);
	while (true) {
		int count = interface.getDeviceCount();
		for (int i = 0; i < count; ++i) {
			auto deviceId = interface.getDeviceId(i);
			StringBuffer<16> b = hex(deviceId);
			menu.entry(b);
		}
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show(this->display);
	}
	interface.setCommissioning(false);
}
*/
AwaitableCoroutine RoomControl::devicesMenu(Interface &interface) {
	interface.setCommissioning(true);
	Menu menu(this->swapChain);
	while (true) {
		int count = interface.getDeviceCount();
		for (int i = 0; i < count; ++i) {
			auto deviceId = interface.getDeviceId(i);
			StringBuffer<16> b;
			if (deviceId <= 0xffffffff)
				b = hex(uint32_t(deviceId));
			else
				b = hex(deviceId);
			if (menu.entry(b))
				co_await deviceMenu(interface, deviceId);
		}
		if (menu.entry("Exit"))
			break;
		
		// show menu and wait for new event until timeout so that we can show newly commissioned devices
		co_await select(menu.show(), Timer::sleep(250ms));
	}
	interface.setCommissioning(false);
}


AwaitableCoroutine RoomControl::deviceMenu(Interface &interface, DeviceId deviceId) {
	Menu menu(this->swapChain);
	while (true) {
		if (menu.entry("Endpoints"))
			co_await endpointsMenu(interface, deviceId);
		if (menu.entry("Message Logger"))
			co_await messageLogger(interface, deviceId);
		if (menu.entry("Message Generator"))
			co_await messageGenerator(interface, deviceId);
		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show();
	}
}

AwaitableCoroutine RoomControl::endpointsMenu(Interface &interface, DeviceId deviceId) {
	Menu menu(this->swapChain);
	while (true) {
		Array<Interface::EndpointType const> endpoints = interface.getEndpoints(deviceId);
		for (int i = 0; i < endpoints.count(); ++i) {
			auto endpointType = endpoints[i];
			StringBuffer<24> b = dec(i) + ": ";
			switch (endpointType & Interface::EndpointType::TYPE_MASK) {
			case Interface::EndpointType::ON_OFF:
				b += "On/Off";
				break;
			case Interface::EndpointType::TRIGGER:
				b += "Trigger";
				break;
			case Interface::EndpointType::UP_DOWN:
				b += "Up/Down";
				break;
				
			case Interface::EndpointType::LEVEL:
				b += "Level";
				break;

			case Interface::EndpointType::TEMPERATURE:
				b += "Temperature";
				break;
			case Interface::EndpointType::AIR_PRESSURE:
				b += "Air Pressure";
				break;
			case Interface::EndpointType::AIR_HUMIDITY:
				b += "Air Humidity";
				break;
			case Interface::EndpointType::AIR_VOC:
				b += "Air VOC";
				break;
			case Interface::EndpointType::ILLUMINANCE:
				b += "Illuminance";
				break;
				
			case Interface::EndpointType::VOLTAGE:
				b += "Voltage";
				break;
			case Interface::EndpointType::CURRENT:
				b += "Current";
				break;
			case Interface::EndpointType::BATTERY_LEVEL:
				b += "Battery Level";
				break;
			case Interface::EndpointType::ENERGY_COUNTER:
				b += "Energy Counter";
				break;
			case Interface::EndpointType::POWER:
				b += "Power";
				break;

			default:
				b += "<Unknown>";
				break;
			}
			if ((endpointType & Interface::EndpointType::DIRECTION_MASK) == Interface::EndpointType::IN)
				b += " In";
			else
				b += " Out";
			menu.entry(b);
		}
		if (menu.entry("Exit"))
			break;

		// show menu
		//co_await menu.show();
		// show menu and wait for new event until timeout so that we can show endpoints of recommissioned device
		co_await select(menu.show(), Timer::sleep(250ms));
	}
}

static MessageType getDefaultMessageType(Interface::EndpointType endpointType) {
	switch (endpointType & Interface::EndpointType::TYPE_MASK) {
	case Interface::EndpointType::ON_OFF:
		return MessageType::ON_OFF;
	case Interface::EndpointType::TRIGGER:
		return MessageType::TRIGGER;
	case Interface::EndpointType::UP_DOWN:
		return MessageType::UP_DOWN;
	case Interface::EndpointType::LEVEL:
		return MessageType::MOVE_TO_LEVEL;
	case Interface::EndpointType::TEMPERATURE:
		return MessageType::CELSIUS;
	case Interface::EndpointType::AIR_PRESSURE:
		return MessageType::HECTOPASCAL;
		default:
			break;
	}
	return MessageType::UNKNOWN;
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
		return {.airPressure = 1000.0f};
	default:
		break;
	}
	return {};
}

AwaitableCoroutine RoomControl::messageLogger(Interface &interface, DeviceId deviceId) {
	Subscriber::Barrier barrier;

	Subscriber subscribers[32];

	// subscribe to all endpoints
	for (uint8_t endpointIndex = 0; endpointIndex < /*min(endpoints.count(), */array::count(subscribers)/*)*/; ++endpointIndex) {
		auto &subscriber = subscribers[endpointIndex];
		subscriber.subscriptionIndex = endpointIndex;
		//auto endpointType = endpoints[endpointIndex];
		//subscriber.messageType = getDefaultMessageType(endpointType);
		subscriber.barrier = &barrier;
		interface.addSubscriber(deviceId, endpointIndex, subscriber);
	}

	// event queue
	struct Event {
		uint8_t endpointIndex;
		::Message message;
	};
	Queue<Event, 16> queue;
	
	// add an empty event at the back of the queue to receive the first message
	queue.addBack();

	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		Array<Interface::EndpointType const> endpoints = interface.getEndpoints(deviceId);
		for (uint8_t endpointIndex = 0; endpointIndex < min(endpoints.count(), array::count(subscribers)); ++endpointIndex) {
			auto &subscriber = subscribers[endpointIndex];
			auto endpointType = endpoints[endpointIndex];
			subscriber.messageType = getDefaultMessageType(endpointType);
		}

		// display events
		for (int i = queue.count() - 2; i >= 0; --i) {
			Event &event = queue[i];
			if (event.endpointIndex < endpoints.count()) {
				StringBuffer<24> b = dec(event.endpointIndex) + ": ";

				auto endpointType = endpoints[event.endpointIndex];
				switch (endpointType & Interface::EndpointType::TYPE_MASK) {
					case Interface::EndpointType::ON_OFF:
						b << switchStatesLong[event.message.onOff & 3];
						break;
					case Interface::EndpointType::TRIGGER:
						b << triggerStatesLong[event.message.trigger & 1];
						break;
					case Interface::EndpointType::UP_DOWN:
						b << upDownStatesLong[event.message.upDown & 3];
						break;

					case Interface::EndpointType::TEMPERATURE:
						b << flt(event.message.temperature, 1) << "oC";
						break;
					case Interface::EndpointType::AIR_PRESSURE:
						b << flt(event.message.temperature, 1) << "hPa";
						break;
					default:
						break;
				}
				menu.entry(b);
			}
		}
		
		if (menu.entry("Exit"))
			break;

		// get the empty event at the back of the queue
		Event &event = queue.getBack();

		// show menu or receive event (event gets filled in)
		int selected = co_await select(menu.show(), barrier.wait(event.endpointIndex, &event.message), Timer::sleep(250ms));
		if (selected == 2) {
			// received an event: add new empty event at the back of the queue
			queue.addBack();
		}
	}
}

AwaitableCoroutine RoomControl::messageGenerator(Interface &interface, DeviceId deviceId) {
	//! skip "in" endpoints -> or forward to subscriptions
	Message message;

	uint8_t endpointIndex = 0;
	Publisher publisher;
	publisher.message = &message;
	
	// menu loop
	Menu menu(this->swapChain);
	while (true) {
		Array<Interface::EndpointType const> endpoints = interface.getEndpoints(deviceId);
		if (endpoints.count() > 0) {
			endpointIndex = clamp(endpointIndex, 0, endpoints.count() - 1);
			auto endpointType = endpoints[endpointIndex];

			// select endpoint and value to publish
			int edit = menu.getEdit(endpointType == Interface::EndpointType::LEVEL ? 3 : 2);
			bool editIndex = edit == 1;
			bool editMessage = edit == 2;
			bool editMessage2 = edit == 3;
			int delta = menu.getDelta();

			if (editIndex) {
				// edit endpoint index
				endpointIndex = clamp(endpointIndex + delta, 0, endpoints.count() - 1);
				endpointType = endpoints[endpointIndex];
			}
			auto newMessageType = getDefaultMessageType(endpointType);
			if (newMessageType != publisher.messageType)
				message = getDefaultMessage(endpointType);
			publisher.messageType = newMessageType;

			// edit and dislpay message values
			auto stream = menu.stream();
			stream << underline(dec(endpointIndex), editIndex) << ": ";
			switch (endpointType & Interface::EndpointType::TYPE_MASK) {
				case Interface::EndpointType::ON_OFF:
					if (editMessage)
						message.onOff = clamp(message.onOff + delta, 0, 2);
					stream << underline(switchStatesLong[message.onOff & 3], editMessage);
					break;
				case Interface::EndpointType::TRIGGER:
					if (editMessage)
						message.trigger = clamp(message.trigger + delta, 0, 1);
					stream << underline(triggerStatesLong[message.trigger & 1], editMessage);
					break;
				case Interface::EndpointType::UP_DOWN:
					if (editMessage)
						message.upDown = clamp(message.upDown + delta, 0, 2);
					stream << underline(upDownStatesLong[message.upDown & 3], editMessage);
					break;
				case Interface::EndpointType::LEVEL:
					if (editMessage)
						message.moveToLevel.level = clamp(float(message.moveToLevel.level) + delta * 0.05f, 0.0f, 1.0f);
					if (editMessage2)
						message.moveToLevel.move = clamp(float(message.moveToLevel.move) + delta * 0.5f, 0.0f, 100.0f);
					stream << underline(flt(message.moveToLevel.level, 2), editMessage) << ' ';
					stream << underline(flt(message.moveToLevel.move, 1), editMessage2) << 's';
					break;
				case Interface::EndpointType::TEMPERATURE:
					if (editMessage)
						message.temperature = std::round(message.temperature * 2.0f + delta) * 0.5f;
					stream << underline(flt(message.temperature, -1), editMessage) << "oC";
					break;
				case Interface::EndpointType::AIR_PRESSURE:
					if (editMessage)
						message.airPressure = message.airPressure + delta;
					stream << underline(flt(message.airPressure, 0), editMessage) << "hPa";
					break;
			}
			menu.entry();

			if (menu.entry("Send")) {
				interface.addPublisher(deviceId, endpointIndex, publisher);
				publisher.publish();
			}
		}

		if (menu.entry("Exit"))
			break;
			
		// show menu
		co_await select(menu.show(), Timer::sleep(250ms));
	}
}

/*
AwaitableCoroutine RoomControl::functionsMenu(Interface2 &interface, Storage2::Array<Device2, DeviceState2> &devices) {
	interface.setCommissioning(true);
	Menu menu(this->bitmap);
	while (true) {

		// list devices
		for (auto e : devices) {
			auto &device = *e.flash;
			
			StringBuffer<24> b = device.getName();
			if (menu.entry(b)) {
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
				if (menu.entry(b)) {
				}
			}
		}

		if (menu.entry("Exit"))
			break;

		// show menu
		co_await menu.show(this->display);
	}
	interface.setCommissioning(false);
}
*/

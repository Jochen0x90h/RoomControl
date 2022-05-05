#pragma once

#include "LocalInterface.hpp"
#include "BusInterface.hpp"
#include "RadioInterface.hpp"
#include "AlarmInterface.hpp"
#include "SwapChain.hpp"
#include "Menu.hpp"
#include <MqttSnBroker.hpp> // include at first because of strange compiler error
#include <State.hpp>
#include <Storage.hpp>
#include <ClockTime.hpp>
#include <Coroutine.hpp>
#include <StringBuffer.hpp>
#include <StringSet.hpp>
#include <TopicBuffer.hpp>


/**
 * Main room control class that inherits platform dependent (hardware or emulator) components
 */
class RoomControl {
public:
	// maximum number of mqtt routes
	static constexpr int MAX_ROUTE_COUNT = 32;
	
	// maximum number of timers
	static constexpr int MAX_TIMER_COUNT = 32;


	RoomControl();

	~RoomControl();

	void applyConfiguration();
	

// UpLink
// ------
	
	//void onUpConnected() override;


// MqttSnClient
// ------------
/*
	void onConnected() override;
	
	void onDisconnected() override;
	
	void onSleep() override;
	
	void onWakeup() override;
	
	void onError(int error, mqttsn::MessageType messageType) override;
	

// MqttSnBroker
// ------------

	void onPublished(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) override;
	

// Timer
// -----
		
	void onTimeout();

	// timer index
	static int const timerIndex = TIMER_ROOM_CONTROL;


// Menu
// ----

	// menu state
	enum MenuState {
		IDLE,
		MAIN,
		
		// local devices
		LOCAL_DEVICES,
		EDIT_LOCAL_DEVICE,
		ADD_LOCAL_DEVICE,
		EDIT_LOCAL_COMPONENT,
		ADD_LOCAL_COMPONENT,

		// bus devices
		BUS_DEVICES,
		EDIT_BUS_DEVICE,
		ADD_BUS_DEVICE,
		EDIT_BUS_COMPONENT,
		ADD_BUS_COMPONENT,
		
		// radio devices
		RADIO_DEVICES,
		EDIT_RADIO_DEVICE,
		ADD_RADIO_DEVICE,
		EDIT_RADIO_COMPONENT,
		ADD_RADIO_COMPONENT,

		// connections
		ROUTES,
		EDIT_ROUTE,
		ADD_ROUTE,

		// timers
		TIMERS,
		EDIT_TIMER,
		ADD_TIMER,

		// rules
		
		
		// helper
		SELECT_TOPIC,
	};
	
	Coroutine doMenu();

	bool updateMenu(int delta, bool activated);

	// menu state
	MenuState menuState = IDLE;



// Menu System
// -----------

	void menu(int delta, bool activated);

	void label(String s);

	void line();
	
	bool entry(String s, bool underline = false, int begin = 0, int end = 0);

	bool entryWeekdays(String s, int weekdays, bool underline = false, int index = 0);
	
	bool isSelectedEntry() {
		return this->selected == this->entryIndex;
	}

	/ **
	 * Get edit state. Returns 0 if not in edit mode or not the entry being edited, otherwise returns the 1-based index
	 * of the field being edited
	 * /
	int getEdit(int editCount = 1);

	//bool isAnyEditFinished() {return this->editFinished;}

	void push(MenuState menuState);
	
	void pop();
	
	int getThisIndex() {
		return this->stack[this->stackIndex - 1].selected;
	}

	void setThisIndex(int index) {
		this->stack[this->stackIndex - 1].selected = index;
	}


	// index of selected menu entry
	int selected = 0;
	
	// y coordinate of selected menu entry
	int selectedY = 0;
		
	// starting y coodinate of display
	int yOffset = 0;
	
	// menu stack
	struct StackEntry {MenuState menuState; int selected; int selectedY; int yOffset;};
	int stackIndex = 0;
	StackEntry stack[6];
	bool stackHasChanged = false;

	// true if selected menu entry was activated
	bool activated = false;

	// index of current menu entry
	int entryIndex = 0xffff;
	
	// y coordinate of current menu entry
	int entryY;

	// edit value of selected element
	int edit = 0;
	//bool editFinished = false;

	// toast data
	SystemTime toastTime;
	
	// temporary string buffer
	StringBuffer<32> buffer;


// Storage
// -------

	Storage storage;



//

	// subscribe everything to mqtt topics
	void subscribeAll();


// Devices
// -------
	
	struct Device {
		static constexpr int MAX_COMPONENT_COUNT = 32;
		static constexpr int BUFFER_SIZE = MAX_COMPONENT_COUNT * 16;
		
		// device component
		struct Component {
			static constexpr int CLASS_FLAGS = 0;
			
			// component type. Note: Keep elementInfos array in sync
			enum Type : uint8_t {
				// button with pressed and released states
				BUTTON,

				// button that sends a "stop" instead of "release" message when held for a configured duration
				HOLD_BUTTON,

				// button that goes to pressed state only if it is held down for a configured duration
				DELAY_BUTTON,

				// switch with on and off states
				SWITCH,

				// rocker switch with up, down and released states
				ROCKER,

				// rocker that sends a "stop" instead of "release" message when held for a configured duration
				HOLD_ROCKER,

				// relay with on and off states
				RELAY,

				// ralay that automatically switches off after some duration
				TIME_RELAY,
					   
				// blind
				BLIND,

				// venetian blind
				//VENETIAN_BLIND,

				// celsius temperature sensor
				CELSIUS,
				
				// fahrenheit temperature sensor
				FAHRENHEIT,
				
			};

			// get name of this component (which is used as topic component)
			Plus<String, Dec<uint8_t>> getName() const;

			// set first valid component type and clear data
			void init(EndpointType endpointType);

			// rotate component type to the next valid type
			void rotateType(EndpointType endpointType, int delta);

			// check if given class is compatible to the actual class of the component
			bool checkClass(uint8_t classFlags) const;

			template <typename T>
			bool is() const {
				// check if class is compatible
				return T::CLASS_FLAGS == 0 || checkClass(T::CLASS_FLAGS);
			}

			template <typename T>
			T const &cast() const {
				// check if class is compatible
				assert(is<T>());

				// cast and assert at compile time that T inherits Device::Component
				T const *ptr = reinterpret_cast<T const *>(this);
				Component const *base = ptr;
				return *ptr;
			}

			template <typename T>
			T &cast() {
				// check if class is compatible
				assert(is<T>());

				// cast and assert at compile time that T inherits Device::Component
				T *ptr = reinterpret_cast<T *>(this);
				Component *base = ptr;
				return *ptr;
			}


			// component type
			Type type;

			// index of device endpoint this component is connected to
			uint8_t endpointIndex;
			
			// index of this component
			uint8_t nameIndex;
			
			// index of element, used e.g. by arrays of binary or ternary sensors
			uint8_t elementIndex;
		};

		struct TimeComponent : public Component {
			static constexpr int CLASS_FLAGS = Component::CLASS_FLAGS | 1;

			SystemDuration duration;
		};

		using Button = Component;
		using Switch = Component;
		using Rocker = Component;
		using HoldButton = TimeComponent;
		using DelayButton = TimeComponent;
		using HoldRocker = TimeComponent;
		using Relay = Component;
		using TimeRelay = TimeComponent;
		using Blind = TimeComponent;
		using TemperatureSensor = Component;


		Device &operator =(const Device &) = delete;

		/ **
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 * /
		int getFlashSize() const;

		/ **
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 * /
		int getRamSize() const;
		
		/ **
		 * Returns the name of the device
		 * @return device name
		 * /
		String getName() const {return this->name;}

		/ **
		 * Set the name of the device
		 * @param name device name
		 * /
		void setName(String name);

		/ **
		 * get a unique name index for the given type
		 * /
		uint8_t getNameIndex(Component::Type type, int skipComponentIndex) const;
		

		// device id
		DeviceId deviceId;

		// device name
		char name[15];

		// number of components of this device
		uint8_t componentCount;
				
		// buffer for components
		// todo: check for buffer overflow (instead of MAX_COMPONENT_COUNT)
		uint32_t buffer[BUFFER_SIZE / 4];
	};
	
	struct DeviceState {
		static constexpr int BUFFER_SIZE = 16 * Device::MAX_COMPONENT_COUNT;

		// base struct for component state
		struct Component {
			static constexpr int CLASS_FLAGS = 0;

			// check if given class is compatible to the actual class of the component
			bool checkClass(Device::Component::Type type, uint8_t classFlags) const;

			template <typename T>
			bool is(Device::Component const &component) const {
				// check if class is compatible
				return T::CLASS_FLAGS == 0 || checkClass(component.type, T::CLASS_FLAGS);
			}
			
			template <typename T>
			T &cast() {
				// cast and assert at compile time that T inherits DeviceState::Component
				T *ptr = reinterpret_cast<T *>(this);
				Component *base = ptr;
				return *ptr;
			}

			// general purpose flags
			uint8_t flags;
			
			// endpoint id of button/rocker
			uint8_t endpointId;

			// topic for publishing the status
			uint16_t statusTopicId;
		};
		
		using Button = Component;
		using Switch = Component;
		using Rocker = Component;

		struct TimeComponent : public Component {
			static constexpr int CLASS_FLAGS = Component::CLASS_FLAGS | 1;

			SystemTime timeout;
		};

		using HoldButton = TimeComponent;
		using DelayButton = TimeComponent;
		using HoldRocker = TimeComponent;
		
		// element that can receive commands
		struct CommandComponent : public Component {
			static constexpr int CLASS_FLAGS = Component::CLASS_FLAGS | 2;

			// topic receiving commands
			uint16_t commandTopicId;
		};

		struct Relay : public CommandComponent {
			// flag indicating the relay state
			static constexpr int RELAY = 1;

			// flag indicating if a controlling button or rocker is currently pressed
			static constexpr int PRESSED = 0x80;
		};

		struct TimeRelay : public Relay {
			static constexpr int CLASS_FLAGS = Relay::CLASS_FLAGS | 4;

			// timeout after which the relay goes to off state
			SystemTime timeout;
		};

		struct Blind : public TimeRelay {
			static constexpr int CLASS_FLAGS = TimeRelay::CLASS_FLAGS | 8;

			// flag indicating the relay state
			static constexpr int RELAY_UP = 1;
			static constexpr int RELAY_DOWN = 2;
			static constexpr int RELAYS = 3;
			
			// flag indicating the last direcion
			static constexpr int DIRECTIION_UP = 4;

			// current position measured as time
			SystemDuration duration;
		};

		struct TemperatureSensor : public Component {
		
			// last temperature measurement
			uint16_t temperature;
		};


		DeviceState &operator =(const DeviceState &) = delete;

		// topic id for list of attributes in device (topic: enum/<room>/<device>)
		uint16_t deviceTopicId;

		// buffer for component states
		uint32_t buffer[BUFFER_SIZE / 4];
	};
	
	// subscribe one device to mqtt topics
	void subscribeDevice(Storage::Element<Device, DeviceState> e);

	// update all devices, handle time, endpoint events and topic events
	SystemTime updateDevices(SystemTime time,
		uint8_t localEndpointId, uint8_t busEndpointId, uint8_t radioEndpointId, uint8_t const *data,
		uint16_t topicId, String message);
*/
	// update devices for one interface
	//SystemTime updateDevices(SystemTime time, SystemDuration duration, bool reportChanging,
	//	Interface &interface, Storage::Array<Device, DeviceState> &devices, uint8_t endpointId, uint8_t const *data,
	//	uint16_t topicId, String message, SystemTime nextTimeout);
	//SystemTime updateDevice(SystemTime time, SystemDuration duration, bool reportChanging,
	//	Interface &interface, Device const &device, DeviceState &deviceState, uint8_t endpointId, uint8_t const *data,
	//	uint16_t topicId, String message, SystemTime nextTimeout);

	// check compatibility between device endpoint and device element
	//static bool isCompatible(EndpointType endpointType, Device::Component::Type type);
/*
	// list devices menu
	void listDevices(Interface &interface, Storage::Array<Device, DeviceState> &devices,
		MenuState editDevice, MenuState addDevice);

	// edit device menu
	void editDevice(Interface &interface, Storage::Array<Device, DeviceState> &devices, bool add,
		MenuState editComponent, MenuState addComponent);

	// edit device component menu
	void editComponent(int delta, Interface &interface, bool add);

	// clone device and its state (subscribe command topics)
	void clone(Interface &interface, Device &dstDevice, DeviceState &dstDeviceState,
		Device const &srcDevice, DeviceState const &srcDeviceState);

	// destroy device (unsubscribe command topics)
	void destroy(Interface &interface, Device const &device, DeviceState &deviceState);
*/

	//Storage::Array<Device, DeviceState> localDevices;
	//Storage::Array<Device, DeviceState> busDevices;
	//Storage::Array<Device, DeviceState> radioDevices;

	// next time for reporting changing values
	//SystemTime nextReportTime;
	
	// time of last update of changing values
	//SystemTime lastUpdateTime;


// ComponentIterator
// -----------------
/*
	// iterator for device elements
	struct ComponentIterator {
		ComponentIterator(Device const &device, DeviceState &state)
			: componentCount(device.componentCount)
			, component(device.buffer)
			, state(state.buffer)
		{}
		
		Device::Component const &getComponent() const {
			return *reinterpret_cast<Device::Component const *>(this->component);
		}

		DeviceState::Component &getState() {
			return *reinterpret_cast<DeviceState::Component *>(this->state);
		}

		void next();

		bool atEnd() {return this->componentIndex == this->componentCount;}

		int componentCount;
		int componentIndex = 0;
		uint32_t const *component;
		uint32_t *state;
	};


// ComponentEditor
// ---------------

	// editor for device components
	struct ComponentEditor {
		ComponentEditor(Device &device, DeviceState &state, int index);
		
		Device::Component &getComponent() {
			return *reinterpret_cast<Device::Component *>(this->component);
		}

		DeviceState::Component &getState() {
			return *reinterpret_cast<DeviceState::Component *>(this->state);
		}

		Device::Component &insert(Device::Component::Type type);

		void changeType(Device::Component::Type type);

		void erase();


		uint32_t *component;
		uint32_t *componentsEnd;
		uint32_t *state;
		uint32_t *statesEnd;
	};
*/

// Interface
// ---------

	// subscribe a device to an interface
	//void subscribeInterface(Interface &interface, Storage::Element<Device, DeviceState> e);
	
	//void subscribeInterface(Interface &interface, Storage::Array<Device, DeviceState> &devices);


// LocalInterface
// --------------

	// called when a local device has sent data
	//void onLocalReceived(uint8_t endpointId, uint8_t const *data, int length);

	// interface to locally connected devices
	//LocalInterface localInterface;


// BusInterface
// ------------

	// called when bus interface has finished enumerating the devices
	//void onBusReady();

	// called after enumeration of bus devices or when send queue became empty
	//void onBusSent();

	// called when a device connected to the bus has sent data
	//void onBusReceived(uint8_t endpointId, uint8_t const *data, int length);

/*
	// interface to devices connected via bus
	//BusInterface busInterface;
	
	// startup: Index of device to subscribe when onBusSent() gets called
	int busSubscribeIndex = 0;


// RadioInterface
// --------------

	// called when a device connected via radio has sent data
	void onRadioReceived(uint8_t endpointId, uint8_t const *data, int length);


	// interface to devices connected via radio
	//RadioInterface radioInterface;


// Routes
// ------

	struct Route {
		Route &operator =(const Route &) = delete;

		/ **
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 * /
		int getFlashSize() const;

		/ **
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 * /
		int getRamSize() const;
	
		String getSrcTopic() const {return {this->srcTopicLength, buffer};}
		String getDstTopic() const {return {this->dstTopicLength, buffer + this->srcTopicLength};}

		void setSrcTopic(String topic);
		void setDstTopic(String topic);
	
		uint8_t srcTopicLength;
		uint8_t dstTopicLength;
		uint8_t buffer[TopicBuffer::MAX_TOPIC_LENGTH * 2];
	};
	
	struct RouteState {
		RouteState &operator =(const RouteState &) = delete;

		// source topic id (gets subscribed to receive messages)
		uint16_t srcTopicId;
		
		// destination topic id (only registered to publish messages)
		uint16_t dstTopicId;
	};

	// register/subscribe route to mqtt topics
	void subscribeRoute(int index);

	// copy route and its state
	void clone(Route &dstRoute, RouteState &dstRouteState, Route const &srcRoute, RouteState const &srcRouteState);

	Storage::Array<Route, RouteState> routes;


// Command
// -------

	/ **
	 * Command to be executed e.g. on timer event.
	 * Contains an mqtt topic where to publish a value and the value itself, both are stored in an external data buffer
	 * /
	struct Command {
		enum class ValueType : uint8_t {
			// button: pressed or release
			BUTTON,
			
			// switch: on or off
			SWITCH,

			// rocker: up, down or release
			ROCKER,			
						
			// 0-100%
			PERCENTAGE,
			
			// temperature in Celsius
			CELSIUS,
			
			// temperature in Fahrenheit
			FAHRENHEIT,

			// color rgb
			COLOR_RGB,

			// 8 bit value, 0-255
			VALUE8,
		};
		
		// rotate value type to the next type
		void rotateValueType(int delta);

		/ **
		 * Get the size of the value in bytes
		 * /
		//int getValueSize() const;
		
		/ **
		 * Get the number of values (e.g. 3 for rgb color)
		 * /
		//int getValueCount() const;
		
		//int getFlashSize() const {return this->topicLength + getValueSize();}
		
		//void changeType(int delta, uint8_t *data, uint8_t *end);
				
		//void setTopic(String topic, uint8_t *data, uint8_t *end);
		
		// length of topic where value gets published
		uint8_t topicLength;

		// type of value that is published on the topic
		ValueType valueType;
	};

	struct CommandState {
		// id of topic to publish to
		uint16_t topicId;
	};

	// publish a command
	void publishCommand(uint16_t topicId, Command::ValueType valueType, uint8_t const *value);


// Timers
// ------

	struct Timer {
		// weekday flags
		static constexpr uint8_t MONDAY = 0x01;
		static constexpr uint8_t TUESDAY = 0x02;
		static constexpr uint8_t WEDNESDAY = 0x04;
		static constexpr uint8_t THURSDAY = 0x08;
		static constexpr uint8_t FRIDAY = 0x10;
		static constexpr uint8_t SATURDAY = 0x20;
		static constexpr uint8_t SUNDAY = 0x40;

		static constexpr int MAX_COMMAND_COUNT = 16;
		static constexpr int MAX_VALUE_SIZE = 4;
		static constexpr int BUFFER_SIZE = (sizeof(Command) + MAX_VALUE_SIZE + TopicBuffer::MAX_TOPIC_LENGTH)
			* MAX_COMMAND_COUNT;

		Timer &operator =(const Timer &) = delete;

		/ **
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 * /
		int getFlashSize() const;

		/ **
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 * /
		int getRamSize() const;

		/ **
		 * Begin of buffer behind the list of commands containing the value and topic
		 * /
		//uint8_t *begin() {return this->u.buffer + this->commandCount * sizeof(Command);}
		//uint8_t const *begin() const {return this->u.buffer + this->commandCount * sizeof(Command);}
		
		/ **
		 * End of buffer
		 * /
		//uint8_t *end() {return this->u.buffer + BUFFER_SIZE;}

		// timer time and weekday flags
		ClockTime time;
	
	
		uint8_t commandCount;
		/ *union {
			// list of commands, overlaps with buffer
			Command commands[MAX_COMMAND_COUNT];
			
			// buffer for commands, topics and values
			uint8_t buffer[BUFFER_SIZE];
		} u;
		* /
		uint8_t commands[BUFFER_SIZE];
	};
	
	struct TimerState {
		TimerState &operator =(const TimerState &) = delete;

		//uint16_t topicIds[Timer::MAX_COMMAND_COUNT];
		
		CommandState commands[Timer::MAX_COMMAND_COUNT];
	};
	
	// checks every second if a timer has elapsed
	Coroutine doTimers();
	
	// register timer mqtt topics
	void subscribeTimer(int index);

	void clone(Timer &dstTimer, TimerState &dstTimerState, Timer const &srcTimer, TimerState const &srcTimerState);

	Storage::Array<Timer, TimerState> timers;
	using TimerElement = Storage::Element<Timer, TimerState>;


// CommandIterator
// ---------------

	// iterator for commands
	struct CommandIterator {
		CommandIterator(Timer const &timer, TimerState &state)
			: commandCount(timer.commandCount)
			, command(timer.commands)
			, state(state.commands)
		{}
		
		Command const &getCommand() const {
			return *reinterpret_cast<Command const *>(this->command);
		}

		CommandState &getState() {
			return *this->state;
		}

		String getTopic() {
			Command const &command = *reinterpret_cast<Command const *>(this->command);
			return {command.topicLength, this->command + sizeof(Command)};
		}

		uint8_t const *getValue() {
			Command const &command = *reinterpret_cast<Command const *>(this->command);
			return this->command + sizeof(Command) + command.topicLength;
		}

		void next();

		bool atEnd() {return this->commandIndex == this->commandCount;}

		int commandCount;
		int commandIndex = 0;
		uint8_t const *command;
		CommandState *state;
	};


// CommandEditor
// -------------

	// editor for commands
	struct CommandEditor {
		CommandEditor(Timer &timer, TimerState &state)
			: command(timer.commands), commandsEnd(array::end(timer.commands))
			, state(state.commands), statesEnd(array::end(state.commands))
		{}

		CommandEditor(Timer &timer, TimerState &state, int index);

		Command &getCommand() {
			return *reinterpret_cast<Command *>(this->command);
		}

		CommandState &getState() {
			return *this->state;
		}

		String getTopic() {
			Command &command = *reinterpret_cast<Command *>(this->command);
			return {command.topicLength, this->command + sizeof(Command)};
		}

		void setTopic(String topic);

		void setValueType(Command::ValueType valueType);
		
		uint8_t *getValue() {
			Command &command = *reinterpret_cast<Command *>(this->command);
			return this->command + sizeof(Command) + command.topicLength;
		}

		void insert();

		void erase();

		void next();


		uint8_t *command;
		uint8_t *commandsEnd;
		CommandState *state;
		CommandState *statesEnd;
	};


// Temporary variables for editing via menu
// ----------------------------------------

	// first level
	union {
		Device device;
		Route route;
		Timer timer;
	} temp;
	union {
		DeviceState device;
		RouteState route;
		TimerState timer;
	} tempState;
	
	// the device that the temporary variable replaces
	void const *tempFor = nullptr;
	
	// second level
	int tempIndex;
	union {
		Device::Component component;
		Device::TimeComponent timeComponent;
	} temp2;
	

// Topic Selector
// --------------

	void enterTopicSelector(String topic, bool onlyCommands, int index);

	// selected topic including space for prefix
	TopicBuffer selectedTopic;
	uint16_t selectedTopicId = 0;
	uint16_t topicDepth;
	StringSet<64, 64 * 16> topicSet;
	bool onlyCommands;
*/





//
//


// Storage
// -------

	Storage storage;
	PersistentStateManager stateManager;
	

// Configuration
// -------------

	String getRoomName() {return this->configurations[0]->name;}

	// global configuration for this room control, see Configuration.hpp (array contains only one entry)
	Storage::Array<Configuration> configurations;

	// network key
	AesKey networkKey;

	// topic id for list of rooms in house (topic: enum)
	uint16_t houseTopicId;

	// topic id for list of devices in room (topic: enum/<room>)
	uint16_t roomTopicId;


// Interfaces
// ----------

	LocalInterface localInterface;
	BusInterface busInterface;
	RadioInterface radioInterface;
	AlarmInterface alarmInterface;


// Functions
// ---------

	static constexpr int MAX_INPUT_COUNT = 16;
	static constexpr int MAX_OUTPUT_COUNT = 8;

	// plug (input or output) of a function
	struct Plug {
		// platform and options
		enum class Interface : uint8_t {
			// local devices
			LOCAL = 0,

			// bus devices (LIN physical layer)
			BUS = 1,

			// radio (ieee 802.15.4)
			RADIO = 2,

			// alarms (pseudo-devices)
			ALARMS = 3,

			// mqtt-sn with string topic
			MQTT = 4,

			COUNT = 5,
		};


		// index to identify the input/output
		uint8_t index = 0;

		// platform and invert
		uint8_t flags = 0;

		// device id or length of MQTT topic string
		uint8_t deviceId = 0;

		uint8_t endpointIndex;

		void setInterface(Interface interface) {this->flags = (this->flags & 0xf0) | uint8_t(interface);}
		void setInvert(bool invert) {this->flags = (this->flags & 0x7f) | uint8_t(invert) << 7;}
		Interface getInterface() const {return Interface(this->flags & 0x0f);}
		bool isInvert() const {return (this->flags & 0x80) != 0;}
		bool isMqtt() const {return getInterface() == Interface::MQTT;}
	};

	// plug info
	struct PlugInfo {
		bool input;

		// message type that is expected/sent by the input/output
		MessageType type;

		// name if onput/output
		String name;
	};

	struct PlugConstIterator {
		uint32_t const *buffer;

		PlugConstIterator &operator ++() {
			auto &plug = *reinterpret_cast<Plug const *>(this->buffer);
			int size = sizeof(Plug);
			if (plug.isMqtt())
				size += plug.deviceId - (sizeof(Plug) - offsetof(Plug, endpointIndex));
			this->buffer += (size + 3) / 4;
			return *this;
		}
		Plug const &getPlug() const {
			return *reinterpret_cast<Plug const *>(this->buffer);
		}
		String getTopic() const {
			Plug const &plug = *reinterpret_cast<Plug const *>(this->buffer);
			return {plug.deviceId, reinterpret_cast<char const *>(this->buffer) + sizeof(Plug)};
		}
	};

	struct FunctionFlash;

	struct PlugIterator {
		uint32_t *buffer;
		FunctionFlash &flash;

		PlugIterator &operator ++() {
			auto &plug = *reinterpret_cast<Plug *>(this->buffer);
			int size = sizeof(Plug);
			if (plug.isMqtt())
				size += plug.deviceId - (sizeof(Plug) - offsetof(Plug, endpointIndex));
			this->buffer += (size + 3) / 4;
			return *this;
		}
		Plug &getPlug() {
			return *reinterpret_cast<Plug *>(this->buffer);
		}
		String getTopic() const {
			Plug &plug = *reinterpret_cast<Plug *>(this->buffer);
			return {plug.deviceId, reinterpret_cast<char *>(this->buffer) + offsetof(Plug, endpointIndex)};
		}
		void setTopic(String topic);

		void insert();

		void erase();
	};

	class Function;

	struct FunctionFlash  {
		static constexpr int BUFFER_SIZE = 1024;
		
		enum class Type : uint8_t {
			UNKNOWN,

			// simple on/off switch
			SIMPLE_SWITCH,

			// switch that turns off after a configured time
			TIMEOUT_SWITCH,

			//DELAYED_SWITCH,
		
			// simple up/down blind
			SIMPLE_BLIND,

			// blind with known position using timing
			TRACKED_BLIND,
			
			TWO_LEVEL_CONTROLLER,

			TYPE_COUNT
		};
		
		// function type
		Type type = Type::SIMPLE_SWITCH;
	
		// name is at offset 0 in buffer
		uint8_t nameLength = 0;
		
		// offset of function configuration in 32 bit units (only some functions need a configuration)
		uint8_t configOffset = 0;
		
		// number of connections to the device plugs (inputs and outputs)
		uint8_t connectionCount = 0;

		// offset of plugs in 32 bit units
		uint8_t connectionsOffset = 0;

		// buffer size in 32 bit units
		uint16_t bufferSize = 0;


		FunctionFlash() = default;
		FunctionFlash(FunctionFlash const &flash);

		/**
		 * Set type and reallocate and clear configuration
		 * @param type function type
		 */
		void setType(Type type);

		/**
		 * Returns the name of the function (e.g. "Ceiling Light")
		 * @return device name
		 */
		String getName() const {return String(this->nameLength, this->buffer);}
		void setName(String name);

		/**
		 * Get function specific configuration
		 */
		template <typename T>
		T const &getConfig() const {return *reinterpret_cast<T const *>(this->buffer + this->configOffset);}
		template <typename T>
		T &getConfig() {return *reinterpret_cast<T *>(this->buffer + this->configOffset);}

		/**
		 * Get list of inputs
		 */
		PlugConstIterator getPlugs() const {return {this->buffer + this->connectionsOffset};}
		PlugIterator getPlugs() {return {this->buffer + this->connectionsOffset, *this};}

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const {return getOffset(FunctionFlash, buffer[this->bufferSize]);}

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		Function *allocate() const;
	
	
		// buffer for config, inputs and outputs
		uint32_t buffer[BUFFER_SIZE / 4];
	};

	class Function : public Storage::Element<FunctionFlash> {
	public:
		explicit Function(FunctionFlash const &flash) : Storage::Element<FunctionFlash>(flash) {}
		virtual ~Function();

		// start the function coroutine
		[[nodiscard]] virtual Coroutine start(RoomControl &roomControl) = 0;

		// each function has its own coroutine
		Coroutine coroutine;
	};


	// simple switch
	class SimpleSwitch : public Function {
	public:
		explicit SimpleSwitch(FunctionFlash const &flash) : Function(flash) {}
		~SimpleSwitch() override;

		Coroutine start(RoomControl &roomControl) override;
	};


	// timeout switch
	struct TimeoutSwitchConfig {
		// timeout duration in 1/100 s
		uint32_t duration;
	};

	class TimeoutSwitch : public Function {
	public:
		explicit TimeoutSwitch(FunctionFlash const &flash) : Function(flash) {}
		~TimeoutSwitch() override;

		Coroutine start(RoomControl &roomControl) override;
	};


	Interface &getInterface(Plug const &plug);

	Coroutine testSwitch();

	int connectFunction(RoomControl::FunctionFlash const &flash, Array<PlugInfo const> plugInfos,
		Array<Subscriber, MAX_INPUT_COUNT> subscribers, Subscriber::Barrier &barrier,
		Array<Publisher, MAX_OUTPUT_COUNT> publishers, Array<void const *> states);

	// list of functions
	Storage::Array<Function> functions;


// Menu Helpers
// ------------

	void printPlug(Menu::Stream &stream, Plug const &plug);

	float stepTemperature(float value, int delta);
	Flt displayTemperature(float kelvin);
	String getTemperatureUnit();

	MessageType editMessage(Menu::Stream &stream, Interface::EndpointType endpointType,
		MessageType messageType, Message &message, bool editMessage1, bool editMessage2, int delta);


// Menu
// ----

	// display
	SwapChain swapChain;

	Coroutine idleDisplay();
	[[nodiscard]] AwaitableCoroutine mainMenu();
	[[nodiscard]] AwaitableCoroutine devicesMenu(Interface &interface);
	[[nodiscard]] AwaitableCoroutine deviceMenu(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine alarmsMenu(AlarmInterface &interface);
	[[nodiscard]] AwaitableCoroutine alarmMenu(AlarmInterface &interface, int index, AlarmInterface::AlarmFlash &flash);
	[[nodiscard]] AwaitableCoroutine alarmTimeMenu(AlarmTime &time);
	[[nodiscard]] AwaitableCoroutine alarmActionsMenu(AlarmInterface::AlarmFlash &flash);
	[[nodiscard]] AwaitableCoroutine endpointsMenu(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine messageLogger(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine messageGenerator(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine functionsMenu();
	[[nodiscard]] AwaitableCoroutine functionMenu(int index, FunctionFlash &flash);
	[[nodiscard]] AwaitableCoroutine editFunctionPlug(PlugIterator &it, Plug &plug, PlugInfo const &info, bool add);
	[[nodiscard]] AwaitableCoroutine selectFunctionDevice(Plug &plug, PlugInfo const &info);
	[[nodiscard]] AwaitableCoroutine selectFunctionEndpoint(Interface::Device &device, Plug &plug, PlugInfo const &info);
};

#pragma once

#include "LocalInterface.hpp"
#include "BusInterface.hpp"
#include <Bitmap.hpp>
#include <StringBuffer.hpp>
#include <StringSet.hpp>
#include <TopicBuffer.hpp>
#include <MqttSnBroker.hpp>
#include <SSD1309.hpp>
#include <Storage.hpp>
#include <calendar.hpp>
#include <poti.hpp>


/**
 * Main room control class that inherits platform dependent (hardware or emulator) components
 */
class RoomControl : public MqttSnBroker {
public:
	static constexpr int TIMER_INDEX = 2;

	// maximum number of mqtt routes
	static constexpr int MAX_ROUTE_COUNT = 32;
	
	// maximum number of timers
	static constexpr int MAX_TIMER_COUNT = 32;


	RoomControl();

	~RoomControl() override;


// UpLink
// ------
	
	void onUpConnected() override;


// MqttSnClient
// ------------

	void onConnected() override;
	
	void onDisconnected() override;
	
	void onSleep() override;
	
	void onWakeup() override;
	
	void onError(int error, mqttsn::MessageType messageType) override;
	

// MqttSnBroker
// ------------

	void onPublished(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) override;
	

// SystemTimer
// -----------
		
	void onTimeout();


// Display
// -------

	void onDisplayReady();

	// display bitmap
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> bitmap;
	
	// display controller
	SSD1309 display;
	

// Poti
// ----

	void onPotiChanged(int delta, bool activated);


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

		// devices connected by bus
		BUS_DEVICES,
		EDIT_BUS_DEVICE,
		ADD_BUS_DEVICE,
		EDIT_BUS_COMPONENT,
		ADD_BUS_COMPONENT,
		
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
	
	
	void updateMenu(int delta, bool activated);

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

	/**
	 * Get edit state. Returns 0 if not in edit mode or not the entry being edited, otherwise returns the 1-based index
	 * of the field being edited
	 */
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


// Room
// ----

	struct Room {
		// name of this room
		char name[16];

		/**
		 * Returns the size in bytes needed to store the control configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the contol state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
	};

	struct RoomState {
		// topic id for list of rooms in house (topic: enum)
		uint16_t houseTopicId;

		// topic id for list of devices in room (topic: enum/<room>)
		uint16_t roomTopicId;
	};

	String getRoomName() {return this->room[0].flash->name;}

	// subscribe everything to mqtt topics
	void subscribeAll();


	Storage::Array<Room, RoomState> room;
	using RoomElement = Storage::Element<Room, RoomState>;




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

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
		
		/**
		 * Returns the name of the device
		 * @return device name
		 */
		String getName() const {return this->name;}

		/**
		 * Set the name of the device
		 * @param name device name
		 */
		void setName(String name);

		/**
		 * get a unique name index for the given type
		 */
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
		uint8_t localEndpointId, uint8_t busEndpointId, uint8_t const *data,
		uint16_t topicId, String message);

	// update devices for one interface
	SystemTime updateDevices(SystemTime time, SystemDuration duration, bool reportChanging,
		Interface &interface, Storage::Array<Device, DeviceState> &devices, uint8_t endpointId, uint8_t const *data,
		uint16_t topicId, String message, SystemTime nextTimeout);

	// check compatibility between device endpoint and device element
	static bool isCompatible(EndpointType endpointType, Device::Component::Type type);

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


	Storage::Array<Device, DeviceState> localDevices;
	Storage::Array<Device, DeviceState> busDevices;
	//using DeviceElement = Storage::Element<Device, DeviceState>;

	// next time for reporting changing values
	SystemTime nextReportTime;
	
	// time of last update of changing values
	SystemTime lastUpdateTime;


// ComponentIterator
// -----------------

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


// Interface
// ---------

	// subscribe a device to an interface
	void subscribeInterface(Interface &interface, Storage::Element<Device, DeviceState> e);
	
	void subscribeInterface(Interface &interface, Storage::Array<Device, DeviceState> &devices);


// LocalInterface
// --------------

	// called when a local device has sent data
	void onLocalReceived(uint8_t endpointId, uint8_t const *data, int length);

	// interface to locally connected devices
	LocalInterface localInterface;


// BusInterface
// ------------

	// called when bus interface has finished enumerating the devices
	//void onBusReady();

	// called after enumeration of bus devices or when send queue became empty
	void onBusSent();

	// called when a device connected to the bus has sent data
	void onBusReceived(uint8_t endpointId, uint8_t const *data, int length);


	// interface to devices connected via bus
	BusInterface busInterface;
	
	// startup: Index of device to subscribe when onBusSent() gets called
	int busSubscribeIndex = 0;

// Routes
// ------

	struct Route {
		Route &operator =(const Route &) = delete;

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
	
		String getSrcTopic() const {return {buffer, this->srcTopicLength};}
		String getDstTopic() const {return {buffer + this->srcTopicLength, this->dstTopicLength};}

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

	/**
	 * Command to be executed e.g. on timer event.
	 * Contains an mqtt topic where to publish a value and the value itself, both are stored in an external data buffer
	 */
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

		/**
		 * Get the size of the value in bytes
		 */
		//int getValueSize() const;
		
		/**
		 * Get the number of values (e.g. 3 for rgb color)
		 */
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

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;

		/**
		 * Begin of buffer behind the list of commands containing the value and topic
		 */
		//uint8_t *begin() {return this->u.buffer + this->commandCount * sizeof(Command);}
		//uint8_t const *begin() const {return this->u.buffer + this->commandCount * sizeof(Command);}
		
		/**
		 * End of buffer
		 */
		//uint8_t *end() {return this->u.buffer + BUFFER_SIZE;}

		// timer time and weekday flags
		ClockTime time;
	
	
		uint8_t commandCount;
		/*union {
			// list of commands, overlaps with buffer
			Command commands[MAX_COMMAND_COUNT];
			
			// buffer for commands, topics and values
			uint8_t buffer[BUFFER_SIZE];
		} u;
		*/
		uint8_t commands[BUFFER_SIZE];
	};
	
	struct TimerState {
		TimerState &operator =(const TimerState &) = delete;

		//uint16_t topicIds[Timer::MAX_COMMAND_COUNT];
		
		CommandState commands[Timer::MAX_COMMAND_COUNT];
	};
	
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
			return {this->command + sizeof(Command), command.topicLength};
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
			return {this->command + sizeof(Command), command.topicLength};
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


// Clock
// -----

	void onSecondElapsed();


// Temporary variables for editing via menu
// ----------------------------------------

	// first level
	union {
		Room room;
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
};

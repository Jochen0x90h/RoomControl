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
#include <Terminal.hpp>


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


// Storage
// -------

	// manager for flash storage
	Storage storage;

	PersistentStateManager stateManager;
	

// Configuration
// -------------

	String getRoomName() {return this->configurations[0]->name;}

	// global configuration for this room control, see Configuration.hpp (array contains only one entry)
	Storage::Array<Configuration> configurations;

	// network key
	//AesKey networkKey;

	// topic id for list of rooms in house (topic: enum)
	//uint16_t houseTopicId;

	// topic id for list of devices in room (topic: enum/<room>)
	//uint16_t roomTopicId;


// Interfaces
// ----------

	LocalInterface localInterface;
	BusInterface busInterface;
	RadioInterface radioInterface;
	AlarmInterface alarmInterface;


// Functions
// ---------

	// plug, describes an input/output of a function
	struct Plug {
		// name of plug
		String name;

		// message type that is expected/sent by the input/output
		MessageType messageType;

		// flags: 1: indexed input
		//uint8_t flags;

		bool isInput() const {return (this->messageType & MessageType::DIRECTION_MASK) == MessageType::IN;}
		bool isOutput() const {return (this->messageType & MessageType::DIRECTION_MASK) == MessageType::OUT;}
	};

	// maximum number of inputs/outputs of a function
	static constexpr int MAX_INPUT_COUNT = 16;
	static constexpr int MAX_OUTPUT_COUNT = 8;

	// connection to/from a plug of a function
	struct Connection {
		// platform and options
		enum class Interface : uint8_t {
			// local devices
			LOCAL = 0,

			// bus devices (LIN physical layer)
			BUS = 1,

			// radio (ieee 802.15.4)
			RADIO = 2,

			// alarm (pseudo-devices)
			ALARM = 3,

			// mqtt-sn with string topic
			MQTT = 4,

			COUNT = 5,
		};

		ConvertOptions convertOptions;

		// plug index to identify the input/output
		uint8_t plugIndex = 0;

		// interface
		Interface interface = Interface::LOCAL;

		// device id or length of MQTT topic string
		uint8_t deviceId = 0;

		uint8_t endpointIndex;

		// mqtt topic string follows here (first character stored in plugIndex)

		bool isMqtt() const {return this->interface == Interface::MQTT;}
	};

	struct ConnectionConstIterator {
		uint32_t const *buffer;

		ConnectionConstIterator &operator ++() {
			auto &plug = *reinterpret_cast<Connection const *>(this->buffer);
			int size = sizeof(Connection);
			if (plug.isMqtt())
				size = offsetof(Connection, endpointIndex) + plug.deviceId;
			this->buffer += (size + 3) / 4;
			return *this;
		}
		Connection const &getConnection() const {
			return *reinterpret_cast<Connection const *>(this->buffer);
		}
		String getTopic() const {
			Connection const &plug = *reinterpret_cast<Connection const *>(this->buffer);
			return {plug.deviceId, reinterpret_cast<char const *>(this->buffer) + offsetof(Connection, endpointIndex)};
		}
	};

	struct FunctionFlash;

	struct ConnectionIterator {
		uint32_t *buffer;
		FunctionFlash &flash;

		ConnectionIterator &operator ++() {
			auto &connection = *reinterpret_cast<Connection *>(this->buffer);
			int size = sizeof(Connection);
			if (connection.isMqtt())
				size = offsetof(Connection, endpointIndex) + connection.deviceId;
			this->buffer += (size + 3) / 4;
			return *this;
		}
		Connection &getConnection() {
			return *reinterpret_cast<Connection *>(this->buffer);
		}
		String getTopic() const {
			auto &connection = *reinterpret_cast<Connection *>(this->buffer);
			return {connection.deviceId, reinterpret_cast<char *>(this->buffer) + offsetof(Connection, endpointIndex)};
		}
		void setTopic(String topic);

		void insert();

		void erase();
	};

	class Function;

	struct FunctionFlash  {
		static constexpr int BUFFER_SIZE = 1024;
		
		enum class Type : uint8_t {
			// simple on/off switch
			SIMPLE_SWITCH = 1,

			// switch that turns off after a configured time
			TIMEOUT_SWITCH = 2,

			// blind with known position using timing
			TIMED_BLIND = 10,
			
			HEATING_CONTROL = 20,
		};

		enum class TypeClass : uint8_t {
			SWITCH = 1,
			BLIND = 2,
			HEATING_CONTROL = 3,
		};

		// function type
		Type type = Type::SIMPLE_SWITCH;
	
		// name is at offset 0 in buffer
		uint8_t nameLength = 0;
		
		// offset of function configuration in 32 bit units (only some functions need a configuration)
		uint8_t configOffset = 0;
		
		// number of connections to the device plugs (inputs and outputs)
		uint8_t connectionCount = 0;

		// offset of connections in 32 bit units
		uint8_t connectionsOffset = 0;

		// buffer size in 32 bit units
		uint16_t bufferSize = 0;

		// buffer for config, inputs and outputs
		uint32_t buffer[BUFFER_SIZE / 4];


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
		ConnectionConstIterator getConnections() const {return {this->buffer + this->connectionsOffset};}
		ConnectionIterator getConnections() {return {this->buffer + this->connectionsOffset, *this};}

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const {return offsetOf(FunctionFlash, buffer[this->bufferSize]);}

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		Function *allocate() const;
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
	class TimeoutSwitch : public Function {
	public:
		struct Config {
			// timeout duration in 1/100 s
			uint32_t duration;
		};

		explicit TimeoutSwitch(FunctionFlash const &flash) : Function(flash) {}
		~TimeoutSwitch() override;

		Coroutine start(RoomControl &roomControl) override;
	};


	// timed blind
	class TimedBlind : public Function {
	public:
		struct Config {
			// rocker hold time after which blind stops after release (in 1/100s)
			uint16_t holdTime;

			// blind run time from fully open to fully closed (in 1/100s)
			uint16_t runTime;
		};

		explicit TimedBlind(FunctionFlash const &flash) : Function(flash) {}
		~TimedBlind() override;

		Coroutine start(RoomControl &roomControl) override;
	};


	class HeatingControl : public Function {
	public:
		// heating regulator
		struct Config {
		};

		explicit HeatingControl(FunctionFlash const &flash) : Function(flash) {}
		~HeatingControl() override;

		Coroutine start(RoomControl &roomControl) override;
	};


	Interface &getInterface(Connection const &connection);

	//Coroutine testSwitch();

	int connectFunction(RoomControl::FunctionFlash const &flash, Array<Plug const> plugs,
		Array<Subscriber, MAX_INPUT_COUNT> subscribers, PublishInfo::Barrier &barrier,
		Array<Publisher, MAX_OUTPUT_COUNT> publishers);

	// list of functions
	Storage::Array<Function> functions;


// Menu Helpers
// ------------

	void printConnection(Menu::Stream &stream, Connection const &connection);

	// get array of switch states
	Array<String const> getSwitchStates(Usage usage);

	// get switch type (binary/ternary, stable/one-shot)
	int getSwitchType(MessageType type);

	// default convert options for message logger and generator
	ConvertOptions getDefaultConvertOptions(MessageType type);

	// default convert options for connections
	ConvertOptions getDefaultConvertOptions(MessageType dstType, MessageType srcType);


	// get default float value for given usage
	float getDefaultFloatValue(Usage usage);

	// do a step on a value
	float stepValue(Usage usage, bool relative, float value, int delta);

	// get a value for display including unit conversion
	Flt getDisplayValue(Usage usage, bool relative, float value);

	// get the unit for display, depending on user settings
	String getDisplayUnit(Usage usage);

	void editMessage(Menu::Stream &stream, MessageType messageType,
		Message &message, bool editMessage1, bool editMessage2, int delta);

	void editConvertSwitch2Switch(Menu &menu, ConvertOptions &convertOptions,
		Array<String const> const &dstStates, Array<String const> const &srcStates);

	void editConvertFloat2Switch(Menu &menu, ConvertOptions &convertOptions,
		Array<String const> const &dstStates, Usage srcUsage);

	void editConvertSwitch2FloatCommand(Menu &menu, ConvertOptions &convertOptions, Usage dstUsage,
		Array<String const> const &srcCommands);

	void editConvertOptions(Menu &menu, ConvertOptions &convertOptions, MessageType dstType, MessageType srcType);

// Menu
// ----

	// display
	SwapChain swapChain;

	Coroutine idleDisplay();
	[[nodiscard]] AwaitableCoroutine mainMenu();
	[[nodiscard]] AwaitableCoroutine devicesMenu(Interface &interface);
	[[nodiscard]] AwaitableCoroutine deviceMenu(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine alarmsMenu(AlarmInterface &alarms);
	[[nodiscard]] AwaitableCoroutine alarmMenu(AlarmInterface &alarms, uint8_t id, AlarmInterface::AlarmData &data);
	[[nodiscard]] AwaitableCoroutine alarmTimeMenu(AlarmTime &time);
	[[nodiscard]] AwaitableCoroutine endpointsMenu(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine messageLogger(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine messageGenerator(Interface::Device &device);
	[[nodiscard]] AwaitableCoroutine functionsMenu();
	[[nodiscard]] AwaitableCoroutine functionMenu(int index, FunctionFlash &flash);
	[[nodiscard]] AwaitableCoroutine measureRunTime(Interface::Device &device, Connection const &connection, uint16_t &measureRunTime);
	[[nodiscard]] AwaitableCoroutine editFunctionConnection(ConnectionIterator &it, Plug const &plug, Connection connection, bool add);
	[[nodiscard]] AwaitableCoroutine selectFunctionDevice(Connection &connection, Plug const &plug);
	[[nodiscard]] AwaitableCoroutine selectFunctionEndpoint(Interface::Device &device, Connection &connection, Plug const &plug);
};

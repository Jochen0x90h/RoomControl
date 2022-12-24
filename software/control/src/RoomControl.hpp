#pragma once

#include "LocalInterface.hpp"
#include "BusInterface.hpp"
#include "RadioInterface.hpp"
#include "AlarmInterface.hpp"
#include "FunctionInterface.hpp"
#include "SwapChain.hpp"
#include "Menu.hpp"
#include <MqttSnBroker.hpp> // include at first because of strange compiler error
#include <ClockTime.hpp>
#include <Coroutine.hpp>
#include <StringBuffer.hpp>
#include <StringSet.hpp>
#include <TopicBuffer.hpp>
#include <Terminal.hpp>
#undef DELETE
#undef IGNORE


/**
 * Main room control class that inherits platform dependent (hardware or emulator) components
 */
class RoomControl {
public:
	// maximum number of mqtt routes
	static constexpr int MAX_ROUTE_COUNT = 32;
	
	// maximum number of timers
	static constexpr int MAX_TIMER_COUNT = 32;


	RoomControl(Drivers &drivers);

	~RoomControl();




// Drivers
// -------

	QuadratureDecoder &decoder;
	Storage &storage;


// Configuration
// -------------

	void applyConfiguration();

	void saveConfiguration();

	Configuration configuration;


// Interfaces
// ----------

	static constexpr int LOCAL_INTERFACE = 0;
	static constexpr int BUS_INTERFACE = 1;
	static constexpr int RADIO_INTERFACE = 2;
	static constexpr int ALARM_INTERFACE = 3;
	static constexpr int FUNCTION_INTERFACE = 4;
	static constexpr int INTERFACE_COUNT = 5;

	LocalInterface localInterface;
	BusInterface busInterface;
	RadioInterface radioInterface;
	AlarmInterface alarmInterface;
	FunctionInterface functionInterface;

	Interface *interfaces[INTERFACE_COUNT];


// Connections
// -----------

	// list of all connections to one destination device
	struct Connections {
		Connections *next;

		uint8_t interfaceIndex;
		uint8_t elementId;

		// dynamically allocated lists of data and subscribers
		uint8_t count;
		Connection *data;
		Subscriber *subscribers;
	};

	// temporary list of connections to one destination device, used while editing
	struct TempConnections {
		static constexpr int MAX_CONNECTIONS = 64;

		Connections **p = nullptr;
		uint8_t count = 0;
		Connection data[MAX_CONNECTIONS];

		void insert(int index, Connection const &connection) {
			array::insert(this->data + index, this->data + this->count, 1);
			++this->count;
			this->data[index] = connection;
		}
		void erase(int index) {
			array::erase(this->data + index, this->data + this->count, 1);
			--this->count;
		}
	};

	void connect(Connections *connections);
	TempConnections getConnections(uint8_t interfaceIndex, uint8_t deviceId);
	void writeConnections(uint8_t interfaceIndex, uint8_t deviceId, TempConnections &tc);
	void eraseConnections(uint8_t interfaceIndex, uint8_t deviceId, TempConnections &tc);
	void printSource(Menu::Stream &stream, Source const &source);

	MessageType getSrcType(Source const &source) {
		auto &interface = *this->interfaces[source.interfaceId];
		auto plugs = interface.getPlugs(source.elementId);
		if (source.plugIndex < plugs.count())
			return plugs[source.plugIndex];
		return MessageType::INVALID;
	}

	// linked list of connections
	Connections *connectionsList = nullptr;


// Listeners
// ---------

	// message that gets displayed, stored in flash
	struct DisplaySource {
		uint8_t elementId;
		uint8_t plugIndex;
	};

	// list of messages that get displayed
	struct DisplaySources {
		int count = 0;
		DisplaySource *sources = nullptr;

		bool contains(uint8_t deviceId, uint8_t plugIndex) const;
	};

	struct TempDisplaySources {
		static constexpr int MAX_SOURCES = 64;

		int count = 0;
		DisplaySource sources[MAX_SOURCES];


		TempDisplaySources(DisplaySources const &displaySources);
		void insert(uint8_t deviceId, uint8_t plugIndex);
		void erase(uint8_t deviceId, uint8_t plugIndex);
		bool contains(uint8_t deviceId, uint8_t plugIndex) const;
	};

	void listen(ListenerBarrier &barrier, Listener *listeners);

	void writeDisplaySources(int interfaceIndex, TempDisplaySources &tempDisplaySources);

	Coroutine displayMessageFilter();

	// list of display sources, one per interface
	DisplaySources displaySourcesList[INTERFACE_COUNT];

	// barrier for messages to be displayed in idle mode
	ListenerBarrier displayMessageBarrier;


// Helpers
// -------

	// get array of switch states
	Array<String const> getSwitchStates(Usage usage);

	// get switch type (binary/ternary, stable/one-shot)
	int getSwitchType(MessageType type);

	// print a message value including unit to a stream
	void printMessage(Stream &stream, MessageType type, Message &message);


// Menu Helpers
// ------------


	// default convert options for message logger and generator
	//ConvertOptions getDefaultConvertOptions(MessageType type);

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

	void editConvertInt2FloatCommand(Menu &menu, ConvertOptions &convertOptions, Usage dstUsage);

	void editConvertOptions(Menu &menu, ConvertOptions &convertOptions, MessageType dstType, MessageType srcType);


// Menu
// ----

	enum class Result {
		OK,
		CANCEL,
		DELETE
	};

	// display
	SwapChain swapChain;

	Coroutine idleMenu();
	[[nodiscard]] AwaitableCoroutine mainMenu();

	// local, bus and radio devices
	[[nodiscard]] AwaitableCoroutine devicesMenu(int interfaceIndex);
	[[nodiscard]] AwaitableCoroutine deviceMenu(int interfaceIndex, uint8_t id);

	// alarms
	[[nodiscard]] AwaitableCoroutine alarmsMenu();
	[[nodiscard]] AwaitableCoroutine alarmMenu(AlarmInterface::Data &data);
	[[nodiscard]] AwaitableCoroutine alarmTimeMenu(AlarmTime &time);

	// functions
	[[nodiscard]] AwaitableCoroutine functionsMenu();
	[[nodiscard]] AwaitableCoroutine functionMenu(FunctionInterface::DataUnion &data);
	[[nodiscard]] AwaitableCoroutine measureRunTime(uint8_t deviceId, uint8_t plugIndex, uint16_t &runTime);

	// connections
	[[nodiscard]] AwaitableCoroutine connectionsMenu(Array<MessageType const> plugs, TempConnections &tc);
	[[nodiscard]] AwaitableCoroutine editConnection(Connection &connection, MessageType dstType, Result &result);
	[[nodiscard]] AwaitableCoroutine selectDevice(Connection &connection, MessageType dstType);
	[[nodiscard]] AwaitableCoroutine detectSource(Source &source, MessageType dstType);
	[[nodiscard]] AwaitableCoroutine selectPlug(Interface &interface, uint8_t deviceId, Connection &connection, MessageType dstType);

	// helpers
	[[nodiscard]] AwaitableCoroutine plugsMenu(Interface &interface, uint8_t deviceId, TempDisplaySources &tempDisplaySources);
	[[nodiscard]] AwaitableCoroutine messageLogger(Interface &interface, uint8_t deviceId);
	[[nodiscard]] AwaitableCoroutine messageGenerator(Interface &interface, uint8_t deviceId);

};

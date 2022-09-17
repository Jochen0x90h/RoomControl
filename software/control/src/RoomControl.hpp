#pragma once

#include "LocalInterface.hpp"
#include "BusInterface.hpp"
#include "RadioInterface.hpp"
#include "AlarmInterface.hpp"
#include "FunctionInterface.hpp"
#include "SwapChain.hpp"
#include "Menu.hpp"
#include <MqttSnBroker.hpp> // include at first because of strange compiler error
#include <State.hpp>
#include <ArrayStorage.hpp>
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


// Storage
// -------

	PersistentStateManager stateManager;
	

// Configuration
// -------------

	void applyConfiguration();

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

	// connections to one input plug
	struct Connections {
		Connections *next;

		uint8_t interfaceIndex;
		uint8_t deviceId;

		uint8_t count;
		ConnectionData *data;
		Subscriber *subscribers;
	};

	struct TempConnections {
		static constexpr int MAX_CONNECTIONS = 64;

		Connections **p = nullptr;
		uint8_t count = 0;
		ConnectionData data[MAX_CONNECTIONS];

		void insert(int index, ConnectionData const &connection) {
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
	void printConnection(Menu::Stream &stream, ConnectionData const &connection);

	Connections *connectionGroups = nullptr;


// Menu Helpers
// ------------

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

	// connections
	[[nodiscard]] AwaitableCoroutine connectionsMenu(Array<MessageType const> plugs, TempConnections &tc);
	[[nodiscard]] AwaitableCoroutine editFunctionConnection(TempConnections &tc, int index, MessageType dstType, ConnectionData connection, bool add);
	[[nodiscard]] AwaitableCoroutine selectFunctionDevice(ConnectionData &connection, MessageType dstType);
	[[nodiscard]] AwaitableCoroutine selectFunctionPlug(Interface &interface, uint8_t id, ConnectionData &connection, MessageType dstType);

	// helpers
	[[nodiscard]] AwaitableCoroutine plugsMenu(Interface &interface, uint8_t id);
	[[nodiscard]] AwaitableCoroutine messageLogger(Interface &interface, uint8_t id);
	[[nodiscard]] AwaitableCoroutine messageGenerator(Interface &interface, uint8_t id);
/*
	[[nodiscard]] AwaitableCoroutine measureRunTime(Interface &interface, uint8_t id, Connection const &connection, uint16_t &measureRunTime);
*/
};

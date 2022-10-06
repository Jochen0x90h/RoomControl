#pragma once

#include "Interface.hpp"
#include <Coroutine.hpp>
#include <Data.hpp>


class FunctionInterface : public Interface {
public:
	// maximum number of functions
	static constexpr int MAX_FUNCTION_COUNT = 128;

	FunctionInterface(uint8_t interfaceId);
	~FunctionInterface() override;

	String getName() override;
	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	SubscriberTarget getSubscriberTarget(uint8_t id, uint8_t plugIndex) override;
	void subscribe(Subscriber &subscriber) override;
	void listen(Listener &listener) override;
	void erase(uint8_t id) override;


	enum class Type : uint8_t {
		INVALID = 0,

		// on/off switch with timeout
		SWITCH = 1,

		// light with timeout and on/off transition times
		LIGHT = 2,

		// color light with timeout and on/off transition times
		COLOR_LIGHT = 3,

		ANIMATED_LIGHT = 4,

		// blind with known position using timing
		TIMED_BLIND = 10,

		HEATING_CONTROL = 20,
	};

	// function data that is stored in flash
	struct Data {
		uint8_t id;

		// function type
		Type type;

		// function name, zero-terminated if shorter than maximum length
		char name[MAX_NAME_LENGTH];
	};

	struct SwitchData : public Data {
		// timeout duration in 1/100 s
		uint32_t timeout;
	};

	struct LightData : public Data {
		// timeout duration in 1/100 s
		uint32_t timeout;


		// fade times in 1/10 s
		uint16_t offFade;
		uint16_t timeoutFade;

		struct Setting {
			// brightness
			uint8_t brightness;

			// fade on or to this setting in 1/10 s
			uint16_t fade;
		};

		uint16_t settingCount;
		Setting settings[4];
	};

	struct ColorSetting {
		uint8_t brightness;
		uint8_t hue;
		uint8_t saturation;
		uint16_t fade; // 1/10s
	};

	struct ColorLightData : public Data {
		// timeout duration in 1/100 s
		uint32_t timeout;

		// fade times in 1/10 s
		uint16_t offFade;
		uint16_t timeoutFade;

		uint16_t settingCount;
		ColorSetting settings[4];
	};

	struct AnimatedLightData : public Data {
		// timeout duration in 1/100 s
		uint32_t timeout;

		// fade times in 1/10 s
		uint16_t onFade;
		uint16_t offFade;
		uint16_t timeoutFade;

		uint8_t stepCount;
		ColorSetting steps[16];
	};

	struct TimedBlindData : public Data {
		// rocker hold time after which blind stops after release (in 1/100s)
		uint16_t holdTime;

		// blind run time from fully open to fully closed (in 1/100s)
		uint16_t runTime;
	};

	struct HeatingControlData : public Data {
	};


	union DataUnion {
		Data data;
		SwitchData switchData;
		LightData lightData;
		ColorLightData colorLightData;
		AnimatedLightData animatedLightData;
		TimedBlindData timedBlindData;
		HeatingControlData heatingControlData;
	};

	// edit helpers
	void getData(uint8_t id, DataUnion &data) const;
	void set(DataUnion &data);
	static String getName(Type type);
	static Type getNextType(Type type, int delta);
	static void setType(DataUnion &data, Type type);
	static Array<MessageType const> getPlugs(Type type);

	// configuration methods
	void publishSwitch(uint8_t id, uint8_t plugIndex, uint8_t value);


	class Function : public Device {
	public:
		// adds to linked list and takes ownership of the data
		Function(FunctionInterface *interface, Data *data)
			: Device(data->id, interface->listeners), next(interface->functions), data(data)
		{
			interface->functions = this;
		}

		~Function();


		// next function
		Function *next;

		// function data that is stored in flash
		Data *data;

		// each function has its own coroutine
		Coroutine coroutine;

		// coroutines wait here until something gets published to them
		SubscriberBarrier barrier;
	};

	int functionCount = 0;
	uint8_t functionIds[MAX_FUNCTION_COUNT];
	Function *functions = nullptr;

	// find function by id
	Function *findFunction(uint8_t id) const;

	uint8_t allocateId();

	// listeners that listen on all messages of the interface (as opposed to subscribers that subscribe to one plug)
	ListenerList listeners;
};

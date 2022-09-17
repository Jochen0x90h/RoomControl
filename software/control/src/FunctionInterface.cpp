#include "FunctionInterface.hpp"
#include "Cie1931.hpp"
#include <Timer.hpp>
#include <Storage.hpp>
#include <Terminal.hpp>
#include <StringOperators.hpp>
#include <util.hpp>
#include <appConfig.hpp>


constexpr float CELSIUS_OFFSET = 273.15f;


// helper class
struct OnOff {
	uint8_t state = 0;

	bool apply(uint8_t command) {
		if (command < 2) {
			bool changed = this->state != command;
			this->state = command;
			return changed;
		} else if (command == 2) {
			this->state ^= 1;
			return true;
		}
		return false;
	}

	operator bool() const {return this->state != 0;}
};

struct FloatValue {
	float value = 0.0f;

	void apply(Message const &message) {
		if (message.command == 0) {
			// set
			this->value = message.value.f32;
		} else {
			// step
			this->value += message.value.f32;
		}
	}

	operator float() const {return this->value;}
};

struct TypeInfo {
	FunctionInterface::Type type;
	String name;
	std::initializer_list<MessageType> plugs;
	int (*size) (FunctionInterface::Data *data);
	Coroutine (*start) (FunctionInterface::Function *function);
};

static const TypeInfo typeInfos[] = {
	{
		FunctionInterface::Type::SWITCH,
		"Switch",
		{
			MessageType::BINARY_POWER_CMD_IN,
			MessageType::BINARY_POWER_OUT
		},
		[](FunctionInterface::Data *data) {
			return sizeOf(FunctionInterface::LightData);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::SwitchData const *>(function->data);
			auto timeout = data.timeout * 10ms;
			OnOff on;
			while (true) {
				// wait for message
				MessageInfo info;
				uint8_t message;
				if (!on || timeout == 0ms) {
					// off or no timeout: wait for message
					co_await function->publishBarrier.wait(info, &message);
				} else {
					// on: wait for message or timeout
					int s = co_await select(function->publishBarrier.wait(info, &message), Timer::sleep(timeout));
					if (s == 2) {
						// timeout: switch off
						message = 0;
					}
				}

				// process message
				bool changed = on.apply(message);

				// publish on/off
				if (changed)
					function->subscribers.publishSwitch(1, on.state);
			}
		}
	},
	{
		FunctionInterface::Type::LIGHT,
		"Light",
		{
			MessageType::BINARY_POWER_LIGHT_CMD_IN,
			MessageType::BINARY_POWER_LIGHT_OUT,
			MessageType::LIGHTING_BRIGHTNESS_CMD_OUT
		},
		[](FunctionInterface::Data *data) {
			auto d = reinterpret_cast<FunctionInterface::LightData *>(data);
			return offsetOf(FunctionInterface::LightData, settings[d->settingCount]);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::LightData const *>(function->data);
			auto timeout = data.timeout * 10ms;

			// current state
			OnOff on;

			// fade off either by switching off or after timeout
			auto switchOffFade = data.offFade;
			auto timeoutFade = data.timeoutFade;
			uint16_t offFade = 0;

			// current color setting
			int settingIndex = -1;

			// transition
			bool transition = false;
			SystemTime endTime;

			// timeout
			SystemTime offTime;

			while (true) {
				// current time
				SystemTime now;

				// wait for message
				MessageInfo info;
				uint8_t message;
				bool timeoutActive = timeout > 0ms && on;
				if (!timeoutActive && !transition) {
					// timeout not active and no transition in progress: wait for message
					co_await function->publishBarrier.wait(info, &message);
					//Terminal::out << "plug " << dec(info.plug.id) << '\n';
					now = Timer::now();
				} else {
					// on: wait for message or timeout
					//Terminal::out << "select" << '\n';
					bool off = timeoutActive && (!transition || offTime <= endTime);
					now = off ? offTime : endTime;
					int s = co_await select(function->publishBarrier.wait(info, &message), Timer::sleep(now));

					// "relaxed" time to prevent lagging behind
					now = Timer::now();

					if (s == 1) {
						// switched on or off

						// "precise" time that may lag behind if cpu is overloaded
						//now = Timer::now();

						// set off fade
						offFade = switchOffFade;
					} else {
						// timeout
						if (off) {
							// off timeout
							//Terminal::out << "off\n";
							message = 0;

							// set off fade
							offFade = timeoutFade;
						} else {
							// end of transition
							//Terminal::out << "transition end\n";
							transition = false;

							// nothing to do
							continue;
						}
					}
					//Terminal::out << "select " << dec(s) << " plug " << dec(info.plug.id) << '\n';
				}

				// process message
				bool changed = on.apply(message);

				// set or reset off time
				if (on) {
					offTime = now + timeout;
				}

				// change color setting
				bool force = false;
				int newSettingIndex = info.sourceIndex % data.settingCount;
				if (on && settingIndex != newSettingIndex) {
					settingIndex = newSettingIndex;
				} else if (!changed) {
					// current state is confirmed (off -> off or on -> on)
					if (transition) {
						// interrupt transition and set immediately
						transition = false;
						force = true;
					} else {
						// nothing to do
						continue;
					}
				}
				auto &setting = data.settings[settingIndex];

				// get color setting
				float brightness;
				int brightnessFade;
				if (on) {
					// on: fade on or to new color
					int fade = setting.fade;

					// fade to brightness
					brightness = float(setting.brightness) * 0.01f;
					brightnessFade = fade;
				} else {
					// off
					brightness = 0.0f;
					brightnessFade = offFade;
				}

				// start transition or set immediately
				if (!force) {
					// start transition
					transition = brightnessFade > 0;
					endTime = now + brightnessFade * 100ms;
				} else {
					// set immediately (1/10s)
					transition = false;
					brightnessFade = 1;
					force = false;
				}

				// publish on/off
				if (changed)
					function->subscribers.publishSwitch(1, on.state);

				// publish brightness
				function->subscribers.publishFloatTransition(2, brightness, 0, brightnessFade);
			}
		}
	},
	{
		FunctionInterface::Type::COLOR_LIGHT,
		"Color Light",
		{
			MessageType::BINARY_POWER_LIGHT_CMD_IN,
			MessageType::BINARY_POWER_LIGHT_OUT,
			MessageType::LIGHTING_BRIGHTNESS_CMD_OUT,
			MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X_CMD_OUT,
			MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y_CMD_OUT
		},
		[](FunctionInterface::Data *data) {
			auto d = reinterpret_cast<FunctionInterface::ColorLightData *>(data);
			return offsetOf(FunctionInterface::ColorLightData, settings[d->settingCount]);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::ColorLightData const *>(function->data);

			OnOff on;

			auto timeout = data.timeout * 10ms;
			auto settings = data.settings;

			// fade off either by switching off or after timeout
			auto switchOffFade = data.offFade;
			auto timeoutFade = data.timeoutFade;
			uint16_t offFade;

			// color settings
			int settingIndex = -1;

			// current time
			SystemTime now;

			// transition
			bool transition = false;
			SystemTime endTime;

			// timeout
			SystemTime offTime;

			// no references to flash allowed beyond this point as flash may be reallocated
			while (true) {
				// wait for message
				MessageInfo info;
				uint8_t message;
				bool timeoutActive = timeout > 0ms && on;
				if (!timeoutActive && !transition) {
					// timeout not active and no transition in progress: wait for message
					co_await function->publishBarrier.wait(info, &message);
					//Terminal::out << "plug " << dec(info.plug.id) << '\n';
					now = Timer::now();
				} else {
					// on: wait for message or timeout
					//Terminal::out << "select" << '\n';
					bool off = timeoutActive && (!transition || offTime <= endTime);
					now = off ? offTime : endTime;
					int s = co_await select(function->publishBarrier.wait(info, &message), Timer::sleep(now));

					// "relaxed" time to prevent lagging behind
					now = Timer::now();

					if (s == 1) {
						// switched on or off

						// "precise" time that may lag behind if cpu is overloaded
						//now = Timer::now();

						// set off fade
						offFade = switchOffFade;
					} else {
						// timeout
						if (off) {
							// off timeout
							//Terminal::out << "off\n";
							message = 0;

							// set off fade
							offFade = timeoutFade;
						} else {
							// end of transition
							//Terminal::out << "transition end\n";
							transition = false;

							// nothing to do
							continue;
						}
					}
					//Terminal::out << "select " << dec(s) << " plug " << dec(info.plug.id) << '\n';
				}

				// process message
				bool changed = on.apply(message);

				// set or reset off time
				if (on) {
					offTime = now + timeout;
				}

				// change color setting
				bool setColor = false;
				bool force = false;
				if (on && settingIndex != info.sourceIndex) {
					settingIndex = info.sourceIndex;
					setColor = true;
				} else if (!changed) {
					// current state is confirmed (off -> off or on -> on)
					if (transition) {
						// interrupt transition and set immediately
						transition = false;
						force = true;
					} else {
						// nothing to do
						continue;
					}
				}
				auto &setting = settings[settingIndex];

				// get color setting
				float brightness;
				int brightnessFade;
				Cie1931 color;
				int colorFade;
				if (on) {
					// on: fade on or to new color
					int fade = setting.fade;

					// fade to brightness
					brightness = float(setting.brightness) * 0.01f;
					brightnessFade = fade;

					// fade to color
					color = hueToCie(float(setting.hue * 5), setting.saturation * 0.01f);
					if (!transition && changed) {
						// set color immediately when switched on while no transition was going on
						colorFade = min(fade, 1); // 0 does not seem to work on a philips hue
						setColor = true;
					} else {
						colorFade = fade;
					}
				} else {
					// off
					brightness = 0.0f;
					brightnessFade = offFade;
				}

				// start transition or set immediately
				if (!force) {
					// start transition
					transition = brightnessFade > 0;
					endTime = now + brightnessFade * 100ms;
				} else {
					// set immediately (1/10s)
					//transition = false;
					brightnessFade = 1;
					colorFade = 1;
					setColor = on;
					force = false;
				}
				/*
					Terminal::out << "brightness " << flt(brightness, 5) << '\n';
				if (setColor)
					Terminal::out << "color " << dec(settingIndex) << " xy " << flt(color.x, 1) << ' ' << flt(color.y, 1) << " fade " << dec(colorFade) << '\n';
				*/

				// publish on/off
				if (changed)
					function->subscribers.publishSwitch(1, on.state);

				// publish brightness
				function->subscribers.publishFloatTransition(2, brightness, 0, brightnessFade);

				// publish color x
				if (setColor)
					function->subscribers.publishFloatTransition(3, color.x, 0, colorFade);

				// publish color y
				if (setColor)
					function->subscribers.publishFloatTransition(4, color.y, 0, colorFade);
			}
		}
	},
	{
		FunctionInterface::Type::ANIMATED_LIGHT,
		"Animated Light",
		{
			MessageType::BINARY_POWER_LIGHT_CMD_IN,
			MessageType::BINARY_POWER_LIGHT_OUT,
			MessageType::LIGHTING_BRIGHTNESS_CMD_OUT,
			MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X_CMD_OUT,
			MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y_CMD_OUT
		},
		[](FunctionInterface::Data *data) {
			auto d = reinterpret_cast<FunctionInterface::AnimatedLightData *>(data);
			return offsetOf(FunctionInterface::AnimatedLightData, steps[d->stepCount]);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::AnimatedLightData const *>(function->data);

			OnOff on;

			auto timeout = data.timeout * 10ms;
			auto stepCount = data.stepCount;
			auto steps = data.steps;

			// fade on
			auto onFade = data.onFade;

			// fade off either by switching off or after timeout
			auto switchOffFade = data.offFade;
			auto timeoutFade = data.timeoutFade;
			uint16_t offFade;

			// current time
			SystemTime now;

			// on/off transition
			bool transition = false;
			SystemTime startTime = {};
			SystemTime endTime = {};

			// animation steps
			int stepIndex = 0;
			SystemTime stepTime;
			SystemTime nextTime;

			// timeout
			SystemTime offTime;

			//SystemTime brightnessTime = Timer::now();

			// no references to flash allowed beyond this point as flash may be reallocated
			while (true) {
				// wait for message
				MessageInfo info;
				uint8_t message;
				bool setColor = false;
				if (!on && !transition) {
					// off: wait for message
					co_await function->publishBarrier.wait(info, &message);
					//Terminal::out << "plug " << dec(info.plug.id) << '\n';
					now = Timer::now();
					//setColor = true;
				} else {
					// on: wait for message or timeout
					//Terminal::out << "select" << '\n';
					now = nextTime;
					if (transition && endTime <= now)
						now = endTime;
					bool off = timeout > 0ms && on && offTime <= now;
					if (off)
						now = offTime;
					int s = co_await select(function->publishBarrier.wait(info, &message), Timer::sleep(now));

					// "relaxed" time to prevent lagging behind
					now = Timer::now();

					if (s == 1) {
						// switched on or off

						// "precise" time that may lag behind if cpu is overloaded
						//now = Timer::now();

						// set off fade
						offFade = switchOffFade;
					} else {
						// timeout
						if (off) {
							// off timeout
							//Terminal::out << "off" << '\n';
							message = 0;

							// set off fade
							offFade = timeoutFade;
						} else {
							// next animation step or end of transition: don't change state
							message = 3;
						}
					}

					// check check if we need to fade to next animation step
					if (now >= nextTime) {
						//Terminal::out << "fade to next" << '\n';
						++stepIndex;
						if (stepIndex >= stepCount)
							stepIndex = 0;
						stepTime = now;
						setColor = true;
					}

					// check if transition ended
					if (now >= endTime)
						transition = false;

					//Terminal::out << "select " << dec(s) << " plug " << dec(info.plug.id) << '\n';
				}

				// process message
				bool changed = on.apply(message);

				// set or reset off time
				if (on && message < 3) {
					offTime = now + timeout;
				}

				bool force = false;
				if (changed) {
					// changed: merge with transition that is still in progress
					auto duration = (on ? onFade : offFade) * 100ms;
					if (transition/*now > startTime && now < endTime*/) {
						float s = (now - startTime) / (endTime - startTime);
						//Terminal::out << "s " << flt(s, 2) << '\n';
						startTime = now - (1.0f - s) * duration;
					} else {
						startTime = now;
					}
					endTime = startTime + duration;

					if (on) {
						// off -> on
						//Terminal::out << "off -> on" << '\n';

						// set time of current step
						stepTime = now;

						// set first color
						setColor = true;
					} else {
						// on -> off
						//Terminal::out << "on -> off" << '\n';
					}

					// transition is now in progress
					transition = true;
				} else {
					// not changed
					if (transition && message < 3) {
						// interrupt transition and set immediately
						//Terminal::out << "force\n";
						transition = false;
						force = true;
					} else if (!setColor || (!on && !transition)) {
						// nothing to do
						//Terminal::out << "nothing to do\n";
						continue;
					}
				}

				// get animation step
				auto &step = steps[stepIndex];
				auto fade = max(step.fade, 1);
				nextTime = stepTime + fade * 100ms;
				//Terminal::out << "index " << dec(index) << " hue " << dec(step.hue * 5) << " x " << flt(color.x, 2) << " fade " << dec(fade) << '\n';
				float brightness = float(step.brightness) * 0.01f;
				int brightnessFade = fade;
				auto color = hueToCie(float(step.hue * 5), step.saturation * 0.01f);
				int colorFade = fade;
				if (force) {
					// force: set brightness immediately (1/10s)
					brightnessFade = 1;
					if (!on)
						brightness = 0.0f;
					force = false;
				} else if (on) {
					// on: fade on or to new color
					if (transition) {
						// off -> on
						if (nextTime >= endTime) {
							if (changed)
								brightnessFade = onFade;
						} else {
							brightness *= float((nextTime - startTime) / (endTime - startTime));
						}
					}
				} else {
					// off
					if (transition) {
						// on -> off
						if (nextTime >= endTime) {
							if (changed)
								brightnessFade = offFade;
							else
								brightnessFade = int((endTime - stepTime) / 100ms);
							brightness = 0.0f;
						} else {
							brightness *= 1.0f - float((nextTime - startTime) / (endTime - startTime));
						}
					}
				}
				/*
						auto n = Timer::now();
						Terminal::out << "brightness " << flt(brightness, 5) << " t " << dec((n - brightnessTime).value / 100)
							<< " transition " << dec(transition) << '\n';
						brightnessTime = n;
						if (setColor)
							Terminal::out << "color " << dec(stepIndex) << '\n';
				*/
				// publish on/off
				if (changed)
					function->subscribers.publishSwitch(1, on.state);

				// publish brightness
				function->subscribers.publishFloatTransition(2, brightness, 0, brightnessFade);

				// publish color x/y
				if (setColor) {
					function->subscribers.publishFloatTransition(3, color.x, 0, colorFade);
					function->subscribers.publishFloatTransition(4, color.y, 0, colorFade);
				}
			}
		}
	},
	{
		FunctionInterface::Type::TIMED_BLIND,
		"Blind",
		{
			MessageType::TERNARY_BUTTON_IN,
			MessageType::BINARY_BUTTON_IN,
			MessageType::LEVEL_OPEN_BLIND_CMD_IN,
			MessageType::BINARY_ENABLE_CLOSE_IN,
			MessageType::TERNARY_OPENING_BLIND_OUT, // stop/opening/closing
			MessageType::LEVEL_OPEN_BLIND_OUT
		},
		[](FunctionInterface::Data *data) {
			return sizeOf(FunctionInterface::TimedBlindData);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::TimedBlindData const *>(function->data);

			// state
			uint8_t state = 0;
			bool up = false;
			uint8_t enableDown = 1;

			// rocker timeout
			SystemDuration const holdTime = data.holdTime * 10ms;

			// position
			SystemDuration const maxPosition = data.runTime * 10ms;
			SystemDuration position = maxPosition / 2;
			SystemDuration targetPosition = 0s;
			SystemTime startTime;
			SystemTime lastTime;

			// no references to flash allowed beyond this point as flash may be reallocated
			while (true) {
				// wait for message
				MessageInfo info;
				Message message;
				if (state == 0) {
					co_await function->publishBarrier.wait(info, &message);
					//Terminal::out << "plug " << dec(info.plug.id) << '\n';
				} else {
					auto d = targetPosition - position;

					// wait for event or timeout with a maximum to regularly report the current position
					int s = co_await select(function->publishBarrier.wait(info, &message), Timer::sleep(min(up ? -d : d, 200ms)));

					// set invalid plug index when timeout occurred
					if (s != 1)
						info.plugIndex = 255;
					//Terminal::out << "select " << dec(s) << " plug " << dec(info.plug.id) << '\n';

					// get time since last time
					auto time = Timer::now();
					d = time - lastTime;
					lastTime = time;

					// update position
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
				switch (info.plugIndex) {
				case 0:
				case 1:
					// up/down or trigger in
					if (message.value.u8 == 0) {
						// released: stop if timeout elapsed
						if (Timer::now() > startTime + holdTime)
							targetPosition = position;
					} else {
						// up or down pressed
						if (state == 0) {
							// start if currently stopped
							if (info.plugIndex == 1) {
								// trigger
								targetPosition = ((up || (targetPosition == 0s)) && targetPosition < maxPosition) ? maxPosition : 0s;
							} else if (message.value.u8 == 1) {
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
					break;
				case 2: {
					// position in
					auto p = maxPosition * message.value.f32;
					if (message.command == 0) {
						// set position
						Terminal::out << "set position " << flt(message.value.f32) << '\n';
						targetPosition = p;
					} else {
						// step position
						Terminal::out << "step position " << flt(message.value.f32) << '\n';
						targetPosition += p;
						break;
					}
					if (targetPosition < 0s)
						targetPosition = 0s;
					if (targetPosition > maxPosition)
						targetPosition = maxPosition;
					break;
				}
				case 3:
					// enable down
					enableDown = message.value.u8;
					break;
				}

				// stop if down is disabled and direction is down
				if (!enableDown && targetPosition > position)
					targetPosition = position;

				// check if target position already reached
				if (position == targetPosition) {
					// stop
					state = 0;
				} else if (state == 0) {
					// start
					up = targetPosition < position;
					state = up ? 1 : 2;
					lastTime = startTime = Timer::now();
				}

				// publish up/down
				function->subscribers.publishSwitch(4, state);

				// publish position
				function->subscribers.publishFloat(5, position / maxPosition);
			}
		}
	},
	{
		FunctionInterface::Type::HEATING_CONTROL,
		"Heating Control",
		{
			MessageType::BINARY_POWER_AC_CMD_IN,
			MessageType::BINARY_IN,
			MessageType::BINARY_IN,
			MessageType::BINARY_OPEN_WINDOW_IN,
			MessageType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER_CMD_IN,
			MessageType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_IN,
			MessageType::BINARY_OPEN_VALVE_OUT
		},
		[](FunctionInterface::Data *data) {
			return sizeOf(FunctionInterface::HeatingControlData);
		},
		[](FunctionInterface::Function *function) -> Coroutine {
			auto const &data = *reinterpret_cast<FunctionInterface::HeatingControlData const *>(function->data);

			uint8_t valve = 0;

			OnOff on;
			OnOff night;
			OnOff summer;
			uint32_t windows = 0;
			FloatValue setTemperature = {20.0f + CELSIUS_OFFSET};
			float temperature = 20.0f + 273.15f;

			// no references to flash allowed beyond this point as flash may be reallocated
			while (true) {
				// wait for message
				MessageInfo info;
				Message message;
				co_await function->publishBarrier.wait(info, &message);

				// process message
				switch (info.plugIndex) {
				case 0:
					// on/off in
					on.apply(message.value.u8);
					break;
				case 1:
					// night in
					night.apply(message.value.u8);
					break;
				case 2:
					// summer in
					summer.apply(message.value.u8);
					break;
				case 3:
					// windows in
					Terminal::out << "window " << dec(info.sourceIndex) << (message.value.u8 ? " open\n" : " closed\n");
					if (message.value.u8 == 0)
						windows &= ~(1 << info.sourceIndex);
					else
						windows |= 1 << info.sourceIndex;
					break;
				case 4:
					// set temperature in
					setTemperature.apply(message);
					Terminal::out << "set temperature " << flt(float(setTemperature) - 273.15f) + '\n';
					break;
				case 5:
					// measured temperature in
					temperature = message.value.f32;
					Terminal::out << "temperature " << flt(temperature - 273.15f) + '\n';
					break;
				}

				// check if on and all windows closed
				if (on == 1 && windows == 0) {
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
					// switch off because a window is open
					valve = 0;
				}

				// publish valve
				function->subscribers.publishSwitch(6, valve);
			}
		}
	}
};

static int findIndex(FunctionInterface::Type type) {
	return array::binaryLowerBound(typeInfos, [type](TypeInfo const &element) { return element.type < type; });
}

// find type info, need to check the type if it can be invalid
static TypeInfo const &findInfo(FunctionInterface::Type type) {
	int i = array::binaryLowerBound(typeInfos, [type](TypeInfo const &element) { return element.type < type; });
	return typeInfos[i];
}


FunctionInterface::FunctionInterface() {
	// load list of device ids
	int deviceCount = Storage::read(STORAGE_CONFIG, STORAGE_ID_FUNCTION, sizeof(this->functionIds), this->functionIds);

	// load devices
	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		uint8_t id = this->functionIds[i];

		// get size of stored data
		int size = Storage::size(STORAGE_CONFIG, STORAGE_ID_FUNCTION | id);
		if (size < sizeof(Data))
			continue;

		// allocate memory and read data
		auto data = reinterpret_cast<Data *>(malloc(size));
		Storage::read(STORAGE_CONFIG, STORAGE_ID_FUNCTION | id, size, data);

		// check id
		if (data->id != id) {
			free(data);
			continue;
		}

		// find type and check type and size
		auto &typeInfo = findInfo(data->type);
		if (typeInfo.type != data->type || typeInfo.size(data) != size) {
			free(data);
			continue;
		}

		// create function
		Function *function = new Function(this, data);
		/*
		switch (data->type) {
		case Type::SWITCH:
			function = new Switch(this, data);
			break;
		case Type::LIGHT:
			function = new Light(this, data);
			break;
		default:
			continue;
		}*/

		// start coroutine
		function->coroutine = typeInfo.start(function);//function->start();

		// device was correctly loaded
		this->functionIds[j] = id;
		++j;
	}
	this->functionCount = j;
}

FunctionInterface::~FunctionInterface() {
}

String FunctionInterface::getName() {
	return "Function";
}

void FunctionInterface::setCommissioning(bool enabled) {
}

Array<uint8_t const> FunctionInterface::getDeviceIds() {
	return {this->functionCount, this->functionIds};
}

String FunctionInterface::getName(uint8_t id) const {
	auto function = findFunction(id);
	if (function != nullptr)
		return String(function->data->name);
	return {};
}

void FunctionInterface::setName(uint8_t id, String name) {
	auto function = findFunction(id);
	if (function != nullptr) {
		assign(function->data->name, name);

		// write to flash
		auto &typeInfo = findInfo(function->data->type);
		Storage::write(STORAGE_CONFIG, STORAGE_ID_FUNCTION | function->data->id, typeInfo.size(function->data), function->data);
	}
}

Array<MessageType const> FunctionInterface::getPlugs(uint8_t id) const {
	auto function = findFunction(id);
	if (function != nullptr) {
		auto &typeInfo = findInfo(function->data->type);
		return {int(typeInfo.plugs.size()), typeInfo.plugs.begin()};
	}
	return {};
}

void FunctionInterface::subscribe(Subscriber &subscriber) {
	assert(subscriber.info.barrier != nullptr);
	subscriber.remove();
	auto id = subscriber.data->source.deviceId;
	auto function = findFunction(id);
	if (function != nullptr)
		function->subscribers.add(subscriber);
}

SubscriberInfo FunctionInterface::getSubscriberInfo(uint8_t id, uint8_t plugIndex) {
	auto function = findFunction(id);
	if (function != nullptr) {
		auto &typeInfo = findInfo(function->data->type);
		if (plugIndex < typeInfo.plugs.size())
			return {typeInfo.plugs.begin()[plugIndex], &function->publishBarrier};
	}
	return {};
}

void FunctionInterface::erase(uint8_t id) {
	auto p = &this->functions;
	while (*p != nullptr) {
		auto function = *p;
		if (function->data->id == id) {
			// remove function from linked list
			*p = function->next;

			// erase from flash
			Storage::erase(STORAGE_CONFIG, STORAGE_ID_FUNCTION | id);

			// delete function
			delete function;

			goto list;
		}
		p = &function->next;
	}

list:

	// erase from list of device id's
	this->functionCount = eraseId(this->functionCount, this->functionIds, id);
	Storage::write(STORAGE_CONFIG, STORAGE_ID_FUNCTION, this->functionCount, this->functionIds);
}


void FunctionInterface::getData(uint8_t id, DataUnion &data) const {
	auto function = findFunction(id);
	if (function != nullptr) {
		auto &typeInfo = findInfo(function->data->type);
		array::copy(typeInfo.size(function->data), reinterpret_cast<uint8_t *>(&data), reinterpret_cast<uint8_t *>(function->data));
	}
}

void FunctionInterface::set(DataUnion &data) {
	auto id = data.data.id;

	// get type info (type must be valid)
	auto typeInfo = findInfo(data.data.type);

	// copy data to newly allocated data
	int size = typeInfo.size(&data.data);
	auto newData = reinterpret_cast<Data *>(malloc(size));
	array::copy(size, reinterpret_cast<uint8_t *>(newData), reinterpret_cast<uint8_t *>(&data));

	// try to find function
	auto function = findFunction(id);
	if (function == nullptr) {
		// new function: allocate a new id
		newData->id = allocateId();

		// add to list of functions
		this->functionIds[this->functionCount++] = newData->id;
		Storage::write(STORAGE_CONFIG, STORAGE_ID_FUNCTION, this->functionCount, this->functionIds);

		function = new Function(this, newData);
	} else {
		// function already exists: destroy old coroutine
		function->coroutine.destroy();

		// free old data and set new data
		free(function->data);
		function->data = newData;
	}

	// store data to flash
	Storage::write(STORAGE_CONFIG, STORAGE_ID_FUNCTION | newData->id, size, newData);

	// start coroutine
	function->coroutine = typeInfo.start(function);
}

String FunctionInterface::getName(Type type) {
	return findInfo(type).name;
}

FunctionInterface::Type FunctionInterface::getNextType(Type type, int delta) {
	int i = findIndex(type);
	i = (i + array::count(typeInfos) * 256 + delta) % array::count(typeInfos);
	return typeInfos[i].type;
}

void FunctionInterface::setType(DataUnion &data, Type type) {
	array::fill(sizeof(DataUnion) - sizeof(Data), reinterpret_cast<uint8_t *>(&data) + sizeof(Data), 0);

	data.data.type = type;

	// set default config
	switch (type) {
	case Type::SWITCH:
		break;
	case Type::LIGHT:
		data.lightData.settingCount = 4;
		data.lightData.settings[0] = {100, 10};
		data.lightData.settings[1] = {75, 10};
		data.lightData.settings[2] = {50, 10};
		data.lightData.settings[3] = {25, 10};
		break;
	case Type::COLOR_LIGHT:
		data.colorLightData.settingCount = 4;
		data.colorLightData.settings[0] = {100, 0, 100, 10};
		data.colorLightData.settings[1] = {100, 18, 100, 10};
		data.colorLightData.settings[2] = {100, 36, 100, 10};
		data.colorLightData.settings[3] = {100, 54, 100, 10};
		break;
	case Type::ANIMATED_LIGHT:
		data.animatedLightData.stepCount = 4;
		data.animatedLightData.steps[0] = {100, 0, 100, 10};
		data.animatedLightData.steps[1] = {100, 18, 100, 10};
		data.animatedLightData.steps[2] = {100, 36, 100, 10};
		data.animatedLightData.steps[3] = {100, 54, 100, 10};
		break;
	case Type::TIMED_BLIND:
		data.timedBlindData.holdTime = 200;
		data.timedBlindData.runTime = 1000;
		break;
	default:;
	}
}

Array<MessageType const> FunctionInterface::getPlugs(Type type) {
	auto &typeInfo = findInfo(type);
	return {int(typeInfo.plugs.size()), typeInfo.plugs.begin()};
}


// protected:

FunctionInterface::Function::~Function() {
	this->coroutine.destroy();
	free(this->data);
}

FunctionInterface::Function *FunctionInterface::findFunction(uint8_t id) const {
	auto function = this->functions;
	while (function != nullptr) {
		if (function->data->id == id)
			return function;
		function = function->next;
	}
	return nullptr;
}

uint8_t FunctionInterface::allocateId() {
	// find a free id
	int id;
	for (id = 1; id < 256; ++id) {
		auto function = this->functions;
		while (function != nullptr) {
			if (function->data->id == id)
				goto found;
			function = function->next;
		}
		break;
		found:;
	}
	return id;
}

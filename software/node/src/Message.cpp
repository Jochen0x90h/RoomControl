#include "Message.hpp"

/*
bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage) {
	Message &dst = *reinterpret_cast<Message *>(dstMessage);
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (dstType) {
		case MessageType::UNKNOWN:
			return false;
		case MessageType::ON_OFF:
			switch (srcType) {
				case MessageType::ON_OFF:
					dst.onOff = src.onOff;
					break;
				case MessageType::ON_OFF2:
					// invert on/off (0, 1, 2 -> 1, 0, 2)
					dst.onOff = src.onOff ^ 1 ^ (src.onOff >> 1);
					break;
				case MessageType::TRIGGER:
					// trigger (button) toggles on/off
					if (src.trigger == 0)
						return false; // conversion failed
					dst.onOff = 2;
					break;
				case MessageType::UP_DOWN:
					// up switches off, down switches on (1, 2 -> 0, 1)
					if (src.upDown == 0)
						return false; // conversion failed
					dst.onOff = src.upDown - 1;
					break;
				default:
					return false;
			}
			break;
		case MessageType::ON_OFF2:
			switch (srcType) {
				case MessageType::ON_OFF:
					// invert on/off (0, 1, 2 -> 1, 0, 2)
					dst.onOff = src.onOff ^ 1 ^ (src.onOff >> 1);
					break;
				case MessageType::ON_OFF2:
					dst.onOff = src.onOff;
					break;
				case MessageType::TRIGGER:
					// use trigger (button) state as switch state
					dst.onOff = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// up switches on, down switches off (1, 2 -> 1, 0)
					if (src.upDown == 0)
						return false; // conversion failed
					dst.onOff = 2 - src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TRIGGER:
			switch (srcType) {
				case MessageType::TRIGGER:
					dst.trigger = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// use up as press (0, 1 -> 0, 1)
					if (src.upDown == 2)
						return false; // conversion failed
					dst.trigger = src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::TRIGGER2:
			switch (srcType) {
				case MessageType::TRIGGER:
					dst.trigger = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// use down as press (0, 2 -> 0, 1)
					if (src.upDown == 1)
						return false; // conversion failed
					dst.trigger = src.upDown >> 1;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::UP_DOWN:
			switch (srcType) {
				case MessageType::TRIGGER:
					// use press as up (0, 1 -> 0, 1)
					dst.upDown = src.trigger;
					break;
				case MessageType::UP_DOWN:
					dst.trigger = src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::UP_DOWN2:
			switch (srcType) {
				case MessageType::TRIGGER:
					// use press as down (0, 1 -> 0, 2)
					dst.upDown = src.trigger << 1;
					break;
				case MessageType::UP_DOWN:
					// exchange up and down (0, 1, 2 -> 0, 2, 1)
					dst.upDown = (src.upDown << 1) | (src.upDown >> 1);
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::LEVEL:
			switch (srcType) {
				case MessageType::LEVEL:
					dst.level = src.level;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TEMPERATURE:
			switch (srcType) {
				case MessageType::TEMPERATURE:
					dst.temperature = src.temperature;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
	/ *
		case MessageType::CELSIUS:
			switch (srcType) {
			case MessageType::CELSIUS:
				dst.temperature = src.temperature;
				break;
			case MessageType::FAHRENHEIT:
				// fahrenheit -> celsius
				dst.temperature.set((src.temperature - 32.0f) * 5.0 / 9.0, src.temperature.getFlag());
				break;
			default:
				// conversion failed
				return false;
			}
			break;
		case MessageType::FAHRENHEIT:
			switch (srcType) {
			case MessageType::CELSIUS:
				dst.temperature = src.temperature;
				break;
			case MessageType::FAHRENHEIT:
				// celsius -> fahrenheit
				dst.temperature.set(src.temperature * 9.0f / 5.0f + 32.0f, src.temperature.getFlag());
				break;
			default:
				// conversion failed
				return false;
			}
			break;
	* /
		case MessageType::PRESSURE:
			switch (srcType) {
				case MessageType::PRESSURE:
					dst.pressure = src.pressure;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::AIR_HUMIDITY:
			switch (srcType) {
				case MessageType::AIR_HUMIDITY:
					dst.airHumidity = src.airHumidity;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::AIR_VOC:
			switch (srcType) {
				case MessageType::AIR_VOC:
					dst.airVoc = src.airVoc;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
	}

	// conversion successful
	return true;
}
*/

bool isCompatibleIn(MessageType dstType, MessageType srcType) {
	switch (dstType) {
		case MessageType::OFF_ON_IN:
		case MessageType::OFF_ON_TOGGLE_IN:
		case MessageType::TRIGGER_IN:
		case MessageType::UP_DOWN_IN:
		case MessageType::OPEN_CLOSE_IN:
			switch (srcType) {
				case MessageType::OFF_ON_IN:
				case MessageType::OFF_ON_TOGGLE_IN:
				case MessageType::TRIGGER_IN:
				case MessageType::UP_DOWN_IN:
				case MessageType::OPEN_CLOSE_IN:
					return true;
				default:
					return false;
			}
		case MessageType::SET_TEMPERATURE_IN:
			switch (srcType) {
				case MessageType::TRIGGER_IN:
				case MessageType::UP_DOWN_IN:
				case MessageType::OPEN_CLOSE_IN:
				case MessageType::SET_TEMPERATURE_IN:
					return true;
				default:
					return false;
			}
		default:
			return false;
	}
}

bool convertCommandIn(MessageType dstType, Message &dst, MessageType srcType, uint8_t src,
	ConvertOptions const &convertOptions)
{
	if (src >= 4)
		return false;

	switch (dstType) {
		case MessageType::OFF_ON_IN:
		case MessageType::OFF_ON_TOGGLE_IN:
		case MessageType::TRIGGER_IN:
		case MessageType::UP_DOWN_IN:
		case MessageType::OPEN_CLOSE_IN: {
			int c = (convertOptions.i32 >> src * 3) & 7;
			if (c >= 4)
				return false; // conversion failed
			dst.indexedCommand.command = c;
			int i = convertOptions.u32 >> 24;
			if (i != 255)
				dst.indexedCommand.index = i;
			break;
		}
		case MessageType::SET_TEMPERATURE_IN: {
			switch (srcType) {
				case MessageType::TRIGGER_IN:
					if (src == 0)
						return false; // conversion failed
					dst.setTemperature = convertOptions.ff;
					break;
				case MessageType::UP_DOWN_IN:
				case MessageType::OPEN_CLOSE_IN:
					if (src == 0)
						return false; // conversion failed
					if (!convertOptions.ff.getFlag()) {
						// absolute
						if (!convertOptions.ff.isNegative()) {
							// positive: Set on up/trigger
							if (src == 2)
								return false; // conversion failed
							dst.setTemperature = convertOptions.ff;
						} else {
							// negative: Set on down/trigger
							if (src == 1)
								return false; // conversion failed
							dst.setTemperature = -convertOptions.ff;
						}
					} else {
						// relative: Up/trigger are positive, down is negative
						dst.setTemperature = src == 2 ? -convertOptions.ff : convertOptions.ff;
					}
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertTemperatureIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions) {
	switch (dstType) {
		case MessageType::TEMPERATURE_IN:
			dst.temperature = src;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertPressureIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions) {
	switch (dstType) {
		case MessageType::PRESSURE_IN:
			dst.pressure = src;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertAirHumidityIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions) {
	switch (dstType) {
		case MessageType::AIR_HUMIDITY_IN:
			dst.airHumidity = src;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertAirVocIn(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions) {
	switch (dstType) {
		case MessageType::AIR_VOC_IN:
			dst.airVoc = src;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool isCompatibleOut(MessageType dstType, MessageType srcType) {
	switch (srcType) {
		case MessageType::OFF_ON_OUT:
		case MessageType::OFF_ON_TOGGLE_OUT:
		case MessageType::TRIGGER_OUT:
		case MessageType::UP_DOWN_OUT:
		case MessageType::OPEN_CLOSE_OUT:
			switch (dstType) {
				case MessageType::OFF_ON_OUT:
				case MessageType::OFF_ON_TOGGLE_OUT:
				case MessageType::TRIGGER_OUT:
				case MessageType::UP_DOWN_OUT:
				case MessageType::OPEN_CLOSE_OUT:
					return true;
				default:
					return false;
			}
		default:
			return false;
	}
}

bool convertCommandOut(uint8_t &dst, MessageType srcType, Message const &src, ConvertOptions const &convertOptions) {
	switch (srcType) {
		case MessageType::OFF_ON_OUT:
		case MessageType::OFF_ON_TOGGLE_OUT:
		case MessageType::TRIGGER_OUT:
		case MessageType::UP_DOWN_OUT:
		case MessageType::OPEN_CLOSE_OUT: {
			int c = (convertOptions.i32 >> int(src.command) * 3) & 7;
			if (c >= 4)
				return false; // conversion failed
			dst = c;
			break;
		}
		default:
			// todo: convert any event to on/off using convertOptions
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

String getTypeName(MessageType messageType) {
	switch (messageType & MessageType::TYPE_MASK) {
		case MessageType::OFF_ON:
			return "Off/On";
		case MessageType::OFF_ON_TOGGLE:
			return "Off/On/Toggle";
		case MessageType::TRIGGER:
			return "Trigger";
		case MessageType::UP_DOWN:
			return "Up/Down";
		case MessageType::OPEN_CLOSE:
			return "Open/Close";

		case MessageType::LEVEL:
			return "Level";
		case MessageType::MOVE_TO_LEVEL:
			return "Move to Level";

		case MessageType::TEMPERATURE:
			return "Temperature";
		case MessageType::PRESSURE:
			return "Pressure";
		case MessageType::AIR_HUMIDITY:
			return "Air Humidity";
		case MessageType::AIR_VOC:
			return "Air VOC";
		case MessageType::ILLUMINANCE:
			return "Illuminance";

		case MessageType::VOLTAGE:
			return "Voltage";
		case MessageType::CURRENT:
			return "Current";
		case MessageType::BATTERY_LEVEL:
			return "Battery Level";
		case MessageType::ENERGY_COUNTER:
			return "Energy Counter";
		case MessageType::POWER:
			return "Power";

		default:
			return "<unknown>";
	}
}

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

bool isCompatible(MessageType dstType, MessageType srcType) {
	// same type is compatible with IN and OUT exchanged
	if (dstType == (srcType ^ (MessageType::OUT ^ MessageType::IN)))
		return true;

	// all switch commands are compatible
	bool dstIsCommand = uint(dstType) - uint(MessageType::OFF_ON_IN) < SWITCH_COMMAND_COUNT;
	bool srcIsCommand = uint(srcType) - uint(MessageType::OFF_ON_OUT) < SWITCH_COMMAND_COUNT;
	if (dstIsCommand && srcIsCommand)
		return true;

	// set command is compatible to move-to command
	if (dstType == MessageType::SET_LEVEL_IN && srcType == MessageType::MOVE_TO_LEVEL_OUT)
		return true;

	// switch command is compatible to set command (uses values in the convert options)
	if (srcIsCommand) {
		switch (dstType) {
		case MessageType::SET_LEVEL_IN:
		case MessageType::MOVE_TO_LEVEL_IN:
		case MessageType::SET_AIR_TEMPERATURE_IN:
			return true;
		default:;
		}
	}

	// simple value is compatible to switch command (compares against values in the convert options)
	if (dstIsCommand) {
		switch (srcType) {
		case MessageType::LEVEL_OUT:
		case MessageType::AIR_TEMPERATURE_OUT:
		case MessageType::AIR_HUMIDITY_OUT:
		case MessageType::AIR_PRESSURE_OUT:
		case MessageType::AIR_VOC_OUT:
			return true;
		default:;
		}
	}

	return false;
}

bool convertCommand(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src >= 3)
		return false;

	switch (dstType) {
	case MessageType::OFF_ON_IN:
	case MessageType::OFF_ON_TOGGLE_IN:
	case MessageType::TRIGGER_IN:
	case MessageType::UP_DOWN_IN:
	case MessageType::OPEN_CLOSE_IN: {
		// command -> command
		int c = convertOptions.getCommand(src);
		if (c >= 3)
			return false; // conversion failed
		dst.command = c;
		break;
	}
	case MessageType::SET_LEVEL_IN:
	case MessageType::MOVE_TO_LEVEL_IN:
	case MessageType::SET_AIR_TEMPERATURE_IN:
	case MessageType::SET_AIR_HUMIDITY_IN: {
		// command -> set-value, use values in convertOptions
		int c = convertOptions.getCommand(src);
		if (c >= 3)
			return false; // conversion failed
		dst.command = c;
		dst.transition = 0;
		dst.value.f = convertOptions.value.f[src];
		break;
	}
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloatValue(MessageType dstType, Message &dst, MessageType srcType, float src,
	ConvertOptions const &convertOptions)
{
	switch (dstType) {
	case MessageType::OFF_ON_IN:
	case MessageType::OFF_ON_TOGGLE_IN:
	case MessageType::TRIGGER_IN:
	case MessageType::UP_DOWN_IN:
	case MessageType::OPEN_CLOSE_IN: {
		// value -> command, compare against threshold values
		float upper = convertOptions.value.f[0];
		float lower = convertOptions.value.f[1];
		int compare = src > upper ? 0 : (src < lower ? 1 : 2);
		int c = convertOptions.getCommand(compare);
		if (c >= 3)
			return false; // conversion failed
		dst.command = c;
		break;
	}
	case MessageType::LEVEL_IN:
	case MessageType::AIR_TEMPERATURE_IN:
	case MessageType::AIR_HUMIDITY_IN:
	case MessageType::AIR_PRESSURE_IN:
	case MessageType::AIR_VOC_IN:
		if (dstType != (srcType ^ (MessageType::OUT | MessageType::IN)))
			return false; // conversion failed
		dst.value.f = src;
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertSetFloatValue(MessageType dstType, Message &dst, MessageType srcType, Message const &src,
	ConvertOptions const &convertOptions) {
	switch (dstType) {
	case MessageType::SET_LEVEL_IN:
	case MessageType::MOVE_TO_LEVEL_IN:
	case MessageType::SET_AIR_TEMPERATURE_IN:
		if (dstType == MessageType::MOVE_TO_LEVEL_IN && srcType == MessageType::SET_LEVEL_OUT) {
			dst.command = src.command;
			dst.transition = 0;
			dst.value.f = src.value.f;
		} else if (dstType != (srcType ^ (MessageType::OUT | MessageType::IN))) {
			return false; // conversion failed
		} else {
			dst.command = src.command;
			dst.transition = src.transition;
			dst.value.f = src.value.f;
		}
		break;
	default:
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
	case MessageType::SET_LEVEL:
		return "Set Level";
	case MessageType::MOVE_TO_LEVEL:
		return "Move to Level";

	case MessageType::AIR_TEMPERATURE:
		return "Air Temperature";
	case MessageType::SET_AIR_TEMPERATURE:
		return "Set Air Temperature";
	case MessageType::AIR_PRESSURE:
		return "Air Pressure";
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

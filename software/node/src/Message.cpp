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
/*
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
*/


using EndpointType = bus::EndpointType2;

String getTypeLabel(EndpointType type) {
	switch (type & EndpointType::CATEGORY) {
	case EndpointType::SWITCH:
		switch (type & EndpointType::SWITCH_CATEGORY) {
		case EndpointType::SWITCH_BINARY:
			switch (type & EndpointType::SWITCH_BINARY_CATEGORY) {
			case EndpointType::SWITCH_BINARY_ONESHOT:
				switch (type & EndpointType::SWITCH_BINARY_ONESHOT_CATEGORY) {
				case EndpointType::SWITCH_BINARY_ONESHOT_BUTTON:
					return "Button";
				default:
					return "One-Shot";
				}
			case EndpointType::SWITCH_BINARY_SWITCH:
				return "Switch";
			case EndpointType::SWITCH_BINARY_ON_OFF:
				return "On/Off";
			case EndpointType::SWITCH_BINARY_START_STOP:
				return "Start/Stop";
			case EndpointType::SWITCH_BINARY_VALVE:
				switch (type & EndpointType::SWITCH_BINARY_VALVE_CATEGORY) {
				case EndpointType::SWITCH_BINARY_VALVE_NO:
					return "Valve NO";
				case EndpointType::SWITCH_BINARY_VALVE_NC:
					return "Valve NC";
				default:
					return "Valve";
				}
			case EndpointType::SWITCH_BINARY_APERTURE:
				switch (type & EndpointType::SWITCH_BINARY_APERTURE_CATEGORY) {
				case EndpointType::SWITCH_BINARY_APERTURE_DOOR:
					return "Door";
				case EndpointType::SWITCH_BINARY_APERTURE_WINDOW:
					return "Window";
				default:
					return "Aperture";
				}
			case EndpointType::SWITCH_BINARY_OCCUPANCY:
				return "Occupancy";
			case EndpointType::SWITCH_BINARY_ENABLE_CLOSE:
				return "Enable Close";
			default:
				return "Binary";
			}
		case EndpointType::SWITCH_TERNARY:
			switch (type & EndpointType::SWITCH_TERNARY_CATEGORY) {
			case EndpointType::SWITCH_TERNARY_ONESHOT:
				switch (type & EndpointType::SWITCH_TERNARY_ONESHOT_CATEGORY) {
				case EndpointType::SWITCH_TERNARY_ONESHOT_ROCKER:
					return "Rocker";
				default:
					return "One-Shot";
				}
			case EndpointType::SWITCH_TERNARY_SWITCH:
				return "Switch";
			case EndpointType::SWITCH_TERNARY_OPEN_CLOSE:
				return "Open/Close";
			default:
				return "Ternary";
			}
		default:
			return "Switch";
		}
	case EndpointType::MULTISTATE:
		switch (type & EndpointType::MULTISTATE_CATEGORY) {
		case EndpointType::MULTISTATE_THERMOSTAT_MODE:
			return "Mode";
		default:
			return "Multistate";
		}
	case EndpointType::LEVEL:
		switch (type & EndpointType::LEVEL_CATEGORY) {
		case EndpointType::LEVEL_APERTURE:
			switch (type & EndpointType::LEVEL_APERTURE_CATEGORY) {
			case EndpointType::LEVEL_APERTURE_GATE:
				return "Gate Position";
			case EndpointType::LEVEL_APERTURE_BLIND:
				return "Blind Position";
			case EndpointType::LEVEL_APERTURE_SLAT:
				return "Slat Position";
			default:
				return "Aperture";
			}
		case EndpointType::LEVEL_BATTERY:
			return "Battery Level";
		case EndpointType::LEVEL_TANK:
			return "Tank Level";
		default:
			return "Level";
		}
	case EndpointType::PHYSICAL:
		switch (type & EndpointType::PHYSICAL_CATEGORY) {
		case EndpointType::PHYSICAL_TEMPERATURE:
			switch (type & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case EndpointType::PHYSICAL_TEMPERATURE_MEASURED:
				switch (type & EndpointType::PHYSICAL_TEMPERATURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FREEZER:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FRIDGE:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OVEN:
					return "Measured Temp.";
				default:
					return "Measured Temp.";
				}
			case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT:
				switch (type & EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FREEZER:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FRIDGE:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_COOLER:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER:
					return "Measured Temp.";
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_OVEN:
					return "Measured Temp.";
				default:
					return "Setpoint Temp.";
				}
			default:
				return "Temperature";
			}
		case EndpointType::PHYSICAL_PRESSURE:
			switch (type & EndpointType::PHYSICAL_PRESSURE_CATEGORY) {
			case EndpointType::PHYSICAL_PRESSURE_MEASURED:
				switch (type & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return "Measured Pressure";
				default:
					return "Measured Pressure";
				}
			case EndpointType::PHYSICAL_PRESSURE_SETPOINT:
				return "Pressure Setpoint";
			default:
				return "Pressure";
			}
		case EndpointType::PHYSICAL_VOLTAGE:
			switch (type & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) {
			case EndpointType::PHYSICAL_VOLTAGE_MEASURED:
				switch (type & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return "Measured Voltage";
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return "Measured Voltage";
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return "Measured Voltage";
				default:
					return "Measured Voltage";
				}
			case EndpointType::PHYSICAL_VOLTAGE_SETPOINT:
				return "Voltage Setpoint";
			default:
				return "Voltage";
			}
		case EndpointType::PHYSICAL_CURRENT:
			switch (type & EndpointType::PHYSICAL_CURRENT_CATEGORY) {
			case EndpointType::PHYSICAL_CURRENT_MEASURED:
				return "Measured Curent";
			case EndpointType::PHYSICAL_CURRENT_SETPOINT:
				return "Curent Setpoint";
			default:
				return "Current";
			}
		case EndpointType::PHYSICAL_POWER:
			switch (type & EndpointType::PHYSICAL_POWER_CATEGORY) {
			case EndpointType::PHYSICAL_POWER_MEASURED:
				return "Measured Power";
			case EndpointType::PHYSICAL_POWER_SETPOINT:
				return "Power Setpoint";
			default:
				return "Current";
			}
		case EndpointType::PHYSICAL_ILLUMINATION:
			return "Illuminatin";
		default:
			return "Physical";
		}
	case EndpointType::CONCENTRATION:
		switch (type & EndpointType::CONCENTRATION_CATEGORY) {
		case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY:
			switch (type & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) {
			case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED:
				switch (type & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) {
				case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR:
					return "Measured Humidity";
				default:
					return "Measured Humidity";
				}
			case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT:
				return "Setpoint Temp.";
			default:
				return "Humidity";
			}
		case EndpointType::CONCENTRATION_VOC:
			return "Volatile Organic";
		case EndpointType::CONCENTRATION_CARBON_MONOXIDE:
			return "Carbon Monox.";
		case EndpointType::CONCENTRATION_CARBON_DIOXIDE:
			return "Carbon Diox.";
		default:
			return "Concentration";
		}
	case EndpointType::LIGHTING:
		switch (type & EndpointType::LIGHTING_CATEGORY) {
		case EndpointType::LIGHTING_BRIGHTNESS:
			return "Brightness";
		case EndpointType::LIGHTING_COLOR_TEMPERATURE:
			return "Color Temp.";
		case EndpointType::LIGHTING_COLOR_PARAMETER:
			switch (type & EndpointType::LIGHTING_COLOR_PARAMETER_CATEGORY) {
			case EndpointType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X:
				return "Chromaticity x";
			case EndpointType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y:
				return "Chromaticity y";
			case EndpointType::LIGHTING_COLOR_PARAMETER_HUE:
				return "Hue";
			case EndpointType::LIGHTING_COLOR_PARAMETER_SATURATION:
				return "Saturation";
			default:
				return "Color Parameter";
			}
		default:
			return "Lighting";
		}
	case EndpointType::METERING:
		switch (type & EndpointType::METERING_CATEGORY) {
		case EndpointType::METERING_ELECTRIC:
			switch (type & EndpointType::METERING_ELECTRIC_CATEGORY) {
			case EndpointType::METERING_ELECTRIC_USAGE:
				switch (type & EndpointType::METERING_ELECTRIC_USAGE_CATEGORY) {
				case EndpointType::METERING_ELECTRIC_USAGE_PEAK:
					return "Peak Usage";
				case EndpointType::METERING_ELECTRIC_USAGE_OFF_PEAK:
					return "Off-Peak Usage";
				default:
					return "Energy Usage";
				}
			case EndpointType::METERING_ELECTRIC_SUPPLY:
				switch (type & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) {
				case EndpointType::METERING_ELECTRIC_SUPPLY_PEAK:
					return "Peak Supply";
				case EndpointType::METERING_ELECTRIC_SUPPLY_OFF_PEAK:
					return "Off-Peak Supply";
				default:
					return "Supply";
				}
			default:
				return "Electric Meter";
			}
		case EndpointType::METERING_WATER:
			return "Water Meter";
		case EndpointType::METERING_GAS:
			return "Gas Meter";
		default:
			return "Metering";
		}
	default:
		return {};
	}
}

Usage getUsage(MessageType2 type) {
	auto t = type & EndpointType::TYPE_MASK;

	switch (t & EndpointType::CATEGORY) {
	case EndpointType::SWITCH:
		switch (t & EndpointType::SWITCH_CATEGORY) {
		case EndpointType::SWITCH_BINARY:
			if (t == EndpointType::SWITCH_BINARY) return Usage::OFF_ON;
			switch (t & EndpointType::SWITCH_BINARY_CATEGORY) {
			case EndpointType::SWITCH_BINARY_ONESHOT:
				switch (t & EndpointType::SWITCH_BINARY_ONESHOT_CATEGORY) {
				case EndpointType::SWITCH_BINARY_ONESHOT_BUTTON:
					return Usage::RELEASE_PRESS;
				case EndpointType::SWITCH_BINARY_ONESHOT_ALARM:
					return Usage::ACTIVATE;
				default:
					return Usage::NONE;
				}
			case EndpointType::SWITCH_BINARY_SWITCH:
				return Usage::OFF_ON;
			case EndpointType::SWITCH_BINARY_ON_OFF:
				return Usage::OFF_ON;
			case EndpointType::SWITCH_BINARY_START_STOP:
				return Usage::STOP_START;
			case EndpointType::SWITCH_BINARY_VALVE:
				return Usage::NONE;
			case EndpointType::SWITCH_BINARY_APERTURE:
				return Usage::OPEN_CLOSE;
			case EndpointType::SWITCH_BINARY_OCCUPANCY:
				return Usage::OCCUPANCY;
			case EndpointType::SWITCH_BINARY_ENABLE_CLOSE:
				return Usage::ENABLE;
			default:
				return Usage::NONE;
			}
		case EndpointType::SWITCH_TERNARY:
			switch (t & EndpointType::SWITCH_TERNARY_CATEGORY) {
			case EndpointType::SWITCH_TERNARY_ONESHOT:
				switch (t & EndpointType::SWITCH_TERNARY_ONESHOT_CATEGORY) {
				case EndpointType::SWITCH_TERNARY_ONESHOT_ROCKER:
					return Usage::UP_DOWN;
				default:
					return Usage::NONE;
				}
			case EndpointType::SWITCH_TERNARY_SWITCH:
				return Usage::OFF_ON2;
			case EndpointType::SWITCH_TERNARY_OPEN_CLOSE:
				return Usage::STOP_OPEN_CLOSE;
			default:
				return Usage::NONE;
			}
		default:
			return Usage::NONE;
		}
	case EndpointType::MULTISTATE:
		return Usage::NONE;
	case EndpointType::LEVEL:
		return Usage::PERCENTAGE;
	case EndpointType::PHYSICAL:
		switch (t & EndpointType::PHYSICAL_CATEGORY) {
		case EndpointType::PHYSICAL_TEMPERATURE:
			if (t == EndpointType::PHYSICAL_TEMPERATURE) return Usage::KELVIN;
			switch (t & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case EndpointType::PHYSICAL_TEMPERATURE_MEASURED:
				switch (t & EndpointType::PHYSICAL_TEMPERATURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FREEZER:
					return Usage::FEEZER_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FRIDGE:
					return Usage::FRIDGE_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR:
					return Usage::OUTDOOR_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM:
					return Usage::ROOM_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OVEN:
					return Usage::OVEN_TEMPERATURE;
				default:
					return Usage::NONE;
				}
			case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT:
				switch (t & EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FREEZER:
					return Usage::FEEZER_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FRIDGE:
					return Usage::FRIDGE_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_COOLER:
					return Usage::ROOM_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER:
					return Usage::ROOM_TEMPERATURE;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_OVEN:
					return Usage::OVEN_TEMPERATURE;
				default:
					return Usage::NONE;
				}
			default:
				return Usage::NONE;
			}
		case EndpointType::PHYSICAL_PRESSURE:
			if (t == EndpointType::PHYSICAL_PRESSURE) return Usage::PASCAL;
			switch (t & EndpointType::PHYSICAL_PRESSURE_CATEGORY) {
			case EndpointType::PHYSICAL_PRESSURE_MEASURED:
				switch (t & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return Usage::ATMOSPHERIC_PRESSURE;
				default:
					return Usage::NONE;
				}
			case EndpointType::PHYSICAL_PRESSURE_SETPOINT:
				return Usage::NONE;
			default:
				return Usage::NONE;
			}
		case EndpointType::PHYSICAL_VOLTAGE:
			if (t == EndpointType::PHYSICAL_VOLTAGE) return Usage::VOLT;
			switch (t & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) {
			case EndpointType::PHYSICAL_VOLTAGE_MEASURED:
				switch (t & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return Usage::LOW_VOLTAGE;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return Usage::MAINS_VOLTAGE;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return Usage::HIGH_VOLTAGE;
				default:
					return Usage::NONE;
				}
			case EndpointType::PHYSICAL_VOLTAGE_SETPOINT:
				return Usage::NONE;
			default:
				return Usage::NONE;
			}
		case EndpointType::PHYSICAL_CURRENT:
			return Usage::AMPERE;
		case EndpointType::PHYSICAL_POWER:
			return Usage::WATT;
		case EndpointType::PHYSICAL_ILLUMINATION:
			return Usage::NONE;
		default:
			return Usage::NONE;
		}
	case EndpointType::CONCENTRATION:
		switch (t & EndpointType::CONCENTRATION_CATEGORY) {
		case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY:
			return Usage::PERCENT;
		case EndpointType::CONCENTRATION_VOC:
			return Usage::NONE;
		case EndpointType::CONCENTRATION_CARBON_MONOXIDE:
			return Usage::NONE;
		case EndpointType::CONCENTRATION_CARBON_DIOXIDE:
			return Usage::NONE;
		default:
			return Usage::NONE;
		}
	case EndpointType::LIGHTING:
		switch (t & EndpointType::LIGHTING_CATEGORY) {
		case EndpointType::LIGHTING_BRIGHTNESS:
			return Usage::PERCENTAGE;
		case EndpointType::LIGHTING_COLOR_TEMPERATURE:
			return Usage::COLOR_TEMPERATURE;
		case EndpointType::LIGHTING_COLOR_PARAMETER:
			return Usage::UNIT_INTERVAL;
		default:
			return Usage::NONE;
		}
	case EndpointType::METERING:
		switch (t & EndpointType::METERING_CATEGORY) {
		case EndpointType::METERING_ELECTRIC:
			return Usage::WATT_HOURS;
		case EndpointType::METERING_WATER:
			return Usage::NONE;
		case EndpointType::METERING_GAS:
			return Usage::NONE;
		default:
			return Usage::NONE;
		}
	default:
		return Usage::NONE;
	}
}

bool isCompatible(EndpointType dstType, EndpointType srcType) {
	bool srcCmd = (srcType & EndpointType::CMD) != 0;
	if (srcCmd && (dstType & EndpointType::CMD) == 0)
		return false;
	if ((srcType & EndpointType::DIRECTION_MASK) != EndpointType::OUT || (dstType & EndpointType::DIRECTION_MASK) != EndpointType::IN)
		return false;
	auto src = srcType & ~(EndpointType::DIRECTION_MASK | EndpointType::CMD);
	auto dst = dstType & ~(EndpointType::DIRECTION_MASK | EndpointType::CMD);

	if ((src & EndpointType::CATEGORY) == dst) return true;
	switch (dst & EndpointType::CATEGORY) {
	case EndpointType::SWITCH:
		if (!srcCmd) {
			switch (src & EndpointType::CATEGORY) {
			case EndpointType::LEVEL:
				return true;
			case EndpointType::PHYSICAL:
				return true;
			case EndpointType::CONCENTRATION:
				return true;
			case EndpointType::LIGHTING:
				return true;
			default:;
			}
		}
		return (src & EndpointType::CATEGORY) == EndpointType::SWITCH;
	case EndpointType::MULTISTATE:
		if ((src & EndpointType::MULTISTATE_CATEGORY) == dst) return true;
		switch (dst & EndpointType::MULTISTATE_CATEGORY) {
		case EndpointType::MULTISTATE_THERMOSTAT_MODE:
			return src == EndpointType::MULTISTATE_THERMOSTAT_MODE;
		default:
			return false;
		}
	case EndpointType::LEVEL:
		if ((src & EndpointType::LEVEL_CATEGORY) == dst) return true;
		switch (dst & EndpointType::LEVEL_CATEGORY) {
		case EndpointType::LEVEL_APERTURE:
			if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
			return (src & EndpointType::LEVEL_CATEGORY) == EndpointType::LEVEL_APERTURE;
		case EndpointType::LEVEL_BATTERY:
			return src == EndpointType::LEVEL_BATTERY;
		case EndpointType::LEVEL_TANK:
			return src == EndpointType::LEVEL_TANK;
		default:
			return false;
		}
	case EndpointType::PHYSICAL:
		if ((src & EndpointType::PHYSICAL_CATEGORY) == dst) return true;
		switch (dst & EndpointType::PHYSICAL_CATEGORY) {
		case EndpointType::PHYSICAL_TEMPERATURE:
			if ((src & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case EndpointType::PHYSICAL_TEMPERATURE_MEASURED:
				return (src & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) == EndpointType::PHYSICAL_TEMPERATURE_MEASURED;
			case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return (src & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) == EndpointType::PHYSICAL_TEMPERATURE_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_PRESSURE:
			if ((src & EndpointType::PHYSICAL_PRESSURE_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_PRESSURE_CATEGORY) {
			case EndpointType::PHYSICAL_PRESSURE_MEASURED:
				if ((src & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) == dst) return true;
				switch (dst & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return src == EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE;
				default:
					return false;
				}
			case EndpointType::PHYSICAL_PRESSURE_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return src == EndpointType::PHYSICAL_PRESSURE_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_VOLTAGE:
			if ((src & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) {
			case EndpointType::PHYSICAL_VOLTAGE_MEASURED:
				if ((src & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == dst) return true;
				switch (dst & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return src == EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return src == EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return src == EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH;
				default:
					return false;
				}
			case EndpointType::PHYSICAL_VOLTAGE_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return src == EndpointType::PHYSICAL_VOLTAGE_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_CURRENT:
			if ((src & EndpointType::PHYSICAL_CURRENT_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_CURRENT_CATEGORY) {
			case EndpointType::PHYSICAL_CURRENT_MEASURED:
				return src == EndpointType::PHYSICAL_CURRENT_MEASURED;
			case EndpointType::PHYSICAL_CURRENT_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return src == EndpointType::PHYSICAL_CURRENT_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_POWER:
			if ((src & EndpointType::PHYSICAL_POWER_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_POWER_CATEGORY) {
			case EndpointType::PHYSICAL_POWER_MEASURED:
				return src == EndpointType::PHYSICAL_POWER_MEASURED;
			case EndpointType::PHYSICAL_POWER_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return src == EndpointType::PHYSICAL_POWER_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_ILLUMINATION:
			return src == EndpointType::PHYSICAL_ILLUMINATION;
		default:
			return false;
		}
	case EndpointType::CONCENTRATION:
		if ((src & EndpointType::CONCENTRATION_CATEGORY) == dst) return true;
		switch (dst & EndpointType::CONCENTRATION_CATEGORY) {
		case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY:
			if ((src & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) == dst) return true;
			switch (dst & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) {
			case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED:
				if ((src & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) == dst) return true;
				switch (dst & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) {
				case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR:
					return src == EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR;
				default:
					return false;
				}
			case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT:
				if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
				return src == EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT;
			default:
				return false;
			}
		case EndpointType::CONCENTRATION_VOC:
			return src == EndpointType::CONCENTRATION_VOC;
		case EndpointType::CONCENTRATION_CARBON_MONOXIDE:
			return src == EndpointType::CONCENTRATION_CARBON_MONOXIDE;
		case EndpointType::CONCENTRATION_CARBON_DIOXIDE:
			return src == EndpointType::CONCENTRATION_CARBON_DIOXIDE;
		default:
			return false;
		}
	case EndpointType::LIGHTING:
		if ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;
		if ((src & EndpointType::LIGHTING_CATEGORY) == dst) return true;
		switch (dst & EndpointType::LIGHTING_CATEGORY) {
		case EndpointType::LIGHTING_BRIGHTNESS:
			return src == EndpointType::LIGHTING_BRIGHTNESS;
		case EndpointType::LIGHTING_COLOR_TEMPERATURE:
			return src == EndpointType::LIGHTING_COLOR_TEMPERATURE;
		case EndpointType::LIGHTING_COLOR_PARAMETER:
			return (src & EndpointType::LIGHTING_CATEGORY) == EndpointType::LIGHTING_COLOR_PARAMETER;
		default:
			return false;
		}
	case EndpointType::METERING:
		if ((src & EndpointType::METERING_CATEGORY) == dst) return true;
		switch (dst & EndpointType::METERING_CATEGORY) {
		case EndpointType::METERING_ELECTRIC:
			if ((src & EndpointType::METERING_ELECTRIC_CATEGORY) == dst) return true;
			switch (dst & EndpointType::METERING_ELECTRIC_CATEGORY) {
			case EndpointType::METERING_ELECTRIC_USAGE:
				if ((src & EndpointType::METERING_ELECTRIC_USAGE_CATEGORY) == dst) return true;
				switch (dst & EndpointType::METERING_ELECTRIC_USAGE_CATEGORY) {
				case EndpointType::METERING_ELECTRIC_USAGE_PEAK:
					return src == EndpointType::METERING_ELECTRIC_USAGE_PEAK;
				case EndpointType::METERING_ELECTRIC_USAGE_OFF_PEAK:
					return src == EndpointType::METERING_ELECTRIC_USAGE_OFF_PEAK;
				default:
					return false;
				}
			case EndpointType::METERING_ELECTRIC_SUPPLY:
				if ((src & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) == dst) return true;
				switch (dst & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) {
				case EndpointType::METERING_ELECTRIC_SUPPLY_PEAK:
					return src == EndpointType::METERING_ELECTRIC_SUPPLY_PEAK;
				case EndpointType::METERING_ELECTRIC_SUPPLY_OFF_PEAK:
					return src == EndpointType::METERING_ELECTRIC_SUPPLY_OFF_PEAK;
				default:
					return false;
				}
			default:
				return false;
			}
		case EndpointType::METERING_WATER:
			return src == EndpointType::METERING_WATER;
		case EndpointType::METERING_GAS:
			return src == EndpointType::METERING_GAS;
		default:
			return false;
		}
	default:
		return false;
	}
}


bool convertSwitch(MessageType2 dstType, Message2 &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src >= 3)
		return false;

	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::SWITCH: {
		// switch -> switch
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		dst.value.u8 = cmd;
		break;
	}
	case EndpointType::LEVEL:
	case EndpointType::PHYSICAL:
	case EndpointType::CONCENTRATION: {
		// switch -> float, use values in convertOptions
		dst.value.f = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & EndpointType::CMD) != 0) {
			dst.command = cmd;
		}
		break;
	}
	case EndpointType::LIGHTING: {
		// switch -> float, use values in convertOptions
		dst.value.f = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & EndpointType::CMD) != 0) {
			dst.command = cmd;
			dst.transition = convertOptions.transition;
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

bool convertFloat(MessageType2 dstType, Message2 &dst, float src, ConvertOptions const &convertOptions)
{
	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::SWITCH: {
		// float -> command, compare against threshold values
		float upper = convertOptions.value.f[0];
		float lower = convertOptions.value.f[1];
		int compare = src > upper ? 0 : (src < lower ? 1 : 2);
		int c = convertOptions.getCommand(compare);
		if (c >= 3)
			return false; // conversion failed
		dst.value.u8 = c;
		break;
	}
	case EndpointType::LEVEL:
	case EndpointType::PHYSICAL:
	case EndpointType::CONCENTRATION:
		dst.value.f = src;
		if ((dstType & EndpointType::CMD) != 0) {
			dst.command = 0; // set
		}
		break;
	case EndpointType::LIGHTING:
		dst.value.f = src;
		if ((dstType & EndpointType::CMD) != 0) {
			dst.command = 0; // set
			dst.transition = 0;
		}
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloatCommand(MessageType2 dstType, Message2 &dst, float src, uint8_t command,
	ConvertOptions const &convertOptions)
{
	// sanity check
	if ((dstType & EndpointType::CMD) == 0)
		return false;

	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::LEVEL:
	case EndpointType::PHYSICAL:
	case EndpointType::CONCENTRATION:
		dst.value.f = src;
		dst.command = command;
		break;
	case EndpointType::LIGHTING:
		dst.value.f = src;
		dst.command = command;
		dst.transition = 0;
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloatTransition(MessageType2 dstType, Message2 &dst, float src, uint8_t command, uint16_t transition,
	ConvertOptions const &convertOptions)
{
	// sanity check
	if ((dstType & EndpointType::CMD) == 0)
		return false;

	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::LIGHTING:
		dst.value.f = src;
		dst.command = command;
		dst.transition = transition;
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

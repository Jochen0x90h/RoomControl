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


using EndpointType = bus::PlugType;
String getTypeLabel(EndpointType type) {
	switch (type & EndpointType::CATEGORY) {
	case EndpointType::BINARY:
		switch (type & EndpointType::BINARY_CATEGORY) {
		case EndpointType::BINARY_BUTTON:
			switch (type & EndpointType::BINARY_BUTTON_CATEGORY) {
			case EndpointType::BINARY_BUTTON_WALL:
				return "Wall Button";
			default:
				return "Button";
			}
		case EndpointType::BINARY_SWITCH:
			switch (type & EndpointType::BINARY_SWITCH_CATEGORY) {
			case EndpointType::BINARY_SWITCH_WALL:
				return "Wall Switch";
			default:
				return "Switch";
			}
		case EndpointType::BINARY_POWER:
			switch (type & EndpointType::BINARY_POWER_CATEGORY) {
			case EndpointType::BINARY_POWER_LIGHT:
				return "Light On";
			case EndpointType::BINARY_POWER_FREEZER:
				return "Freezer On";
			case EndpointType::BINARY_POWER_FRIDGE:
				return "Fridge On";
			case EndpointType::BINARY_POWER_AC:
				return "AC On";
			case EndpointType::BINARY_POWER_OVEN:
				return "Oven On";
			case EndpointType::BINARY_POWER_COOKER:
				return "Cooker On";
			case EndpointType::BINARY_POWER_COFFEE:
				return "Coffee M. On";
			case EndpointType::BINARY_POWER_DISHWASHER:
				return "Dishwasher On";
			case EndpointType::BINARY_POWER_WASHING:
				return "Washing M. On";
			case EndpointType::BINARY_POWER_HIFI:
				return "Hi-Fi On";
			case EndpointType::BINARY_POWER_TV:
				return "TV On";
			default:
				return "Power On";
			}
		case EndpointType::BINARY_OPEN:
			switch (type & EndpointType::BINARY_OPEN_CATEGORY) {
			case EndpointType::BINARY_OPEN_GATE:
				return "Gate State";
			case EndpointType::BINARY_OPEN_DOOR:
				return "Door State";
			case EndpointType::BINARY_OPEN_WINDOW:
				return "Window State";
			case EndpointType::BINARY_OPEN_BLIND:
				return "Blind State";
			case EndpointType::BINARY_OPEN_SLAT:
				return "Slat State";
			case EndpointType::BINARY_OPEN_VALVE:
				return "Valve State";
			default:
				return "Open State";
			}
		case EndpointType::BINARY_LOCK:
			switch (type & EndpointType::BINARY_LOCK_CATEGORY) {
			case EndpointType::BINARY_LOCK_GATE:
				return "Gate Lock";
			case EndpointType::BINARY_LOCK_DOOR:
				return "Door Lock";
			case EndpointType::BINARY_LOCK_WINDOW:
				return "Window Lock";
			default:
				return "Lock State";
			}
		case EndpointType::BINARY_OCCUPANCY:
			return "Occupancy";
		case EndpointType::BINARY_ALARM:
			return "Alarm";
		case EndpointType::BINARY_ENABLE_CLOSE:
			return "Enable Close";
		default:
			return "Binary";
		}
	case EndpointType::TERNARY:
		switch (type & EndpointType::TERNARY_CATEGORY) {
		case EndpointType::TERNARY_BUTTON:
			switch (type & EndpointType::TERNARY_BUTTON_CATEGORY) {
			case EndpointType::TERNARY_BUTTON_WALL:
				return "Wall Up/Down Button";
			default:
				return "Up/Down Button";
			}
		case EndpointType::TERNARY_SWITCH:
			switch (type & EndpointType::TERNARY_SWITCH_CATEGORY) {
			case EndpointType::TERNARY_SWITCH_WALL:
				return "Wall Switch";
			default:
				return "Switch";
			}
		case EndpointType::TERNARY_OPENING:
			switch (type & EndpointType::TERNARY_OPENING_CATEGORY) {
			case EndpointType::TERNARY_OPENING_GATE:
				return "Gate Drive";
			case EndpointType::TERNARY_OPENING_DOOR:
				return "Door Drive";
			case EndpointType::TERNARY_OPENING_WINDOW:
				return "Window Drive";
			case EndpointType::TERNARY_OPENING_BLIND:
				return "Blind Drive";
			case EndpointType::TERNARY_OPENING_SLAT:
				return "Slat Drive";
			case EndpointType::TERNARY_OPENING_VALVE:
				return "Valve Drive";
			default:
				return "Opening Drive";
			}
		case EndpointType::TERNARY_LOCK:
			switch (type & EndpointType::TERNARY_LOCK_CATEGORY) {
			case EndpointType::TERNARY_LOCK_DOOR:
				return "Door Lock";
			case EndpointType::TERNARY_LOCK_WINDOW:
				return "Window Lock";
			default:
				return "Lock State";
			}
		default:
			return "Ternary";
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
		case EndpointType::LEVEL_OPEN:
			switch (type & EndpointType::LEVEL_OPEN_CATEGORY) {
			case EndpointType::LEVEL_OPEN_GATE:
				return "Gate Level";
			case EndpointType::LEVEL_OPEN_DOOR:
				return "Gate Level";
			case EndpointType::LEVEL_OPEN_WINDOW:
				return "Gate Level";
			case EndpointType::LEVEL_OPEN_BLIND:
				return "Blind Level";
			case EndpointType::LEVEL_OPEN_SLAT:
				return "Slat Level";
			case EndpointType::LEVEL_OPEN_VALVE:
				return "Valve Level";
			default:
				return "Open Level";
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
		case EndpointType::PHYSICAL_ILLUMINANCE:
			return "Illuminance";
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

Usage getUsage(EndpointType type) {
	bool cmd = (type & EndpointType::CMD) != 0;
	auto t = type & EndpointType::TYPE_MASK;

	switch (t & EndpointType::CATEGORY) {
	case EndpointType::BINARY:
		switch (t & EndpointType::BINARY_CATEGORY) {
		case EndpointType::BINARY_BUTTON:
			return Usage::RELEASED_PRESSED;
		case EndpointType::BINARY_SWITCH:
			return Usage::OFF_ON;
		case EndpointType::BINARY_POWER:
			return cmd ? Usage::OFF_ON_TOGGLE : Usage::OFF_ON;
		case EndpointType::BINARY_OPEN:
			return cmd ? Usage::CLOSED_OPEN_TOGGLE : Usage::CLOSED_OPEN;
		case EndpointType::BINARY_LOCK:
			return cmd ? Usage::LOCK_TOGGLE : Usage::LOCK;
		case EndpointType::BINARY_OCCUPANCY:
			return Usage::OCCUPANCY;
		case EndpointType::BINARY_ALARM:
			return Usage::ACTIVATION;
		case EndpointType::BINARY_ENABLE_CLOSE:
			return Usage::ENABLED;
		default:
			return Usage::OFF_ON;
		}
	case EndpointType::TERNARY:
		switch (t & EndpointType::TERNARY_CATEGORY) {
		case EndpointType::TERNARY_BUTTON:
			return Usage::RELEASED_UP_DOWN;
		case EndpointType::TERNARY_SWITCH:
			return Usage::OFF_ON1_ON2;
		case EndpointType::TERNARY_OPENING:
			return Usage::STOPPED_OPENING_CLOSING;
		case EndpointType::TERNARY_LOCK:
			return Usage::TILT_LOCK;
		default:
			return Usage::OFF_ON1_ON2;
		}
	case EndpointType::MULTISTATE:
		return Usage::NONE;
	case EndpointType::LEVEL:
		return Usage::PERCENT;
	case EndpointType::PHYSICAL:
		switch (t & EndpointType::PHYSICAL_CATEGORY) {
		case EndpointType::PHYSICAL_TEMPERATURE:
			switch (t & EndpointType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case EndpointType::PHYSICAL_TEMPERATURE_MEASURED:
				switch (t & EndpointType::PHYSICAL_TEMPERATURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FREEZER:
					return Usage::TEMPERATURE_FREEZER;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_FRIDGE:
					return Usage::TEMPERATURE_FRIDGE;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR:
					return Usage::TEMPERATURE_OUTDOOR;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM:
					return Usage::TEMPERATURE_ROOM;
				case EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OVEN:
					return Usage::TEMPERATURE_OVEN;
				default:
					return Usage::TEMPERATURE;
				}
			case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT:
				switch (t & EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_CATEGORY) {
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FREEZER:
					return Usage::TEMPERATURE_FEEZER;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_FRIDGE:
					return Usage::TEMPERATURE_FRIDGE;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_COOLER:
					return Usage::TEMPERATURE_ROOM;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER:
					return Usage::TEMPERATURE_ROOM;
				case EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_OVEN:
					return Usage::TEMPERATURE_OVEN;
				default:
					return Usage::TEMPERATURE;
				}
			default:
				return Usage::TEMPERATURE;
			}
		case EndpointType::PHYSICAL_PRESSURE:
			switch (t & EndpointType::PHYSICAL_PRESSURE_CATEGORY) {
			case EndpointType::PHYSICAL_PRESSURE_MEASURED:
				switch (t & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return Usage::PRESSURE_ATMOSPHERIC;
				default:
					return Usage::PASCAL;
				}
			case EndpointType::PHYSICAL_PRESSURE_SETPOINT:
				return Usage::PASCAL;
			default:
				return Usage::PASCAL;
			}
		case EndpointType::PHYSICAL_VOLTAGE:
			switch (t & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) {
			case EndpointType::PHYSICAL_VOLTAGE_MEASURED:
				switch (t & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return Usage::VOLTAGE_LOW;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return Usage::VOLTAGE_MAINS;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return Usage::VOLTAGE_HIGH;
				default:
					return Usage::VOLTAGE;
				}
			case EndpointType::PHYSICAL_VOLTAGE_SETPOINT:
				return Usage::VOLTAGE;
			default:
				return Usage::VOLTAGE;
			}
		case EndpointType::PHYSICAL_CURRENT:
			return Usage::AMPERE;
		case EndpointType::PHYSICAL_POWER:
			return Usage::WATT;
		case EndpointType::PHYSICAL_ILLUMINANCE:
			return Usage::LUX;
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
			return Usage::PERCENT;
		case EndpointType::LIGHTING_COLOR_TEMPERATURE:
			return Usage::TEMPERATURE_COLOR;
		case EndpointType::LIGHTING_COLOR_PARAMETER:
			return Usage::UNIT_INTERVAL;
		default:
			return Usage::NONE;
		}
	case EndpointType::METERING:
		switch (t & EndpointType::METERING_CATEGORY) {
		case EndpointType::METERING_ELECTRIC:
			return Usage::ELECTRIC_METER;
		case EndpointType::METERING_WATER:
			return Usage::COUNTER;
		case EndpointType::METERING_GAS:
			return Usage::COUNTER;
		default:
			return Usage::COUNTER;
		}
	default:
		return Usage::NONE;
	}
}

bool isCompatible(EndpointType dstType, EndpointType srcType) {
	if ((srcType & EndpointType::DIRECTION_MASK) != EndpointType::OUT || (dstType & EndpointType::DIRECTION_MASK) != EndpointType::IN)
		return false;
	bool srcCommand = (srcType & EndpointType::CMD) != 0;
	bool dstCommand = (dstType & EndpointType::CMD) != 0;
	if (srcCommand && !dstCommand)
		return false;
	auto src = srcType & EndpointType::TYPE_MASK;
	auto dst = dstType & EndpointType::TYPE_MASK;
	auto srcCategory = src & EndpointType::CATEGORY;
	auto dstCategory = dst & EndpointType::CATEGORY;
	bool srcSwitch = srcCategory == EndpointType::BINARY || srcCategory == EndpointType::TERNARY;
	bool dstSwitch = dstCategory == EndpointType::BINARY || dstCategory == EndpointType::TERNARY;
	if (srcSwitch && dstSwitch)
		return true;
	if (srcSwitch && dstCommand)
		return true;
	if (!srcCommand && dstSwitch)
		return true;

	if ((src & EndpointType::CATEGORY) == dst) return true;
	switch (dst & EndpointType::CATEGORY) {
	case EndpointType::BINARY:
		return (src & EndpointType::CATEGORY) == EndpointType::BINARY;
	case EndpointType::TERNARY:
		return (src & EndpointType::CATEGORY) == EndpointType::TERNARY;
	case EndpointType::MULTISTATE:
		if ((src & EndpointType::MULTISTATE_CATEGORY) == dst) return true;
		switch (dst & EndpointType::MULTISTATE_CATEGORY) {
		case EndpointType::MULTISTATE_THERMOSTAT_MODE:
			return (src & EndpointType::MULTISTATE_CATEGORY) == EndpointType::MULTISTATE_THERMOSTAT_MODE;
		default:
			return false;
		}
	case EndpointType::LEVEL:
		if ((src & EndpointType::LEVEL_CATEGORY) == dst) return true;
		switch (dst & EndpointType::LEVEL_CATEGORY) {
		case EndpointType::LEVEL_OPEN:
			return (src & EndpointType::LEVEL_CATEGORY) == EndpointType::LEVEL_OPEN;
		case EndpointType::LEVEL_BATTERY:
			return (src & EndpointType::LEVEL_CATEGORY) == EndpointType::LEVEL_BATTERY;
		case EndpointType::LEVEL_TANK:
			return (src & EndpointType::LEVEL_CATEGORY) == EndpointType::LEVEL_TANK;
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
					return (src & EndpointType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) == EndpointType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE;
				default:
					return false;
				}
			case EndpointType::PHYSICAL_PRESSURE_SETPOINT:
				return (src & EndpointType::PHYSICAL_PRESSURE_CATEGORY) == EndpointType::PHYSICAL_PRESSURE_SETPOINT;
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
					return (src & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == EndpointType::PHYSICAL_VOLTAGE_MEASURED_LOW;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return (src & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == EndpointType::PHYSICAL_VOLTAGE_MEASURED_MAINS;
				case EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return (src & EndpointType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == EndpointType::PHYSICAL_VOLTAGE_MEASURED_HIGH;
				default:
					return false;
				}
			case EndpointType::PHYSICAL_VOLTAGE_SETPOINT:
				return (src & EndpointType::PHYSICAL_VOLTAGE_CATEGORY) == EndpointType::PHYSICAL_VOLTAGE_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_CURRENT:
			if ((src & EndpointType::PHYSICAL_CURRENT_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_CURRENT_CATEGORY) {
			case EndpointType::PHYSICAL_CURRENT_MEASURED:
				return (src & EndpointType::PHYSICAL_CURRENT_CATEGORY) == EndpointType::PHYSICAL_CURRENT_MEASURED;
			case EndpointType::PHYSICAL_CURRENT_SETPOINT:
				return (src & EndpointType::PHYSICAL_CURRENT_CATEGORY) == EndpointType::PHYSICAL_CURRENT_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_POWER:
			if ((src & EndpointType::PHYSICAL_POWER_CATEGORY) == dst) return true;
			switch (dst & EndpointType::PHYSICAL_POWER_CATEGORY) {
			case EndpointType::PHYSICAL_POWER_MEASURED:
				return (src & EndpointType::PHYSICAL_POWER_CATEGORY) == EndpointType::PHYSICAL_POWER_MEASURED;
			case EndpointType::PHYSICAL_POWER_SETPOINT:
				return (src & EndpointType::PHYSICAL_POWER_CATEGORY) == EndpointType::PHYSICAL_POWER_SETPOINT;
			default:
				return false;
			}
		case EndpointType::PHYSICAL_ILLUMINANCE:
			return (src & EndpointType::PHYSICAL_CATEGORY) == EndpointType::PHYSICAL_ILLUMINANCE;
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
					return (src & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) == EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR;
				default:
					return false;
				}
			case EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT:
				return (src & EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) == EndpointType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT;
			default:
				return false;
			}
		case EndpointType::CONCENTRATION_VOC:
			return (src & EndpointType::CONCENTRATION_CATEGORY) == EndpointType::CONCENTRATION_VOC;
		case EndpointType::CONCENTRATION_CARBON_MONOXIDE:
			return (src & EndpointType::CONCENTRATION_CATEGORY) == EndpointType::CONCENTRATION_CARBON_MONOXIDE;
		case EndpointType::CONCENTRATION_CARBON_DIOXIDE:
			return (src & EndpointType::CONCENTRATION_CATEGORY) == EndpointType::CONCENTRATION_CARBON_DIOXIDE;
		default:
			return false;
		}
	case EndpointType::LIGHTING:
		if ((src & EndpointType::LIGHTING_CATEGORY) == dst) return true;
		switch (dst & EndpointType::LIGHTING_CATEGORY) {
		case EndpointType::LIGHTING_BRIGHTNESS:
			return (src & EndpointType::LIGHTING_CATEGORY) == EndpointType::LIGHTING_BRIGHTNESS;
		case EndpointType::LIGHTING_COLOR_TEMPERATURE:
			return (src & EndpointType::LIGHTING_CATEGORY) == EndpointType::LIGHTING_COLOR_TEMPERATURE;
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
					return (src & EndpointType::METERING_ELECTRIC_USAGE_CATEGORY) == EndpointType::METERING_ELECTRIC_USAGE_PEAK;
				case EndpointType::METERING_ELECTRIC_USAGE_OFF_PEAK:
					return (src & EndpointType::METERING_ELECTRIC_USAGE_CATEGORY) == EndpointType::METERING_ELECTRIC_USAGE_OFF_PEAK;
				default:
					return false;
				}
			case EndpointType::METERING_ELECTRIC_SUPPLY:
				if ((src & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) == dst) return true;
				switch (dst & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) {
				case EndpointType::METERING_ELECTRIC_SUPPLY_PEAK:
					return (src & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) == EndpointType::METERING_ELECTRIC_SUPPLY_PEAK;
				case EndpointType::METERING_ELECTRIC_SUPPLY_OFF_PEAK:
					return (src & EndpointType::METERING_ELECTRIC_SUPPLY_CATEGORY) == EndpointType::METERING_ELECTRIC_SUPPLY_OFF_PEAK;
				default:
					return false;
				}
			default:
				return false;
			}
		case EndpointType::METERING_WATER:
			return (src & EndpointType::METERING_CATEGORY) == EndpointType::METERING_WATER;
		case EndpointType::METERING_GAS:
			return (src & EndpointType::METERING_CATEGORY) == EndpointType::METERING_GAS;
		default:
			return false;
		}
	default:
		return false;
	}
}


//

bool convertSwitch(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src >= 3)
		return false;

	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::BINARY:
	case EndpointType::TERNARY: {
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

bool convertFloat(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions)
{
	switch (dstType & EndpointType::CATEGORY) {
	case EndpointType::BINARY:
	case EndpointType::TERNARY: {
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

bool convertFloatCommand(MessageType dstType, Message &dst, float src, uint8_t command,
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

bool convertFloatTransition(MessageType dstType, Message &dst, float src, uint8_t command, uint16_t transition,
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

#include "Message.hpp"
//#include <Terminal.hpp>
//#include <StringOperators.hpp>


using PlugType = bus::PlugType;
String getTypeLabel(PlugType type) {
	switch (type & PlugType::CATEGORY) {
	case PlugType::BINARY:
		switch (type & PlugType::BINARY_CATEGORY) {
		case PlugType::BINARY_BUTTON:
			switch (type & PlugType::BINARY_BUTTON_CATEGORY) {
			case PlugType::BINARY_BUTTON_WALL:
				return "Wall Button";
			default:
				return "Button";
			}
		case PlugType::BINARY_SWITCH:
			switch (type & PlugType::BINARY_SWITCH_CATEGORY) {
			case PlugType::BINARY_SWITCH_WALL:
				return "Wall Switch";
			default:
				return "Switch";
			}
		case PlugType::BINARY_POWER:
			switch (type & PlugType::BINARY_POWER_CATEGORY) {
			case PlugType::BINARY_POWER_LIGHT:
				return "Light On";
			case PlugType::BINARY_POWER_FREEZER:
				return "Freezer On";
			case PlugType::BINARY_POWER_FRIDGE:
				return "Fridge On";
			case PlugType::BINARY_POWER_AC:
				return "AC On";
			case PlugType::BINARY_POWER_OVEN:
				return "Oven On";
			case PlugType::BINARY_POWER_COOKER:
				return "Cooker On";
			case PlugType::BINARY_POWER_COFFEE:
				return "Coffee M. On";
			case PlugType::BINARY_POWER_DISHWASHER:
				return "Dishwasher On";
			case PlugType::BINARY_POWER_WASHING:
				return "Washing M. On";
			case PlugType::BINARY_POWER_HIFI:
				return "Hi-Fi On";
			case PlugType::BINARY_POWER_TV:
				return "TV On";
			default:
				return "On/Off";
			}
		case PlugType::BINARY_OPEN:
			switch (type & PlugType::BINARY_OPEN_CATEGORY) {
			case PlugType::BINARY_OPEN_GATE:
				return "Gate State";
			case PlugType::BINARY_OPEN_DOOR:
				return "Door State";
			case PlugType::BINARY_OPEN_WINDOW:
				return "Window State";
			case PlugType::BINARY_OPEN_BLIND:
				return "Blind State";
			case PlugType::BINARY_OPEN_SLAT:
				return "Slat State";
			case PlugType::BINARY_OPEN_VALVE:
				return "Valve State";
			default:
				return "Open State";
			}
		case PlugType::BINARY_LOCK:
			switch (type & PlugType::BINARY_LOCK_CATEGORY) {
			case PlugType::BINARY_LOCK_GATE:
				return "Gate Lock";
			case PlugType::BINARY_LOCK_DOOR:
				return "Door Lock";
			case PlugType::BINARY_LOCK_WINDOW:
				return "Window Lock";
			default:
				return "Lock State";
			}
		case PlugType::BINARY_OCCUPANCY:
			return "Occupancy";
		case PlugType::BINARY_ALARM:
			return "Alarm";
		case PlugType::BINARY_ENABLE_CLOSE:
			return "Enable Close";
		default:
			return "Binary";
		}
	case PlugType::TERNARY:
		switch (type & PlugType::TERNARY_CATEGORY) {
		case PlugType::TERNARY_BUTTON:
			switch (type & PlugType::TERNARY_BUTTON_CATEGORY) {
			case PlugType::TERNARY_BUTTON_WALL:
				return "Wall Up/Down Button";
			default:
				return "Up/Down Button";
			}
		case PlugType::TERNARY_SWITCH:
			switch (type & PlugType::TERNARY_SWITCH_CATEGORY) {
			case PlugType::TERNARY_SWITCH_WALL:
				return "Wall Switch";
			default:
				return "Switch";
			}
		case PlugType::TERNARY_OPENING:
			switch (type & PlugType::TERNARY_OPENING_CATEGORY) {
			case PlugType::TERNARY_OPENING_GATE:
				return "Gate Drive";
			case PlugType::TERNARY_OPENING_DOOR:
				return "Door Drive";
			case PlugType::TERNARY_OPENING_WINDOW:
				return "Window Drive";
			case PlugType::TERNARY_OPENING_BLIND:
				return "Blind Drive";
			case PlugType::TERNARY_OPENING_SLAT:
				return "Slat Drive";
			case PlugType::TERNARY_OPENING_VALVE:
				return "Valve Drive";
			default:
				return "Opening Drive";
			}
		case PlugType::TERNARY_LOCK:
			switch (type & PlugType::TERNARY_LOCK_CATEGORY) {
			case PlugType::TERNARY_LOCK_DOOR:
				return "Door Lock";
			case PlugType::TERNARY_LOCK_WINDOW:
				return "Window Lock";
			default:
				return "Lock State";
			}
		default:
			return "Ternary";
		}
	case PlugType::MULTISTATE:
		switch (type & PlugType::MULTISTATE_CATEGORY) {
		case PlugType::MULTISTATE_THERMOSTAT_MODE:
			return "Mode";
		default:
			return "Multistate";
		}
	case PlugType::LEVEL:
		switch (type & PlugType::LEVEL_CATEGORY) {
		case PlugType::LEVEL_OPEN:
			switch (type & PlugType::LEVEL_OPEN_CATEGORY) {
			case PlugType::LEVEL_OPEN_GATE:
				return "Gate Level";
			case PlugType::LEVEL_OPEN_DOOR:
				return "Gate Level";
			case PlugType::LEVEL_OPEN_WINDOW:
				return "Gate Level";
			case PlugType::LEVEL_OPEN_BLIND:
				return "Blind Level";
			case PlugType::LEVEL_OPEN_SLAT:
				return "Slat Level";
			case PlugType::LEVEL_OPEN_VALVE:
				return "Valve Level";
			default:
				return "Open Level";
			}
		case PlugType::LEVEL_BATTERY:
			return "Battery Level";
		case PlugType::LEVEL_TANK:
			return "Tank Level";
		default:
			return "Level";
		}
	case PlugType::PHYSICAL:
		switch (type & PlugType::PHYSICAL_CATEGORY) {
		case PlugType::PHYSICAL_TEMPERATURE:
			switch (type & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case PlugType::PHYSICAL_TEMPERATURE_MEASURED:
				switch (type & PlugType::PHYSICAL_TEMPERATURE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_FREEZER:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_FRIDGE:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_OVEN:
					return "Measured Temp.";
				default:
					return "Measured Temp.";
				}
			case PlugType::PHYSICAL_TEMPERATURE_SETPOINT:
				switch (type & PlugType::PHYSICAL_TEMPERATURE_SETPOINT_CATEGORY) {
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_FREEZER:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_FRIDGE:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_COOLER:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER:
					return "Measured Temp.";
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_OVEN:
					return "Measured Temp.";
				default:
					return "Setpoint Temp.";
				}
			default:
				return "Temperature";
			}
		case PlugType::PHYSICAL_PRESSURE:
			switch (type & PlugType::PHYSICAL_PRESSURE_CATEGORY) {
			case PlugType::PHYSICAL_PRESSURE_MEASURED:
				switch (type & PlugType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return "Measured Pressure";
				default:
					return "Measured Pressure";
				}
			case PlugType::PHYSICAL_PRESSURE_SETPOINT:
				return "Pressure Setpoint";
			default:
				return "Pressure";
			}
		case PlugType::PHYSICAL_VOLTAGE:
			switch (type & PlugType::PHYSICAL_VOLTAGE_CATEGORY) {
			case PlugType::PHYSICAL_VOLTAGE_MEASURED:
				switch (type & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return "Measured Voltage";
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return "Measured Voltage";
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return "Measured Voltage";
				default:
					return "Measured Voltage";
				}
			case PlugType::PHYSICAL_VOLTAGE_SETPOINT:
				return "Voltage Setpoint";
			default:
				return "Voltage";
			}
		case PlugType::PHYSICAL_CURRENT:
			switch (type & PlugType::PHYSICAL_CURRENT_CATEGORY) {
			case PlugType::PHYSICAL_CURRENT_MEASURED:
				return "Measured Curent";
			case PlugType::PHYSICAL_CURRENT_SETPOINT:
				return "Curent Setpoint";
			default:
				return "Current";
			}
		case PlugType::PHYSICAL_POWER:
			switch (type & PlugType::PHYSICAL_POWER_CATEGORY) {
			case PlugType::PHYSICAL_POWER_MEASURED:
				return "Measured Power";
			case PlugType::PHYSICAL_POWER_SETPOINT:
				return "Power Setpoint";
			default:
				return "Current";
			}
		case PlugType::PHYSICAL_ILLUMINANCE:
			return "Illuminance";
		default:
			return "Physical";
		}
	case PlugType::CONCENTRATION:
		switch (type & PlugType::CONCENTRATION_CATEGORY) {
		case PlugType::CONCENTRATION_RELATIVE_HUMIDITY:
			switch (type & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) {
			case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED:
				switch (type & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) {
				case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR:
					return "Measured Humidity";
				default:
					return "Measured Humidity";
				}
			case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT:
				return "Setpoint Temp.";
			default:
				return "Humidity";
			}
		case PlugType::CONCENTRATION_VOC:
			return "Volatile Organic";
		case PlugType::CONCENTRATION_CARBON_MONOXIDE:
			return "Carbon Monox.";
		case PlugType::CONCENTRATION_CARBON_DIOXIDE:
			return "Carbon Diox.";
		default:
			return "Concentration";
		}
	case PlugType::LIGHTING:
		switch (type & PlugType::LIGHTING_CATEGORY) {
		case PlugType::LIGHTING_BRIGHTNESS:
			return "Brightness";
		case PlugType::LIGHTING_COLOR_TEMPERATURE:
			return "Color Temp.";
		case PlugType::LIGHTING_COLOR_PARAMETER:
			switch (type & PlugType::LIGHTING_COLOR_PARAMETER_CATEGORY) {
			case PlugType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X:
				return "Color X";
			case PlugType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y:
				return "Color Y";
			case PlugType::LIGHTING_COLOR_PARAMETER_HUE:
				return "Hue";
			case PlugType::LIGHTING_COLOR_PARAMETER_SATURATION:
				return "Saturation";
			default:
				return "Color Parameter";
			}
		default:
			return "Lighting";
		}
	case PlugType::METERING:
		switch (type & PlugType::METERING_CATEGORY) {
		case PlugType::METERING_ELECTRIC:
			switch (type & PlugType::METERING_ELECTRIC_CATEGORY) {
			case PlugType::METERING_ELECTRIC_USAGE:
				switch (type & PlugType::METERING_ELECTRIC_USAGE_CATEGORY) {
				case PlugType::METERING_ELECTRIC_USAGE_PEAK:
					return "Peak Usage";
				case PlugType::METERING_ELECTRIC_USAGE_OFF_PEAK:
					return "Off-Peak Usage";
				default:
					return "Energy Usage";
				}
			case PlugType::METERING_ELECTRIC_SUPPLY:
				switch (type & PlugType::METERING_ELECTRIC_SUPPLY_CATEGORY) {
				case PlugType::METERING_ELECTRIC_SUPPLY_PEAK:
					return "Peak Supply";
				case PlugType::METERING_ELECTRIC_SUPPLY_OFF_PEAK:
					return "Off-Peak Supply";
				default:
					return "Supply";
				}
			default:
				return "Electric Meter";
			}
		case PlugType::METERING_WATER:
			return "Water Meter";
		case PlugType::METERING_GAS:
			return "Gas Meter";
		default:
			return "Metering";
		}
	default:
		return {};
	}
}

Usage getUsage(PlugType type) {
	bool cmd = (type & PlugType::CMD) != 0;
	auto t = type & PlugType::TYPE_MASK;

	switch (t & PlugType::CATEGORY) {
	case PlugType::BINARY:
		switch (t & PlugType::BINARY_CATEGORY) {
		case PlugType::BINARY_BUTTON:
			return Usage::RELEASED_PRESSED;
		case PlugType::BINARY_SWITCH:
			return Usage::OFF_ON;
		case PlugType::BINARY_POWER:
			return cmd ? Usage::OFF_ON_TOGGLE : Usage::OFF_ON;
		case PlugType::BINARY_OPEN:
			return cmd ? Usage::CLOSED_OPEN_TOGGLE : Usage::CLOSED_OPEN;
		case PlugType::BINARY_LOCK:
			return cmd ? Usage::LOCK_TOGGLE : Usage::LOCK;
		case PlugType::BINARY_OCCUPANCY:
			return Usage::OCCUPANCY;
		case PlugType::BINARY_ALARM:
			return Usage::ACTIVATION;
		case PlugType::BINARY_ENABLE_CLOSE:
			return Usage::ENABLED;
		default:
			return Usage::OFF_ON;
		}
	case PlugType::TERNARY:
		switch (t & PlugType::TERNARY_CATEGORY) {
		case PlugType::TERNARY_BUTTON:
			return Usage::RELEASED_UP_DOWN;
		case PlugType::TERNARY_SWITCH:
			return Usage::OFF_ON1_ON2;
		case PlugType::TERNARY_OPENING:
			return Usage::STOPPED_OPENING_CLOSING;
		case PlugType::TERNARY_LOCK:
			return Usage::TILT_LOCK;
		default:
			return Usage::OFF_ON1_ON2;
		}
	case PlugType::MULTISTATE:
		return Usage::NONE;
	case PlugType::LEVEL:
		return Usage::PERCENT;
	case PlugType::PHYSICAL:
		switch (t & PlugType::PHYSICAL_CATEGORY) {
		case PlugType::PHYSICAL_TEMPERATURE:
			switch (t & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case PlugType::PHYSICAL_TEMPERATURE_MEASURED:
				switch (t & PlugType::PHYSICAL_TEMPERATURE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_FREEZER:
					return Usage::TEMPERATURE_FREEZER;
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_FRIDGE:
					return Usage::TEMPERATURE_FRIDGE;
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR:
					return Usage::TEMPERATURE_OUTDOOR;
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM:
					return Usage::TEMPERATURE_ROOM;
				case PlugType::PHYSICAL_TEMPERATURE_MEASURED_OVEN:
					return Usage::TEMPERATURE_OVEN;
				default:
					return Usage::TEMPERATURE;
				}
			case PlugType::PHYSICAL_TEMPERATURE_SETPOINT:
				switch (t & PlugType::PHYSICAL_TEMPERATURE_SETPOINT_CATEGORY) {
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_FREEZER:
					return Usage::TEMPERATURE_FEEZER;
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_FRIDGE:
					return Usage::TEMPERATURE_FRIDGE;
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_COOLER:
					return Usage::TEMPERATURE_ROOM;
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER:
					return Usage::TEMPERATURE_ROOM;
				case PlugType::PHYSICAL_TEMPERATURE_SETPOINT_OVEN:
					return Usage::TEMPERATURE_OVEN;
				default:
					return Usage::TEMPERATURE;
				}
			default:
				return Usage::TEMPERATURE;
			}
		case PlugType::PHYSICAL_PRESSURE:
			switch (t & PlugType::PHYSICAL_PRESSURE_CATEGORY) {
			case PlugType::PHYSICAL_PRESSURE_MEASURED:
				switch (t & PlugType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return Usage::PRESSURE_ATMOSPHERIC;
				default:
					return Usage::PASCAL;
				}
			case PlugType::PHYSICAL_PRESSURE_SETPOINT:
				return Usage::PASCAL;
			default:
				return Usage::PASCAL;
			}
		case PlugType::PHYSICAL_VOLTAGE:
			switch (t & PlugType::PHYSICAL_VOLTAGE_CATEGORY) {
			case PlugType::PHYSICAL_VOLTAGE_MEASURED:
				switch (t & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return Usage::VOLTAGE_LOW;
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return Usage::VOLTAGE_MAINS;
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return Usage::VOLTAGE_HIGH;
				default:
					return Usage::VOLTAGE;
				}
			case PlugType::PHYSICAL_VOLTAGE_SETPOINT:
				return Usage::VOLTAGE;
			default:
				return Usage::VOLTAGE;
			}
		case PlugType::PHYSICAL_CURRENT:
			return Usage::AMPERE;
		case PlugType::PHYSICAL_POWER:
			return Usage::WATT;
		case PlugType::PHYSICAL_ILLUMINANCE:
			return Usage::LUX;
		default:
			return Usage::NONE;
		}
	case PlugType::CONCENTRATION:
		switch (t & PlugType::CONCENTRATION_CATEGORY) {
		case PlugType::CONCENTRATION_RELATIVE_HUMIDITY:
			return Usage::PERCENT;
		case PlugType::CONCENTRATION_VOC:
			return Usage::NONE;
		case PlugType::CONCENTRATION_CARBON_MONOXIDE:
			return Usage::NONE;
		case PlugType::CONCENTRATION_CARBON_DIOXIDE:
			return Usage::NONE;
		default:
			return Usage::NONE;
		}
	case PlugType::LIGHTING:
		switch (t & PlugType::LIGHTING_CATEGORY) {
		case PlugType::LIGHTING_BRIGHTNESS:
			return Usage::PERCENT;
		case PlugType::LIGHTING_COLOR_TEMPERATURE:
			return Usage::TEMPERATURE_COLOR;
		case PlugType::LIGHTING_COLOR_PARAMETER:
			return Usage::UNIT_INTERVAL;
		default:
			return Usage::NONE;
		}
	case PlugType::METERING:
		switch (t & PlugType::METERING_CATEGORY) {
		case PlugType::METERING_ELECTRIC:
			return Usage::ELECTRIC_METER;
		case PlugType::METERING_WATER:
			return Usage::COUNTER;
		case PlugType::METERING_GAS:
			return Usage::COUNTER;
		default:
			return Usage::COUNTER;
		}
	default:
		return Usage::NONE;
	}
}

bool isCompatible(PlugType dstType, PlugType srcType) {
	if ((srcType & PlugType::DIRECTION_MASK) != PlugType::OUT || (dstType & PlugType::DIRECTION_MASK) != PlugType::IN)
		return false;
	bool srcCommand = (srcType & PlugType::CMD) != 0;
	bool dstCommand = (dstType & PlugType::CMD) != 0;
	if (srcCommand && !dstCommand)
		return false;
	auto src = srcType & PlugType::TYPE_MASK;
	auto dst = dstType & PlugType::TYPE_MASK;
	auto srcCategory = src & PlugType::CATEGORY;
	auto dstCategory = dst & PlugType::CATEGORY;
	bool srcSwitch = srcCategory == PlugType::BINARY || srcCategory == PlugType::TERNARY;
	bool dstSwitch = dstCategory == PlugType::BINARY || dstCategory == PlugType::TERNARY;
	if (srcSwitch && dstSwitch)
		return true;
	if (srcSwitch && dstCommand)
		return true;
	if (!srcCommand && dstSwitch)
		return true;

	if ((src & PlugType::CATEGORY) == dst) return true;
	switch (dst & PlugType::CATEGORY) {
	case PlugType::BINARY:
		return (src & PlugType::CATEGORY) == PlugType::BINARY;
	case PlugType::TERNARY:
		return (src & PlugType::CATEGORY) == PlugType::TERNARY;
	case PlugType::MULTISTATE:
		if ((src & PlugType::MULTISTATE_CATEGORY) == dst) return true;
		switch (dst & PlugType::MULTISTATE_CATEGORY) {
		case PlugType::MULTISTATE_THERMOSTAT_MODE:
			return (src & PlugType::MULTISTATE_CATEGORY) == PlugType::MULTISTATE_THERMOSTAT_MODE;
		default:
			return false;
		}
	case PlugType::LEVEL:
		if ((src & PlugType::LEVEL_CATEGORY) == dst) return true;
		switch (dst & PlugType::LEVEL_CATEGORY) {
		case PlugType::LEVEL_OPEN:
			return (src & PlugType::LEVEL_CATEGORY) == PlugType::LEVEL_OPEN;
		case PlugType::LEVEL_BATTERY:
			return (src & PlugType::LEVEL_CATEGORY) == PlugType::LEVEL_BATTERY;
		case PlugType::LEVEL_TANK:
			return (src & PlugType::LEVEL_CATEGORY) == PlugType::LEVEL_TANK;
		default:
			return false;
		}
	case PlugType::PHYSICAL:
		if ((src & PlugType::PHYSICAL_CATEGORY) == dst) return true;
		switch (dst & PlugType::PHYSICAL_CATEGORY) {
		case PlugType::PHYSICAL_TEMPERATURE:
			if ((src & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) == dst) return true;
			switch (dst & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) {
			case PlugType::PHYSICAL_TEMPERATURE_MEASURED:
				return (src & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) == PlugType::PHYSICAL_TEMPERATURE_MEASURED;
			case PlugType::PHYSICAL_TEMPERATURE_SETPOINT:
				return (src & PlugType::PHYSICAL_TEMPERATURE_CATEGORY) == PlugType::PHYSICAL_TEMPERATURE_SETPOINT;
			default:
				return false;
			}
		case PlugType::PHYSICAL_PRESSURE:
			if ((src & PlugType::PHYSICAL_PRESSURE_CATEGORY) == dst) return true;
			switch (dst & PlugType::PHYSICAL_PRESSURE_CATEGORY) {
			case PlugType::PHYSICAL_PRESSURE_MEASURED:
				if ((src & PlugType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) == dst) return true;
				switch (dst & PlugType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE:
					return (src & PlugType::PHYSICAL_PRESSURE_MEASURED_CATEGORY) == PlugType::PHYSICAL_PRESSURE_MEASURED_ATMOSPHERE;
				default:
					return false;
				}
			case PlugType::PHYSICAL_PRESSURE_SETPOINT:
				return (src & PlugType::PHYSICAL_PRESSURE_CATEGORY) == PlugType::PHYSICAL_PRESSURE_SETPOINT;
			default:
				return false;
			}
		case PlugType::PHYSICAL_VOLTAGE:
			if ((src & PlugType::PHYSICAL_VOLTAGE_CATEGORY) == dst) return true;
			switch (dst & PlugType::PHYSICAL_VOLTAGE_CATEGORY) {
			case PlugType::PHYSICAL_VOLTAGE_MEASURED:
				if ((src & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == dst) return true;
				switch (dst & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) {
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_LOW:
					return (src & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == PlugType::PHYSICAL_VOLTAGE_MEASURED_LOW;
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_MAINS:
					return (src & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == PlugType::PHYSICAL_VOLTAGE_MEASURED_MAINS;
				case PlugType::PHYSICAL_VOLTAGE_MEASURED_HIGH:
					return (src & PlugType::PHYSICAL_VOLTAGE_MEASURED_CATEGORY) == PlugType::PHYSICAL_VOLTAGE_MEASURED_HIGH;
				default:
					return false;
				}
			case PlugType::PHYSICAL_VOLTAGE_SETPOINT:
				return (src & PlugType::PHYSICAL_VOLTAGE_CATEGORY) == PlugType::PHYSICAL_VOLTAGE_SETPOINT;
			default:
				return false;
			}
		case PlugType::PHYSICAL_CURRENT:
			if ((src & PlugType::PHYSICAL_CURRENT_CATEGORY) == dst) return true;
			switch (dst & PlugType::PHYSICAL_CURRENT_CATEGORY) {
			case PlugType::PHYSICAL_CURRENT_MEASURED:
				return (src & PlugType::PHYSICAL_CURRENT_CATEGORY) == PlugType::PHYSICAL_CURRENT_MEASURED;
			case PlugType::PHYSICAL_CURRENT_SETPOINT:
				return (src & PlugType::PHYSICAL_CURRENT_CATEGORY) == PlugType::PHYSICAL_CURRENT_SETPOINT;
			default:
				return false;
			}
		case PlugType::PHYSICAL_POWER:
			if ((src & PlugType::PHYSICAL_POWER_CATEGORY) == dst) return true;
			switch (dst & PlugType::PHYSICAL_POWER_CATEGORY) {
			case PlugType::PHYSICAL_POWER_MEASURED:
				return (src & PlugType::PHYSICAL_POWER_CATEGORY) == PlugType::PHYSICAL_POWER_MEASURED;
			case PlugType::PHYSICAL_POWER_SETPOINT:
				return (src & PlugType::PHYSICAL_POWER_CATEGORY) == PlugType::PHYSICAL_POWER_SETPOINT;
			default:
				return false;
			}
		case PlugType::PHYSICAL_ILLUMINANCE:
			return (src & PlugType::PHYSICAL_CATEGORY) == PlugType::PHYSICAL_ILLUMINANCE;
		default:
			return false;
		}
	case PlugType::CONCENTRATION:
		if ((src & PlugType::CONCENTRATION_CATEGORY) == dst) return true;
		switch (dst & PlugType::CONCENTRATION_CATEGORY) {
		case PlugType::CONCENTRATION_RELATIVE_HUMIDITY:
			if ((src & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) == dst) return true;
			switch (dst & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) {
			case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED:
				if ((src & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) == dst) return true;
				switch (dst & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) {
				case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR:
					return (src & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_CATEGORY) == PlugType::CONCENTRATION_RELATIVE_HUMIDITY_MEASURED_AIR;
				default:
					return false;
				}
			case PlugType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT:
				return (src & PlugType::CONCENTRATION_RELATIVE_HUMIDITY_CATEGORY) == PlugType::CONCENTRATION_RELATIVE_HUMIDITY_SETPOINT;
			default:
				return false;
			}
		case PlugType::CONCENTRATION_VOC:
			return (src & PlugType::CONCENTRATION_CATEGORY) == PlugType::CONCENTRATION_VOC;
		case PlugType::CONCENTRATION_CARBON_MONOXIDE:
			return (src & PlugType::CONCENTRATION_CATEGORY) == PlugType::CONCENTRATION_CARBON_MONOXIDE;
		case PlugType::CONCENTRATION_CARBON_DIOXIDE:
			return (src & PlugType::CONCENTRATION_CATEGORY) == PlugType::CONCENTRATION_CARBON_DIOXIDE;
		default:
			return false;
		}
	case PlugType::LIGHTING:
		if ((src & PlugType::LIGHTING_CATEGORY) == dst) return true;
		switch (dst & PlugType::LIGHTING_CATEGORY) {
		case PlugType::LIGHTING_BRIGHTNESS:
			return (src & PlugType::LIGHTING_CATEGORY) == PlugType::LIGHTING_BRIGHTNESS;
		case PlugType::LIGHTING_COLOR_TEMPERATURE:
			return (src & PlugType::LIGHTING_CATEGORY) == PlugType::LIGHTING_COLOR_TEMPERATURE;
		case PlugType::LIGHTING_COLOR_PARAMETER:
			return (src & PlugType::LIGHTING_CATEGORY) == PlugType::LIGHTING_COLOR_PARAMETER;
		default:
			return false;
		}
	case PlugType::METERING:
		if ((src & PlugType::METERING_CATEGORY) == dst) return true;
		switch (dst & PlugType::METERING_CATEGORY) {
		case PlugType::METERING_ELECTRIC:
			if ((src & PlugType::METERING_ELECTRIC_CATEGORY) == dst) return true;
			switch (dst & PlugType::METERING_ELECTRIC_CATEGORY) {
			case PlugType::METERING_ELECTRIC_USAGE:
				if ((src & PlugType::METERING_ELECTRIC_USAGE_CATEGORY) == dst) return true;
				switch (dst & PlugType::METERING_ELECTRIC_USAGE_CATEGORY) {
				case PlugType::METERING_ELECTRIC_USAGE_PEAK:
					return (src & PlugType::METERING_ELECTRIC_USAGE_CATEGORY) == PlugType::METERING_ELECTRIC_USAGE_PEAK;
				case PlugType::METERING_ELECTRIC_USAGE_OFF_PEAK:
					return (src & PlugType::METERING_ELECTRIC_USAGE_CATEGORY) == PlugType::METERING_ELECTRIC_USAGE_OFF_PEAK;
				default:
					return false;
				}
			case PlugType::METERING_ELECTRIC_SUPPLY:
				if ((src & PlugType::METERING_ELECTRIC_SUPPLY_CATEGORY) == dst) return true;
				switch (dst & PlugType::METERING_ELECTRIC_SUPPLY_CATEGORY) {
				case PlugType::METERING_ELECTRIC_SUPPLY_PEAK:
					return (src & PlugType::METERING_ELECTRIC_SUPPLY_CATEGORY) == PlugType::METERING_ELECTRIC_SUPPLY_PEAK;
				case PlugType::METERING_ELECTRIC_SUPPLY_OFF_PEAK:
					return (src & PlugType::METERING_ELECTRIC_SUPPLY_CATEGORY) == PlugType::METERING_ELECTRIC_SUPPLY_OFF_PEAK;
				default:
					return false;
				}
			default:
				return false;
			}
		case PlugType::METERING_WATER:
			return (src & PlugType::METERING_CATEGORY) == PlugType::METERING_WATER;
		case PlugType::METERING_GAS:
			return (src & PlugType::METERING_CATEGORY) == PlugType::METERING_GAS;
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

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::BINARY:
	case PlugType::TERNARY: {
		// switch -> switch
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		dst.value.u8 = cmd;
		break;
	}
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION: {
		// switch -> float, use values in convertOptions
		dst.value.f = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = cmd;
		}
		break;
	}
	case PlugType::LIGHTING: {
		// switch -> float, use values in convertOptions
		dst.value.f = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & PlugType::CMD) != 0) {
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
	switch (dstType & PlugType::CATEGORY) {
	case PlugType::BINARY:
	case PlugType::TERNARY: {
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
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION:
		dst.value.f = src;
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = 0; // set
		}
		break;
	case PlugType::LIGHTING:
		dst.value.f = src;
		if ((dstType & PlugType::CMD) != 0) {
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
	if ((dstType & PlugType::CMD) == 0)
		return false;

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION:
		dst.value.f = src;
		dst.command = command;
		break;
	case PlugType::LIGHTING:
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
	if ((dstType & PlugType::CMD) == 0)
		return false;

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::LIGHTING:
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

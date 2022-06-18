# message includes a command (set, increment, decrement)
CMD = 1

# sub-types are compatible
COMPATIBLE = 2

# type is compatible to switch input or output
SWITCH_IN = 4
SWITCH_OUT = 8

# type infos (name, flags, label, usage, description)
types = [4,
	["SWITCH", CMD | COMPATIBLE, "Switch", None,"switch category (byte)", [2,
		["BINARY", CMD, "Binary", "OFF_ON", "generic binary switch (0: off, 1: on, 2: toggle command)", [4,
			["ONESHOT", 0, "One-Shot", None, "one state is stable, the other is transient", [2,
				["BUTTON", 0, "Button", "RELEASE_PRESS", "button returns to inactive state when released (0: release, 1: press)"],
				["ALARM", 0, "Alarm", "ACTIVATE", "activation of alarm (0: deactivate, 1: activate)"],
			]],
			["SWITCH", 0, "Switch", "OFF_ON", "mechanical switch such as wall switch (0: off, 1: on)"],
			["ON_OFF", CMD, "On/Off", "OFF_ON", "power on/off (0: off, 1: on, 2: toggle command)"],
			["START_STOP", CMD, "Start/Stop", "STOP_START", "start/stop (0: stop, 1: start, 2: toggle command)"],			
			["VALVE", CMD, "Valve", None, "valve", [2,
				["NO", CMD, "Valve NO", None, "normally open valve (0: open, 1: close)"],
				["NC", CMD, "Valve NC", None, "normally open valve (0: close, 1: open)"],
			]],
			["APERTURE", CMD, "Aperture", "OPEN_CLOSE", "aperture of gate or blind", [2,	
				["DOOR", CMD, "Door", None, "door (0: open, 1: close)"],
				["WINDOW", CMD, "Window", None, "window (0: open, 1: close)"],
			]],
			["OCCUPANCY", 0, "Occupancy", "OCCUPANCY", "occupancy measured by e.g. PIR sensor (0: unoccupied, 1: occupied)"],
			["ENABLE_CLOSE", 0, "Enable Close", "ENABLE", "closing of blind/lock enabled (0: disabled, 1: enabled)"],			
		]],
		["TERNARY", 0, "Ternary", None, "ternary state", [4,		
			["ONESHOT", 0, "One-Shot", None, "one state is stable, all others are transient", [2,
				["ROCKER", 0, "Rocker", "UP_DOWN", "rocker returns to inactive state when released (0: release, 1: up, 2: down)"],
			]],
			["SWITCH", 0, "Switch", "OFF_ON2", "mechanical switch with 3 states (0: off, 1: on1, 2: on2)"],
			["OPEN_CLOSE", 0, "Open/Close", "STOP_OPEN_CLOSE", "open/close/stop of blind etc. (0: stop, 1: open, 2: close)"],
		]],
	]],
	["MULTISTATE", CMD, "Multistate", None, "multi-state category (int)", [2,
		["THERMOSTAT_MODE", 0, "Mode", None, "thermostat mode (0: heat, 1: cool, 2: auto)"],
	]],
	["LEVEL", CMD | SWITCH_IN, "Level", "PERCENTAGE", "level category (float, range 0-1, displayed as 0-100%)", [3,
		["APERTURE", CMD | COMPATIBLE | SWITCH_OUT, "Aperture", None, "aperture of gate or blind", [3,	
			["GATE", CMD, "Gate Position", None, "gate position"],
			["BLIND", CMD, "Blind Position", None, "blind position"],
			["SLAT", CMD, "Slat Position", None, "slat position of venetian blind"],
		]],
		["BATTERY", 0, "Battery Level", None, "battery level"],
		["TANK", 0, "Tank Level", None, "water/fuel tank level"],
	]],
	["PHYSICAL", CMD | SWITCH_IN, "Physical", None, "physical quantities category (float)", [4,
		["TEMPERATURE", CMD, "Temperature", "KELVIN", "process temperature in Kelvin (displayed as Celsius or Fahrenheit)", [2,
			["MEASURED", 0 | COMPATIBLE, "Measured Temp.", None, "measured temperature", [3,
				#["LOCAL", 0, "Local Temp.", "AIR_TEMPERATURE", "measured local temperature"], 
				#["ENVIRONMENT", 0, "Outdoor Temp.", None, "measured environment temperature"],
			
				["FREEZER", 0, "Measured Temp.", "TEMPERATURE_FEEZER", "measured freezer temperature"], 
				["FRIDGE", 0, "Measured Temp.", "TEMPERATURE_FRIDGE", "measured fridge temperature"], 
				["OUTDOOR", 0, "Measured Temp.", "TEMPERATURE_OUTDOOR", "measured outdoor temperature"],
				["ROOM", 0, "Measured Temp.", "TEMPERATURE_ROOM", "measured room temperature"],
				["OVEN", 0, "Measured Temp.", "TEMPERATURE_OVEN", "measured oven temperature"],
			]],
			["SETPOINT", CMD | COMPATIBLE | SWITCH_OUT, "Setpoint Temp.", None, "setpoint temperature", [3,
				#["MIN", CMD, "Min. Temp.", None, "minimum temperature (heating setpoint for hvac)"], 
				#["MAX", CMD, "Max. Temp.", None, "maximum temperature (cooling setpoint for hvac)"],

				["FREEZER", CMD, "Measured Temp.", "TEMPERATURE_FEEZER", "cooling setpoint for freezer (-30oC - 0oC)"], 
				["FRIDGE", CMD, "Measured Temp.", "TEMPERATURE_FRIDGE", "cooling setpoint for fridge (0oC - 20oC)"], 
				["COOLER", CMD, "Measured Temp.", "TEMPERATURE_ROOM", "cooling setpoint for hvac (8oC - 30oC)"],
				["HEATER", CMD, "Measured Temp.", "TEMPERATURE_ROOM", "heating setpoint for hvac (8oC - 30oC)"],
				["OVEN", CMD, "Measured Temp.", "TEMPERATURE_OVEN", "heating setpoint for oven (100oC - 400oC)"],
			]],
		]],
		["PRESSURE", CMD, "Pressure", "PASCAL", "pressure in Pascal (displayed as hPa or kPa)", [2,
			["MEASURED", 0, "Measured Pressure", None, "measured pressure", [2,
				["ATMOSPHERE", 0, "Measured Pressure", "PRESSURE_ATMOSPHERIC", "measured atmospheric pressure"],
			]],
			["SETPOINT", CMD | SWITCH_OUT, "Pressure Setpoint",  None, "pressure setpoint"],
		]],
		# low/high voltage?
		["VOLTAGE", CMD, "Voltage", "VOLT", "voltage", [2,
			["MEASURED", 0, "Measured Voltage", None, "measured voltage", [3,
				["LOW", 0, "Measured Voltage", "VOLTAGE_LOW", "low voltage up to 50V"], 
				["MAINS", 0, "Measured Voltage", "VOLTAGE_MAINS", "mains voltage 110V, 230V, 400V"], 
				["HIGH", 0, "Measured Voltage", "VOLTAGE_HIGH", "high voltage"], 
			]],
			["SETPOINT", CMD | SWITCH_OUT, "Voltage Setpoint", None, "voltage setpoint"],
		]],
		["CURRENT", CMD, "Current", "AMPERE", "current", [2,
			["MEASURED", 0, "Measured Curent", None, "measured current"],
			["SETPOINT", CMD | SWITCH_OUT, "Curent Setpoint", None, "current setpoint"],
		]],
		["POWER", CMD, "Current", "WATT", "current", [2,
			["MEASURED", 0, "Measured Power", None, "measured power"],
			["SETPOINT", CMD | SWITCH_OUT, "Power Setpoint", None, "power setpoint"],
		]],
		["ILLUMINATION", 0, "Illuminatin", None, "illumination"]
	]],
	["CONCENTRATION", CMD | SWITCH_IN, "Concentration", None, "concentration of substance category (float)", [5,
		["RELATIVE_HUMIDITY", CMD, "Humidity", "PERCENT", "relative air humidity in percent", [2,
			["MEASURED", 0, "Measured Humidity", None, "measured relative air humidity", [2,
				["AIR", 0, "Measured Humidity", None, "low voltage up to 50V"], 			
			]],
			["SETPOINT", CMD | SWITCH_OUT, "Setpoint Temp.", None, "setpoint relative air humidity"],		
		]],
		["VOC", 0, "Volatile Organic", None, "volatile organic components"], 
		["CARBON_MONOXIDE", 0, "Carbon Monox.", None, "carbon monoxide"], 
		["CARBON_DIOXIDE", 0, "Carbon Diox.", None, "carbon dioxide"], 	
	]],
	["LIGHTING", CMD | SWITCH_IN | SWITCH_OUT, "Lighting", None, "lighting category (float, commands include a transition time)", [4,
		# https://www.waveformlighting.com/tech/calculate-color-temperature-cct-from-cie-1931-xy-coordinates
		# https://ninedegreesbelow.com/photography/xyz-rgb.html#xyY
		["BRIGHTNESS", CMD, "Brightness", "PERCENTAGE", "light brightness"],
		["COLOR_TEMPERATURE", CMD, "Color Temp.", "TEMPERATURE_COLOR", "color temperature in Kelvin"],
		["COLOR_PARAMETER", CMD | COMPATIBLE, "Color Parameter", "UNIT_INTERVAL", "color parameter, commands include a transition time", [3,
			["CHROMATICITY_X", CMD, "Chromaticity x", None, "color chromaticity x (CIE xyY model)"],
			["CHROMATICITY_Y", CMD, "Chromaticity y", None, "color chromaticity y (CIE xyY model)"],
			["HUE", CMD, "Hue", None, "color hue"],
			["SATURATION", CMD, "Saturation", None, "color saturation"],
		]],
	]],
	["METERING", 0, "Metering", None, "metering of elecricity, water, gas etc. category (uint)", [4,
		["ELECTRIC", 0, "Electric Meter", "WATT_HOURS", "electric meter", [2,
			["USAGE", 0, "Energy Usage", None, "usage (OBIS 1.8.0)", [2,
				["PEAK", 0, "Peak Usage", None, "usage during peak hours (OBIS 1.8.1)"],			
				["OFF_PEAK", 0, "Off-Peak Usage", None, "usage during off peak hours (OBIS 1.8.2)"],
			]],
			["SUPPLY", 0, "Supply", None, "supply (OBIS 2.8.0)", [2,
				["PEAK", 0, "Peak Supply", None, "supply during peak hours (OBIS 2.8.1)"],			
				["OFF_PEAK", 0, "Off-Peak Supply", None, "supply during off-peak hours (OBIS 2.8.2)"],
			]],
		]],
		["WATER", 0, "Water Meter", None, "water meter"],
		["GAS", 0, "Gas Meter", None, "gas meter"],
	]],
]


def EndpointType(types, totalBitCount, bitIndex, tabs, parentPrefix, prefix):
	bitCount = types[0]
	bitIndex -= bitCount
	if bitIndex < 0:
		print(f"error: not enough bits for {prefix}CATEGORY")
		exit()
	mask = 2**(totalBitCount - bitIndex) - 1
	print(f"{tabs}{prefix}CATEGORY = {mask} << {bitIndex},")
	
	for i in range(1, len(types)):
		type = types[i]
		if i == 2**bitCount:
			print(f"error: overflow of {prefix}CATEGORY")
			exit()
			
		name = type[0]
		flags = type[1]
		description = type[4]
		print(f"{tabs}// {description}")
		print(f"{tabs}{prefix}{name} = {parentPrefix}{i} << {bitIndex},")
		print(f"{tabs}{prefix}{name}_IN = {prefix}{name} | IN,")
		print(f"{tabs}{prefix}{name}_OUT = {prefix}{name} | OUT,")
		if flags & CMD:
			print(f"{tabs}{prefix}{name}_CMD_IN = {prefix}{name}_IN | CMD,")
			print(f"{tabs}{prefix}{name}_CMD_OUT = {prefix}{name}_OUT | CMD,")
		if len(type) >= 6:
			EndpointType(type[5], totalBitCount, bitIndex, tabs + '\t', f"{prefix}{name} | ", f"{prefix}{name}_")
		

def getLabel(types, tabs, prefix, default):
	print(f"{tabs}switch (type & EndpointType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		label = type[2]
		print(f"{tabs}case EndpointType::{prefix}{name}:")
		if len(type) >= 6:
			getLabel(type[5], tabs + '\t', prefix + name + '_', f"\"{label}\"")
		else:
			print(f"{tabs}\treturn \"{label}\";")		
	print(f"{tabs}default:")
	print(f"{tabs}\treturn {default};")
	print(f"{tabs}}}")		


def hasSwitchIn(types):
	for i in range(1, len(types)):
		type = types[i]
		flags = type[1]
		if flags & SWITCH_IN:
			return True
		if len(type) >= 6 and hasSwitchIn(type[5]):
			return True
	return False


def isSwitchIn(types, tabs, prefix):
	print(f"{tabs}switch (src & EndpointType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]		
		name = type[0]
		flags = type[1]
		if flags & SWITCH_IN or (len(type) >= 6 and hasSwitchIn(type[5])):
			print(f"{tabs}case EndpointType::{prefix}{name}:")
			if flags & SWITCH_IN:
				print(f"{tabs}\treturn true;")
			elif len(type) >= 6:
				isSwitchIn(type[5], tabs + '\t', prefix + name + '_')
					
	print(f"{tabs}default:;")
	print(f"{tabs}}}")


def isCompatible(types, tabs, prefix):
	print(f"{tabs}if ((src & EndpointType::{prefix}CATEGORY) == dst) return true;")
	print(f"{tabs}switch (dst & EndpointType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		flags = type[1]
		print(f"{tabs}case EndpointType::{prefix}{name}:")
		if f"{prefix}{name}" == "SWITCH":
			# dst is switch input, check if src is compatible to switch input
			print(f"{tabs}\tif (!srcCmd) {{")
			isSwitchIn(types, tabs + "\t\t", prefix)
			print(f"{tabs}\t}}")
		if flags & SWITCH_OUT:
			# dst is compatible to switch output, check if src is switch output
			print(f"{tabs}\tif ((src & EndpointType::CATEGORY) == EndpointType::SWITCH) return true;")				
		if flags & COMPATIBLE:
			# check if subtypes are compatible to their parent type
			print(f"{tabs}\treturn (src & EndpointType::{prefix}CATEGORY) == EndpointType::{prefix}{name};")	
		elif len(type) < 6:
			# leaf type
			print(f"{tabs}\treturn src == EndpointType::{prefix}{name};")				
		else:
			isCompatible(type[5], tabs + '\t', prefix + name + '_')	
	print(f"{tabs}default:")
	print(f"{tabs}\treturn false;")
	print(f"{tabs}}}")


def collectUsages(types, usages):
	for i in range(1, len(types)):
		type = types[i]
		usage = type[3]
		if usage:
			usages.add(usage)
		if len(type) >= 6:
			collectUsages(type[5], usages)


def hasUsage(types):
	for i in range(1, len(types)):
		type = types[i]
		usage = type[3]
		if usage:
			return True
		if len(type) >= 6 and hasUsage(type[5]):
			return True
	return False
	
	
def getUsage(types, tabs, prefix):
	print(f"{tabs}switch (t & EndpointType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		usage = type[3]
		print(f"{tabs}case EndpointType::{prefix}{name}:")
		if len(type) < 6 or not hasUsage(type[5]):
			# leaf usage
			if not usage:
				usage = "NONE"
			print(f"{tabs}\treturn Usage::{usage};")				
		else:
			if usage:
				print(f"{tabs}\tif (t == EndpointType::{prefix}{name}) return Usage::{usage};")
			getUsage(type[5], tabs + '\t', prefix + name + '_')	
	print(f"{tabs}default:")
	print(f"{tabs}\treturn Usage::NONE;")
	print(f"{tabs}}}")
	

print("""
enum class EndpointType : uint16_t {
	// direction
	IN = 2 << 14,
	OUT = 1 << 14,
	DIRECTION_MASK = 3 << 14,
	
	// explicit command (set, increase, decrease)
	CMD = 1 << 13,
	
	// type
	TYPE_MASK = 0x1fff,	
	UNKNOWN = 0,""")
EndpointType(types, 13, 13, "\t", "", "")
print("""};
FLAGS_ENUM(EndpointType)
""")


print("String getTypeLabel(EndpointType type) {")
getLabel(types, "\t", "", "{}")
print("}")


print("""
bool isCompatible(EndpointType dstType, EndpointType srcType) {
	bool srcCmd = (srcType & EndpointType::CMD) != 0;
	if (srcCmd && (dstType & EndpointType::CMD) == 0)
		return false;
	if ((srcType & EndpointType::DIRECTION_MASK) != EndpointType::OUT || (dstType & EndpointType::DIRECTION_MASK) != EndpointType::IN)
		return false;
	auto src = srcType & EndpointType::TYPE_MASK;
	auto dst = dstType & EndpointType::TYPE_MASK;
""")
isCompatible(types, "\t", "")
print("}")


usages = {"NONE"}
collectUsages(types, usages)
print("enum class Usages : uint8_t {")
for usage in sorted(usages):
	print(f"\t{usage},")
print("};")


print("""
Usage getUsage(EndpointType type) {
	auto t = type & EndpointType::TYPE_MASK;
""")
getUsage(types, "\t", "")
print("}")

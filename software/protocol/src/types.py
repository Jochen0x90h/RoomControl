import sys

# message includes a command (set, increment, decrement)
CMD = 1

# additional toggle usage
TOGGLE = 2

# sub-types are compatible
COMPATIBLE = 4


# type infos (name, flags, label, usage, description)
types = [4,
	["BINARY", CMD | COMPATIBLE, "Binary", "OFF_ON", "binary category (byte)", [4,
		["BUTTON", 0, "Button", "RELEASED_PRESSED", "button returns to inactive state when released (0: released, 1: pressed)", [3,
			["WALL", 0, "Wall Button", None, "mechanical button mounted on the wall"],
		]],		
		["SWITCH", 0, "Switch", None, "switch with two stable states (0: off, 1: on)", [3,	
			["WALL", 0, "Wall Switch", None, "mechanical switch mounted on the wall"],
		]],
		["POWER", CMD | TOGGLE, "On/Off", None, "power on/off (0: off, 1: on, 2: toggle command)", [4,
			["LIGHT", CMD, "Light On", None, "light"],		
			["FREEZER", CMD, "Freezer On", None, "freezer on"],		
			["FRIDGE", CMD, "Fridge On", None, "fridge on"],		
			["AC", CMD, "AC On", None, "air conditioning on"],		
			["OVEN", CMD, "Oven On", None, "oven or toaster on"],		
			["COOKER", CMD, "Cooker On", None, "hearth, kettle, egg cooker on"],		
			["COFFEE", CMD, "Coffee M. On", None, "coffee machine on"],		
			["DISHWASHER", CMD, "Dishwasher On", None, "dishwasher on"],		
			["WASHING", CMD, "Washing M. On", None, "washing machine on"],		
			["HIFI", CMD, "Hi-Fi On", None, "hi-fi system on"],		
			["TV", CMD, "TV On", None, "television on"],		
		]],
		["OPEN", CMD | TOGGLE, "Open State", "CLOSED_OPEN", "open state of gate, door, window, blind, slat or valve (0: closed, 1: open, 2: toggle command)", [4,	
			["GATE", CMD, "Gate State", None, "gate"],
			["DOOR", CMD, "Door State", None, "door"],
			["WINDOW", CMD, "Window State", None, "window"],
			["BLIND", CMD, "Blind State", None, "blind"],
			["SLAT", CMD, "Slat State", None, "slat"],
			["VALVE", CMD, "Valve State", None, "valve"],
		]],
		["LOCK", CMD | TOGGLE, "Lock State", "LOCK", "lock state of gate, door, window, (0: unlocked, 1: locked, 2: toggle command)", [3,	
			["GATE", CMD, "Gate Lock", None, "gate"],
			["DOOR", CMD, "Door Lock", None, "door"],
			["WINDOW", CMD, "Window Lock", None, "window"],
		]],
		["OCCUPANCY", 0, "Occupancy", "OCCUPANCY", "occupancy measured by e.g. PIR sensor (0: unoccupied, 1: occupied)"],
		["ALARM", 0, "Alarm", "ACTIVATION", "activation of alarm (0: deactivated, 1: activated)"],
		#["START_STOP", CMD | TOGGLE, "Start/Stop", "STOP_START", "start/stop (0: stop, 1: start, 2: toggle command)"],
		["ENABLE_CLOSE", 0, "Enable Close", "ENABLED", "closing of blind/lock enabled (0: disabled, 1: enabled)"],
	]],
	["TERNARY", CMD | COMPATIBLE, "Ternary", "OFF_ON1_ON2", "ternary category (byte)", [4,
		["BUTTON", 0, "Up/Down Button", "RELEASED_UP_DOWN", "button returns to inactive state when released (0: release, 1: up, 2: down)", [3,
			["WALL", 0, "Wall Up/Down Button", None, "mechanical button mounted on the wall"],
		]],		
		["SWITCH", 0, "Switch", None, "switch with three stable states (0: off, 1: on1, 2: on2)", [3,	
			["WALL", 0, "Wall Switch", None, "mechanical switch mounted on the wall"],
		]],
		["OPENING", 0, "Opening Drive", "STOPPED_OPENING_CLOSING", "opening drive of gate, door, window, blind, slat, valve etc. (0: stopped, 1: opening, 2: closing)", [4,
			["GATE", 0, "Gate Drive", None, "gate drive"],
			["DOOR", 0, "Door Drive", None, "door drive"],
			["WINDOW", 0, "Window Drive", None, "window drive"],
			["BLIND", 0, "Blind Drive", None, "blind drive"],
			["SLAT", 0, "Slat Drive", None, "slat drive"],
			["VALVE", 0, "Valve Drive", None, "valve drive"],
		]],	
		["LOCK", CMD, "Lock State", "TILT_LOCK", "lock state of door, window, (0: unlocked, 1: tilt, 2: locked)", [3,	
			["DOOR", CMD, "Door Lock", None, "door"],
			["WINDOW", CMD, "Window Lock", None, "window"],
		]],			
	]],	
	["MULTISTATE", CMD, "Multistate", None, "multi-state category (int)", [2,
		["THERMOSTAT_MODE", 0, "Mode", None, "thermostat mode (0: heat, 1: cool, 2: auto)"],
	]],
	["LEVEL", CMD, "Level", "PERCENT", "level category (float, range 0-1, displayed as 0-100%)", [3,
		["OPEN", CMD | COMPATIBLE, "Open Level", None, "open level of gate, door, window, blind, slat or valve", [3,	
			["GATE", CMD, "Gate Level", None, "gate level"],
			["DOOR", CMD, "Gate Level", None, "gate level"],
			["WINDOW", CMD, "Gate Level", None, "gate level"],
			["BLIND", CMD, "Blind Level", None, "blind level"],
			["SLAT", CMD, "Slat Level", None, "slat level (e.g. slat of venetian blind)"],
			["VALVE", CMD, "Valve Level", None, "valve level"],
		]],
		["BATTERY", 0, "Battery Level", None, "battery level"],
		["TANK", 0, "Tank Level", None, "water/fuel tank level"],
	]],
	["PHYSICAL", CMD, "Physical", None, "physical quantities category (float)", [4,
		["TEMPERATURE", CMD, "Temperature", "TEMPERATURE", "process temperature in Kelvin (displayed as Celsius or Fahrenheit)", [2,
			["MEASURED", 0 | COMPATIBLE, "Measured Temp.", None, "measured temperature", [3,
				["FREEZER", 0, "Measured Temp.", "TEMPERATURE_FREEZER", "measured freezer temperature"], 
				["FRIDGE", 0, "Measured Temp.", "TEMPERATURE_FRIDGE", "measured fridge temperature"], 
				["OUTDOOR", 0, "Measured Temp.", "TEMPERATURE_OUTDOOR", "measured outdoor temperature"],
				["ROOM", 0, "Measured Temp.", "TEMPERATURE_ROOM", "measured room temperature"],
				["OVEN", 0, "Measured Temp.", "TEMPERATURE_OVEN", "measured oven temperature"],
			]],
			["SETPOINT", CMD | COMPATIBLE, "Setpoint Temp.", None, "setpoint temperature", [3,
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
			["SETPOINT", CMD, "Pressure Setpoint",  None, "pressure setpoint"],
		]],
		["VOLTAGE", CMD, "Voltage", "VOLTAGE", "electrical voltage", [2,
			["MEASURED", 0, "Measured Voltage", None, "measured voltage", [3,
				["LOW", 0, "Measured Voltage", "VOLTAGE_LOW", "low voltage up to 50V"], 
				["MAINS", 0, "Measured Voltage", "VOLTAGE_MAINS", "mains voltage 110V, 230V, 400V"], 
				["HIGH", 0, "Measured Voltage", "VOLTAGE_HIGH", "high voltage"], 
			]],
			["SETPOINT", CMD, "Voltage Setpoint", None, "voltage setpoint"],
		]],
		["CURRENT", CMD, "Current", "AMPERE", "electrical current", [2,
			["MEASURED", 0, "Measured Curent", None, "measured current"],
			["SETPOINT", CMD, "Curent Setpoint", None, "current setpoint"],
		]],
		["POWER", CMD, "Current", "WATT", "power", [2,
			["MEASURED", 0, "Measured Power", None, "measured power"],
			["SETPOINT", CMD, "Power Setpoint", None, "power setpoint"],
		]],
		["ILLUMINANCE", 0, "Illuminance", "LUX", "illuminance"]
	]],
	["CONCENTRATION", CMD, "Concentration", None, "concentration of substance category (float)", [5,
		["RELATIVE_HUMIDITY", CMD, "Humidity", "PERCENT", "relative humidity in percent", [2,
			["MEASURED", 0, "Measured Humidity", None, "measured relative humidity", [2,
				["AIR", 0, "Measured Humidity", None, "measured relative air humidity"], 			
			]],
			["SETPOINT", CMD, "Setpoint Temp.", None, "setpoint relative humidity"],		
		]],
		["VOC", 0, "Volatile Organic", None, "volatile organic components"], 
		["CARBON_MONOXIDE", 0, "Carbon Monox.", None, "carbon monoxide"], 
		["CARBON_DIOXIDE", 0, "Carbon Diox.", None, "carbon dioxide"], 	
	]],
	["LIGHTING", CMD, "Lighting", None, "lighting category (float, commands include a transition time)", [4,
		# https://www.waveformlighting.com/tech/calculate-color-temperature-cct-from-cie-1931-xy-coordinates
		# https://ninedegreesbelow.com/photography/xyz-rgb.html#xyY
		["BRIGHTNESS", CMD, "Brightness", "PERCENT", "light brightness"],
		["COLOR_TEMPERATURE", CMD, "Color Temp.", "TEMPERATURE_COLOR", "color temperature in Kelvin"],
		["COLOR_PARAMETER", CMD | COMPATIBLE, "Color Parameter", "UNIT_INTERVAL", "color parameter, commands include a transition time", [3,
			["CHROMATICITY_X", CMD, "Color X", None, "color chromaticity x (CIE xyY model)"],
			["CHROMATICITY_Y", CMD, "Color Y", None, "color chromaticity y (CIE xyY model)"],
			["HUE", CMD, "Hue", None, "color hue"],
			["SATURATION", CMD, "Saturation", None, "color saturation"],
		]],
	]],
	["METERING", 0, "Metering", "COUNTER", "metering of elecricity, water, gas etc. category (uint)", [4,
		["ELECTRIC", 0, "Electric Meter", "ELECTRIC_METER", "electric meter", [2,
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


def PlugType(types, totalBitCount, bitIndex, tabs, parentPrefix, prefix):
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
			PlugType(type[5], totalBitCount, bitIndex, tabs + '\t', f"{prefix}{name} | ", f"{prefix}{name}_")
		

def getLabel(types, tabs, prefix, defaultLabel):
	print(f"{tabs}switch (type & PlugType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		label = type[2]
		print(f"{tabs}case PlugType::{prefix}{name}:")
		if len(type) >= 6:
			getLabel(type[5], tabs + '\t', prefix + name + '_', f"\"{label}\"")
		else:
			print(f"{tabs}\treturn \"{label}\";")		
	print(f"{tabs}default:")
	print(f"{tabs}\treturn {defaultLabel};")
	print(f"{tabs}}}")		


def collectUsages(types, usages, defaultUsage):
	for i in range(1, len(types)):
		type = types[i]
		flags = type[1]
		usage = type[3]
		if not usage:
			usage = defaultUsage
		usages.add(usage)
		if flags & TOGGLE:
			usages.add(f"{usage}_TOGGLE")
		if len(type) >= 6:
			collectUsages(type[5], usages, usage)


def hasUsage(types):
	for i in range(1, len(types)):
		type = types[i]
		usage = type[3]
		if usage:
			return True
		if len(type) >= 6 and hasUsage(type[5]):
			return True
	return False
	
	
def getUsage(types, tabs, prefix, defaultUsage):
	print(f"{tabs}switch (t & PlugType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		flags = type[1]
		usage = type[3]
		if not usage:
			usage = defaultUsage
		print(f"{tabs}case PlugType::{prefix}{name}:")
		if len(type) < 6 or not hasUsage(type[5]):
			# leaf usage
			if flags & TOGGLE:
				print(f"{tabs}\treturn cmd ? Usage::{usage}_TOGGLE : Usage::{usage};")
			else:
				print(f"{tabs}\treturn Usage::{usage};")
		else:
			#print(f"{tabs}\tif (t == PlugType::{prefix}{name}) return Usage::{usage};")
			getUsage(type[5], tabs + '\t', prefix + name + '_', usage)	
	print(f"{tabs}default:")
	print(f"{tabs}\treturn Usage::{defaultUsage};")
	print(f"{tabs}}}")


def isCompatible(types, tabs, prefix):
	print(f"{tabs}if ((src & PlugType::{prefix}CATEGORY) == dst) return true;")
	print(f"{tabs}switch (dst & PlugType::{prefix}CATEGORY) {{")
	for i in range(1, len(types)):
		type = types[i]
		name = type[0]
		flags = type[1]
		print(f"{tabs}case PlugType::{prefix}{name}:")
		if flags & COMPATIBLE or len(type) < 6:
			# subtypes are compatible to this type
			print(f"{tabs}\treturn (src & PlugType::{prefix}CATEGORY) == PlugType::{prefix}{name};")
		else:
			isCompatible(type[5], tabs + '\t', prefix + name + '_')	
	print(f"{tabs}default:")
	print(f"{tabs}\treturn false;")
	print(f"{tabs}}}")



# enum PlugType
# -----------------

f = open('endpointType.txt', 'w')
sys.stdout = f

print("""
enum class PlugType : uint16_t {
	// direction
	IN = 2 << 14,
	OUT = 1 << 14,
	DIRECTION_MASK = 3 << 14,
	
	// explicit command (set, increase, decrease)
	CMD = 1 << 13,
	
	// type
	TYPE_MASK = 0x1fff,	
	UNKNOWN = 0,""")
PlugType(types, 13, 13, "\t", "", "")
print("""};
FLAGS_ENUM(PlugType)
""")



# enum Usage
# ----------

f = open('usage.txt', 'w')
sys.stdout = f

usages = {"NONE"}
collectUsages(types, usages, "NONE")
print("enum class Usage : uint8_t {")
for usage in sorted(usages):
	print(f"\t{usage},")
print("};")



# functions
# ---------
f = open('functions.txt', 'w')
sys.stdout = f

# getTypeLabel()
print("String getTypeLabel(PlugType type) {")
getLabel(types, "\t", "", "{}")
print("}")

# getUsage()
print("""
Usage getUsage(PlugType type) {
	bool cmd = (type & PlugType::CMD) != 0;
	auto t = type & PlugType::TYPE_MASK;
""")
getUsage(types, "\t", "", "NONE")
print("}")

# isCompatible()
print("""
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
""")
isCompatible(types, "\t", "")
print("}")

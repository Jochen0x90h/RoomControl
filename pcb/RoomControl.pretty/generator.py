#!/usr/bin/env python3
import sys
from math import sin, cos

stdout = None

# shapes
CIRCLE = 0.5 # oval if width not equal to height
ROUNDRECT = 0.25 # 25% (KiCad default)
ROUNDRECT10 = 0.1 # 10%
ROUNDRECT5 = 0.05 # 5%
RECT = 0

# line widht of user drawings
userWidth = 0.1
userLayer = "Dwgs.User"

# line width of silkscreen drawings and distance from pads
silkScreenWidth = 0.15
silkScreenDistance = 0.2

# redirect sys.stdout to file and output footprint header
def header(name, top, maskMargin = 0, pasteMargin = 0, description = "", model = ""):
	print(f"{name}")
	
	# open a file for the footprint and redirect stdout to it
	global stdout
	stdout = sys.stdout
	sys.stdout = open(f"{name}.kicad_mod", 'w')
	
	print(f"(module {name} (layer F.Cu) (tedit 5EC043C1)")
	print(f"  (fp_text reference REF** (at 0 {top-2}) (layer F.SilkS) (effects (font (size 1 1) (thickness 0.15))))")
	print(f"  (fp_text value {name} (at 0 {top-0.8}) (layer F.Fab) (effects (font (size 1 1) (thickness 0.15))))")
	if maskMargin != 0:
		print(f"  (solder_mask_margin {maskMargin})")
	if pasteMargin != 0:
		print(f"  (solder_paste_margin {pasteMargin})")
	if description != "":
		print(f"  (descr \"{description}\")")
	if model != "":
		print(f"  (model {model} (at (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))")

# output footprint footer and restore sys.stdout
def footer():
	print(f")")
	
	# restore stdout
	sys.stdout.close()
	sys.stdout = stdout

# draw a line
def line(x1, y1, x2, y2, width, layer):
	print(f"  (fp_line (start {x1:.5g} {y1:.5g}) (end {x2:.5g} {y2:.5g}) (width {width}) (layer {layer}))")

# draw a circle
def circle(x, y, r, width, layer):
	print(f"  (fp_circle (center {x:.5g} {y:.5g}) (end {x+r:.5g} {y:.5g}) (width {width}) (layer {layer}))")

# draw a package rectangle with pin 1 marking to fabrication layer
def fabRect(x1, y1, x2, y2):
	d = min(abs(x2 - x1), abs(y2 - y1)) * 0.2
	x = x1 + (d if x2 > x1 else -d)
	y = y1 + (d if y2 > y1 else -d)
	line(x1, y, x, y1, 0.1, "F.Fab")
	line(x, y1, x2, y1, 0.1, "F.Fab")
	line(x2, y1, x2, y2, 0.1, "F.Fab")
	line(x2, y2, x1, y2, 0.1, "F.Fab")
	line(x1, y2, x1, y, 0.1, "F.Fab")

# draw a rectangle to courtyard layer
def courtyardRect(x1, y1, x2, y2):
	line(x1, y1, x2, y1, 0.05, "F.CrtYd")
	line(x2, y1, x2, y2, 0.05, "F.CrtYd")
	line(x2, y2, x1, y2, 0.05, "F.CrtYd")
	line(x1, y2, x1, y1, 0.05, "F.CrtYd")

# draw a rectangle with pin 1 marking to silkscreen layer
def silkScreenRect(x1, y1, x2, y2):
	d = 4 * silkScreenWidth
	x = x1 + (d if x2 > x1 else -d)
	y = y1 + (d if y2 > y1 else -d)
	
	# pin 1 marking
	#circle(x1, y1, silkScreenWidth, silkScreenWidth * 1.5, "F.SilkS")
	line(x1, y1, x1, y1, silkScreenWidth * 2, "F.SilkS")
	
	# remaining rectangle
	line(x, y1, x2, y1, silkScreenWidth, "F.SilkS")
	line(x2, y1, x2, y2, silkScreenWidth, "F.SilkS")
	line(x2, y2, x1, y2, silkScreenWidth, "F.SilkS")
	line(x1, y2, x1, y, silkScreenWidth, "F.SilkS")	

# draw a rectangle to user layer
def userRect(x1, y1, x2, y2):
	line(x1, y1, x2, y1, userWidth, userLayer)
	line(x2, y1, x2, y2, userWidth, userLayer)
	line(x2, y2, x1, y2, userWidth, userLayer)
	line(x1, y2, x1, y1, userWidth, userLayer)

# draw a line strip (open polygon) to user layer
def userLines(coords):
	for i in range(0, len(coords) - 1):
		line(*coords[i], *coords[i + 1], userWidth, userLayer)

# draw a closed polygon to user layer
def userPoly(coords):
	for i in range(0, len(coords)):
		line(*coords[i], *coords[(i + 1) % len(coords)], userWidth, userLayer)

# define a pad
def pad(index, kind, shape, x, y, width, height, drill = 0, offsetX = 0, offsetY = 0, clearance = 0, layers = ""):
	print(f"  (pad {index} {kind}", end = '')
	if shape <= RECT:
		print(f" rect", end = '')
	elif shape >= CIRCLE:
		if width == height:
			print(f" circle", end = '')
		else:
			print(f" oval", end = '')
	elif shape == ROUNDRECT:
		print(f" roundrect", end = '')
	else:
		print(f" roundrect (roundrect_rratio {shape})", end = '')
	print(f" (at {x:.5g} {y:.5g}) (size {width:.5g} {height:.5g})", end = '')
	if drill != 0:
		if type(drill) == tuple:
			(x, y) = drill 
			print(f" (drill oval {x:.5g} {y:.5g}", end = '')
		else:
			print(f" (drill {drill:.5g}", end = '')
		if offsetX != 0 or offsetY != 0:	
			print(f" (offset {offsetX:.5g} {offsetY:.5g})", end = '')
		print(f")", end = '')
	if clearance > 0:
		print(f" (clearance {clearance})", end = '')
	print(f" (layers {layers}))")

# define a through-hole pad
def thruHolePad(index, shape, x, y, width, height, drill, offsetX = 0, offsetY = 0, clearance = 0, layers = "*.Cu *.Mask"):
	pad(index, "thru_hole", shape, x, y, width, height, drill, offsetX, offsetY, clearance, layers)

# define a smd pad
def smdPad(index, shape, x, y, width, height, clearance = 0, layers = "F.Cu F.Mask F.Paste"):
	pad(index, "smd", shape, x, y, width, height, clearance=clearance, layers=layers)

# define a smd exposed pad
def exposedPad(index, shape, x, y, width, height):
	pad(index, "smd", shape, x, y, width, height, layers="F.Cu F.Mask")
	nx = int(round(width / 1.25))
	ny = int(round(height / 1.25))
	w = width/nx
	h = height/ny
	for j in range(0, ny):
		for i in range(0, nx):
			pad('""', "smd", 0.25, x+(i-(nx-1)*0.5)*w, y+(j-(ny-1)*0.5)*h, w-0.3, h-0.3, layers="F.Paste")

# define a non-plated through-hole pad
def npthPad(x, y, width, height, clearance = 0, maskMargin = 0):
	print('  (pad "" np_thru_hole ', end = '')
	if width == height:
		print(f" circle (at {x:.5g} {y:.5g}) (size {width:.5g} {height:.5g}) (drill {width:.5g})", end = '')
	else:
		print(f" oval (at {x:.5g} {y:.5g}) (size {width:.5g} {height:.5g}) (drill oval {width:.5g} {height:.5g})", end = '')
	if clearance > 0:
		print(f" (clearance {clearance})", end = '')
	if maskMargin > 0:
		print(f" (solder_mask_margin {maskMargin})", end = '')
	print(f" (layers *.Cu *.Mask))")




# define an smd dual in-line footprint
# name: part name
# packageWidth, packageHeight: package size
# count: pin count of one side
# shape: shape of pads
# distance: distance of the pad rows, negative for clock-wise pin numbering
# pitch: distance between pads
# padWidth, padHeight: size of pad
# maskMargin: margin of solder mask
# pasteMargin: margin of paste stencil
# description: part description
# model: path of 3d model
def smdDilFootprint(name, packageWidth, packageHeight, count, shape, distance, pitch, padWidth, padHeight,
		maskMargin = 0, pasteMargin = 0, description = "", model = ""):
	center = (count - 1) / 2

	# package
	packageRight = packageWidth / 2
	packageLeft = -packageRight
	packageBottom = packageHeight / 2
	packageTop = -packageBottom
	
	# pads
	rightPads = abs(distance / 2)
	rightPadsLeft = rightPads - padWidth / 2
	rightPadsRight = rightPads + padWidth / 2
	padsBottom = (count - 1 - center) * pitch + padHeight / 2

	# courtyard encloses package and pads
	courtyardRight = max(packageRight, rightPadsRight + 0.1)
	courtyardLeft = -courtyardRight
	
	# don't print silkscreen onto pads
	d = silkScreenDistance + silkScreenWidth / 2
	silkScreenRight = min(packageRight, rightPadsLeft - d) if packageRight < rightPads \
		else max(packageRight, rightPadsRight + d) 
	if distance < 0:
		silkScreenRight = -silkScreenRight
	silkScreenLeft = -silkScreenRight
	silkScreenBottom = max(packageBottom, padsBottom + d)
	silkScreenTop = -silkScreenBottom
	
	header(name, packageTop, maskMargin, pasteMargin, description, model)
	fabRect(packageLeft, packageTop, packageRight, packageBottom)
	courtyardRect(courtyardLeft, packageTop, courtyardRight, packageBottom)
	silkScreenRect(silkScreenLeft, silkScreenTop, silkScreenRight, silkScreenBottom)

	for i in range(0, count):
		smdPad(1 + i, shape, -distance / 2, (i - center) * pitch, padWidth, padHeight)
		smdPad(count * 2 - i, shape, distance / 2, (i - center) * pitch, padWidth, padHeight)

# define an smd quad flat package footprint
# name: part name
# packageSize: package size
# count: pin count of one side
# shape: shape of pads
# distance: distance of the pad rows
# pitch: distance between pads
# padWidth, padHeight: size of pad
# maskMargin: margin of solder mask
# pasteMargin: margin of paste stencil
# description: part description
# model: path of 3d model
def smdQfpFootprint(name, packageSize, count, shape, distance, pitch, padWidth, padHeight,
		maskMargin = 0, pasteMargin = 0, description = "", model = ""):
	center = (count - 1) / 2

	# package
	packageRight = packageSize / 2
	packageLeft = -packageRight
	packageBottom = packageSize / 2
	packageTop = -packageBottom
	
	# pads
	rightPads = distance / 2
	rightPadsLeft = rightPads - padWidth / 2
	rightPadsRight = rightPads + padWidth / 2
	bottomPads = distance / 2
	bottomPadsTop = bottomPads - padWidth / 2
	bottomPadsBottom = bottomPads + padWidth / 2

	# courtyard encloses package and pads
	courtyardRight = max(packageRight, rightPadsRight + 0.1)
	courtyardLeft = -courtyardRight
	courtyardBottom = max(packageBottom, bottomPadsBottom + 0.1)
	courtyardTop = -courtyardBottom
	
	# don't print silkscreen onto pads
	d = silkScreenDistance + silkScreenWidth / 2
	silkScreenRight = min(packageRight, rightPadsLeft - d) if packageRight < rightPads \
		else max(packageRight, rightPadsRight + d) 
	silkScreenLeft = -silkScreenRight
	silkScreenBottom = min(packageBottom, bottomPadsTop - d) if packageBottom < bottomPads \
		else max(packageBottom, bottomPadsBottom + d) 
	silkScreenTop = -silkScreenBottom
	
	header(name, packageTop, maskMargin, pasteMargin, description, model)
	fabRect(packageLeft, packageTop, packageRight, packageBottom)
	courtyardRect(courtyardLeft, courtyardTop, courtyardRight, courtyardBottom)
	silkScreenRect(silkScreenLeft, silkScreenTop, silkScreenRight, silkScreenBottom)

	for i in range(0, count):
		smdPad(1 + i, shape, -distance / 2, (i - center) * pitch, padWidth, padHeight)
		smdPad(count + 1 + i, shape, (i - center) * pitch, distance / 2, padHeight, padWidth)
		smdPad(count * 3 - i, shape, distance / 2, (i - center) * pitch, padWidth, padHeight)
		smdPad(count * 4 - i, shape, (i - center) * pitch, -distance / 2, padHeight, padWidth)



#
# part definitions
#

# E73-2G4M08S1C-52840

# bunding box
right = 13.0 / 2.0
left = -right
top = -(1.27 * 4.5 + 2.6)
bottom = 1.27 * 4.5 + 3.97
header("E73-2G4M08S1C-52840", top)
fabRect(right, bottom, left, top)
courtyardRect((left - 0.1), (top - 0.1), (right + 0.1), (bottom + 0.1))

# pads
size = 0.8
outerSize = 1.4
outerOffset = -1.0 / 2 + (outerSize - 1) / 2
bottomSize = 0.6
pitch = 1.27
drill = 0.3

# right
for i in range(0, 10):
	smdPad(1 + i, ROUNDRECT5, (right + outerOffset), (4.5 - i) * pitch, outerSize, size)

# top
for i in range(0, 8):
	index = 11 + i * 2
	smdPad(index, ROUNDRECT5, (3.5 - i) * pitch, (top - outerOffset), size, outerSize)
for i in range(0, 7):
	index = 12 + i * 2
	smdPad(index, ROUNDRECT5, (3 - i) * pitch, (top + 2.1), size, size)
	thruHolePad(index, ROUNDRECT, (3 - i) * pitch, (top + 2.1), bottomSize, size, drill)

# left
for i in range(0, 10):
	index = 26 if i == 0 else 25 + i * 2
	smdPad(index, ROUNDRECT5, (left - outerOffset), (-4.5 + i) * pitch, outerSize, size)
for i in range(0, 8):
	index = 28 + i * 2
	smdPad(index, ROUNDRECT5, (left + 2.1), (-3 + i) * pitch, size, size)
	thruHolePad(index, ROUNDRECT, (left + 2.1), (-3 + i) * pitch, size, bottomSize, drill)
footer()



# wago clamps
for pinCount in [4, 5, 6]:
	# bounding box
	pitch = 5
	right = 6.5
	left = -7.5
	L = pinCount * pitch + 2.3
	bottom = (pinCount - 1) * pitch + 3.5
	top = bottom - L
	header(f"Wago_236-4{pinCount:02d}", top)
	fabRect(left, top, right-1.5, bottom)
	courtyardRect(left, top, right, bottom)
	silkScreenRect(left, top, right, bottom)
		
	# pads
	width = 1.8
	offset = 0.1
	height = 1.8
	drill = 1.1
	for i in range(0, pinCount):
		thruHolePad(1 + i, ROUNDRECT, -2.5, i * pitch, width, height, drill, offsetX=offset)
		thruHolePad(1 + i, ROUNDRECT, 2.5, i * pitch, width, height, drill, offsetX=-offset)
	footer()
	

# Texas TPS82130 buck regulator
smdDilFootprint("TPS82130", 2.9, 3.0, 4, ROUNDRECT10, 2.1, 0.65, 0.50, 0.40,
	description="Texas Instruments buck converter (http://www.ti.com/lit/ds/symlink/tps82130.pdf#page=19)",
	model="${KISYS3DMOD}/Package_LGA.3dshapes/Texas_SIL0008D_MicroSiP-8-1EP_2.8x3mm_P0.65mm_EP1.1x1.9mm.wrl")
exposedPad(9, ROUNDRECT5, 0, 0, 1.1, 1.9)
epDrill = 0.3
for i in range(0, 3):
	thruHolePad(9, CIRCLE, 0, (-1 + i) * 0.75, epDrill + 0.1, epDrill + 0.1, epDrill, layers="*.Cu")
footer()


# ST STM32F042F6 microcontroller
smdDilFootprint("STM32F042F6", 4.20, 6.60, 10, ROUNDRECT, 7.10 - 1.35, 0.65, 1.35, 0.40,
	description="ST Arm Cortex M0 microcontroller (https://www.st.com/resource/en/datasheet/stm32f042f6.pdf)")
footer()


# MPS MPQ6526GU
smdQfpFootprint("MPQ6526GU", 5.00, 6, ROUNDRECT10, 4.90, 0.65, 0.70, 0.40,
	description="MPS hex half bridge driver (https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MPQ6526GR-AEC1/document_id/2180/)")
exposedPad(25, ROUNDRECT5, 0, 0, 3.6, 3.6)
footer()


# ON NCV7718, NCV7720, NCV7726 half bridge driver
#smdDilFootprint("NCV77xx", 3.90, 8.65, 12, ROUNDRECT, 6.40 - 1.15, 0.65, 1.15, 0.40,
#	description="ON half bridge driver (https://www.onsemi.com/pub/Collateral/NCV7718-D.PDF https://www.onsemi.com/pub/Collateral/NCV7719-D.PDF https://www.onsemi.com/pub/Collateral/NCV7720-D.PDF)")
#exposedPad(25, ROUNDRECT5, 0, 0, 2.8, 5.6)
#footer()


# ON NCV7428MW lin driver
smdDilFootprint("NCV7428MW", 3.00, 3.00, 4, ROUNDRECT10, 3.30 - 0.60, 0.65, 0.60, 0.40,
	description="ON LIN driver (https://www.onsemi.com/pub/Collateral/NCV7428-D.PDF)")
exposedPad(9, ROUNDRECT5, 0, 0, 1.60, 2.56)
footer()


# ON MOC3043M opto-triac
smdDilFootprint("MOC3043M", 6.10, 8.13, 3, ROUNDRECT10, (7.49 + 10.54) / 2, 2.54, 1.54, 1.54,
	description="ON opto-triac (https://www.onsemi.com/pub/Collateral/MOC3043M-D.pdf)")
footer()


# Maxim MAX98357A audio driver
smdQfpFootprint("MAX98357A", 3.00, 4, ROUNDRECT10, 3 + (0.30 - 0.40), 0.5, 0.40 + 0.30, 0.25,
	description="Maxim audio driver (https://datasheets.maximintegrated.com/en/ds/MAX98357A-MAX98357B.pdf)")
exposedPad(17, ROUNDRECT5, 0, 0, 1.1, 1.1)
footer()


# Bosch BME680 air sensor, clockwise pin numbering
smdDilFootprint("BME680", 3.0, 3.0, 4, ROUNDRECT10, -2.40, 0.80, 0.45, 0.45,
	description="Bosch Air sensor (https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors-bme680/)")
footer()

# Infineon FM25CL64B FeRam SOIC
smdDilFootprint("FM25CL64B", 3.90, 4.90, 4, ROUNDRECT10, 6.00 - 0.40*2, 1.27, 1.0, 0.6,
	description="Infineon 64k FRAM (https://www.infineon.com/dgdl/Infineon-FM25CL64B_64-Kbit_(8_K_8)_Serial_(SPI)_F-RAM-DataSheet-v11_00-EN.pdf?fileId=8ac78c8c7d0d8da4017d0ebdfcd83119)")
footer()



# Omron relay G6SU-2
padWidth = 1.8
padHeight = 1.4
offset = 0.1
drill = 1
pitch = 2.54
packageRight = 2.54 + 1.11
packageLeft = -2.54 - 1.11
packageTop = -1.05
packageBottom = top + 14.8
header("G6SU-2", top,
	description="Omron relay 2A (http://omronfs.omron.com/en_US/ecb/products/pdf/en-g6s.pdf)",
	model="{KISYS3DMOD}/Relay_THT.3dshapes/Relay_DPDT_Omron_G6S-2.wrl")
fabRect(packageLeft, packageTop, packageRight, packageBottom)
courtyardRect(packageLeft, packageTop, packageRight, packageBottom)
silkScreenRect(packageLeft, packageTop, packageRight, packageBottom)
for i in range(0, 5):
	if i != 1:
		thruHolePad(1 + i, ROUNDRECT, -pitch, i * pitch, padWidth, padHeight, drill, offsetX=offset)
		thruHolePad(12 - i, ROUNDRECT, pitch, i * pitch, padWidth, padHeight, drill, offsetX=-offset)
footer()



# Omron micro switch D2F-01
padWidth = 1.5
padHeight = 2
drill = 1.1
pitch = 5.08
packageRight = 5.8 / 2
packageLeft = -packageRight
packageBottom = 12.8 / 2
packageTop = -packageBottom
header("D2F-01", top,
	description="Omron micro switch (https://omronfs.omron.com/en_US/ecb/products/pdf/en-d2f.pdf)")
fabRect(packageLeft, packageTop, packageRight, packageBottom)
courtyardRect(packageLeft, packageTop, packageRight, packageBottom)
silkScreenRect(packageLeft, packageTop, packageRight, packageBottom)
thruHolePad(3, ROUNDRECT, 0, 0, padWidth, padHeight, drill)
thruHolePad(2, ROUNDRECT, 0, -pitch, padWidth, padHeight, drill)
thruHolePad(1, ROUNDRECT, 0, pitch, padWidth, padHeight, drill)
footer()



# LED/photo diode holes
def diodeHoles(name, pitch, angle):
	padWidth = 1.3
	padHeight = 1.5
	drill = 0.9
	packageRight = pitch / 2 + padWidth / 2
	packageLeft = -packageRight
	packageBottom = padHeight / 2
	packageTop = -packageBottom
	silkScreenRight = packageRight + silkScreenDistance + silkScreenWidth / 2
	silkScreenLeft = -silkScreenRight
	silkScreenBottom = packageBottom + silkScreenDistance + silkScreenWidth / 2
	silkScreenTop = -silkScreenBottom
	header(name, packageTop)
	fabRect(packageLeft, packageTop, packageRight, packageBottom)
	courtyardRect(packageLeft, packageTop, packageRight, packageBottom)
	silkScreenRect(silkScreenLeft, silkScreenTop, silkScreenRight, silkScreenBottom)
	x = pitch/2 * cos(angle);
	y = pitch/2 * sin(angle);
	thruHolePad(1, ROUNDRECT5, -x, -y, padWidth, padHeight, drill)
	thruHolePad(2, ROUNDRECT, x, y, padWidth, padHeight, drill)
	footer()

diodeHoles("DiodeHoles", 2.5, 0)
diodeHoles("DiodeHolesSwitch", 1.8, 0)


# switch mount
def switchMountDrawing(coords):
	# mirror at x/y
	i = len(coords)
	while i > 0:
		i -= 1
		(x, y) = coords[i]
		coords += [(y, x)]
	
	# mirror at y
	i = len(coords)
	while i > 0:
		i -= 1
		(x, y) = coords[i]
		coords += [(-x, y)]
	
	# mirror at x
	i = len(coords)
	while i > 0:
		i -= 1
		(x, y) = coords[i]
		coords += [(x, -y)]
	
	userPoly(coords)
	

def mirrorY(coords):
	for i in range(0, len(coords)):
		(x, y) = coords[i]
		coords[i] = (-x, y)
	return coords
	


header("Switch_Mount", 0)
# outer contour
switchMountDrawing([(25.5, 7), (22.8, 7), (22.8, 13.2), (24, 13.2), (24, 18.2), (23, 18.2)])
#switchMountDrawing([(25.5, 7), (22.8, 7), (22.8, 13.2), (22, 13.2), (22, 17.8)])

# allowed area
area = [(20, -11), (20, -18), (17, -21), (5, -21), (5, -17.2), (2.5, -17.2), (2.5, -13)]
userLines(area)
userLines(mirrorY(area))

# carrier
userRect(24, 7, -24, -7)

#userRect(24, 4, -24, 7)
#userRect(24, -4, -24, -7)
#userRect(5.6/2, 34.4/2, -5.6/2, 40/2)
#userRect(5.6/2, -34.4/2, -5.6/2, -40/2)

# mounting holes
width = 3
height = 7
frontClearance = 1.5
backClearance = 0.3
thruHolePad(1, CIRCLE, 30, 0, width+backClearance*2, height+backClearance*2, (width, height))
smdPad(1, CIRCLE, 30, 0, width+frontClearance*2, height+frontClearance*2, layers="F.Cu F.Mask")
thruHolePad(2, CIRCLE, -30, 0, width+backClearance*2, height+backClearance*2, (width, height))
smdPad(2, CIRCLE, -30, 0, width+frontClearance*2, height+frontClearance*2, layers="F.Cu F.Mask")
thruHolePad(3, CIRCLE, 0, 30, height+backClearance*2, width+backClearance*2, (height, width))
smdPad(3, CIRCLE, 0, 30, height+frontClearance*2, width+frontClearance*2, layers="F.Cu F.Mask")
thruHolePad(4, CIRCLE, 0, -30, height+backClearance*2, width+backClearance*2, (height, width))
smdPad(4, CIRCLE, 0, -30, height+frontClearance*2, width+frontClearance*2, layers="F.Cu F.Mask")
#npthPad(20.5, 6.6, 5, 3, clearance=1)
#npthPad(-20.5, 6.6, 5, 3, clearance=1)
npthPad(20.5, 4, 3, 3, clearance=1)
npthPad(-20.5, 4, 3, 3, clearance=1)
footer()

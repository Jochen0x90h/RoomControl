// export checkist
// - resolution of circles high enough? ($fn = 128)
// - drill positions up to date? (gerber/drill2cad.py)
$fn = 128;

// variables

// pcb
pcbWidth = 68;
pcbHeight = 76;
pcbX = 0; // center of pcb
pcbY = 0;
pcbZ1 = 3;
pcbZ2 = pcbZ1+1.6; // mounting surface
pcbX1 = pcbX-pcbWidth/2;
pcbX2 = pcbX+pcbWidth/2;
pcbY1 = pcbY-pcbHeight/2;
pcbY2 = pcbY+pcbHeight/2;

// poti (Bourns PEC12R-4215F-S0024)
potiX = 22;
potiY = -22;
potiH1 = 7; // height of body above pcb (variant with switch)
potiH2 = 11.2; // height of shaft bearing above pcb
potiL = 17.5; // height of shaft end above pcb
potiF = 5; // shaft cutaway
potiZ1 = pcbZ2 + potiH1;
potiZ2 = pcbZ2 + potiH2;
potiZ4 = pcbZ2 + potiL;
potiZ3 = potiZ4 - potiF;
potiD1 = 7; // shaft bearing diameter
potiD2 = 6; // shaft diameter

// base
baseZ1 = 0;
baseZ2 = pcbZ1;

// cover
coverZ1 = pcbZ2;
coverZ2 = potiZ4 + 1; // use height of poti mounted on pcb plus cap
coverFit = 0.4;
coverOverlap = 2.5;

// wheel
wheelD1 = 10; // diameter of shaft
wheelD2 = 39; // diameter of wheel
wheelGap = 0.5; // visible air gab between box and wheel
wheelZ1 = potiZ3;
wheelZ2 = coverZ2 - 3; // thickness of wheel
wheelZ3 = coverZ2;

// pir lens
lensX = -23;
lensY = -30;
lensD1 = 8.9 + 0.1; // tolerance
lensD2 = 10.2 + 0.2; // tolerance
lensZ2 = coverZ2;
lensZ1 = lensZ2 - 4.35;
lensZ0 = lensZ1 - 4.5 - 0.1; // tolerance

// photo diode
photoX = lensX;
photoY = -14;
photoD = 5.1 + 0.1; // tolerance
photoZ2 = coverZ2;
photoZ1 = photoZ2 - 3.85;

// speaker
speakerX = 0;
speakerY = -25;
speakerWidth = 25 + 0.2; // tolerance
speakerHeight = 14 + 0.2; // tolerance
speakerZ2 = wheelZ2 - 2;
speakerZ1 = speakerZ2 - 4.5; // thickness of speaker

// micro usb
usbX = 22;
usbWidth = 8;
usbZ2 = pcbZ2 + 3; // thickness of usb connector
usbPlugWidth = 12;
usbPlugThickness = 8; // thickness of plug of usb cable

// e73 module
e73X = -13;
e73Width = 18.5;
e73Z2 = pcbZ2 + 2;

// display (2.42 inch)
displayTolerance = 0.4;
displayCableWidth1 = 13;
displayCableWidth2 = 16; // where the flat cable is connected to display
displayCableLength = 35;

// display screen (active area of display)
screenX = 0;
screenY = 20.5;
screenWidth = 57.01;
screenHeight = 29.49;
screenOffset = 1.08 + displayTolerance; // distance between upper panel border and upper screen border (tolerance at upper border)
screenX1 = screenX - screenWidth/2;
screenX2 = screenX + screenWidth/2;
screenY1 = screenY - screenHeight/2;
screenY2 = screenY + screenHeight/2;

// display panel
panelWidth = 60.5 + displayTolerance;
panelHeight = 37 + displayTolerance;
panelThickness1 = 1.2; // at side where cable is connected
panelThickness2 = 2.3;
panelX = screenX;
panelX1 = panelX - panelWidth / 2; // left border of panel
panelX2 = panelX + panelWidth / 2; // right border of panel
panelY2 = screenY + screenHeight/2+screenOffset; // upper border of panel
panelY1 = panelY2-panelHeight; // lower border of panel
panelY = (panelY2+panelY1)/2;
panelZ1 = coverZ2 - 1 - panelThickness2;
panelZ2 = coverZ2 - 1 - panelThickness1;
panelZ3 = coverZ2 - 1;
//echo(100+panelX1);
//echo(100-panelY1);
//echo(100+panelX2);
//echo(100-panelY2);

// power 
powerX1 = -12.3 - 0.4;
powerX2 = 21.7 + 0.4;
powerY1 = 17.5 - 0.3;
powerY2 = 37.5 + 0.3;
powerZ2 = pcbZ2 + 15 + 0.2;
assert(powerZ2 <= panelZ1, "power supply intersects display");

// clamps
clampsX1 = -16.3;
clampsX2 = 16.3;
clampsY1 = -10.5;
clampsY2 = 19;
clampsHeight = 13 + 1;


// cable hole
cableX1 = -17.0;
cableY1 = -1.6;
cableX2 = 34;
cableY2 = 10.2;
cableY = (cableY1 + cableY2) / 2;
fixationHeight = cableY2 - cableY1 - 2 * 2;


// box with center/size in x/y plane and z ranging from z1 to z2
module box(x, y, w, h, z1, z2) {
	translate([x-w/2, y-h/2, z1])
		cube([w, h, z2-z1]);
}

module cuboid(x1, y1, x2, y2, z1, z2) {
	translate([x1, y1, z1])
		cube([x2-x1, y2-y1, z2-z1]);
}

module barrel(x, y, d, z1, z2) {
	translate([x, y, z1])
		cylinder(r=d/2, h=z2-z1);
}

module cone(x, y, d1, d2, z1, z2) {
	translate([x, y, z1])
		cylinder(r1=d1/2, r2=d2/2, h=z2-z1);
}

module frustum(x, y, w1, h1, w2, h2, z1, z2) {
	// https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/Primitive_Solids#polyhedron	
	points = [
		// lower square
		[x-w1/2,  y-h1/2, z1],  // 0
		[x+w1/2,  y-h1/2, z1],  // 1
		[x+w1/2,  y+h1/2, z1],  // 2
		[x-w1/2,  y+h1/2, z1],  // 3
		// upper square
		[x-w2/2,  y-h2/2, z2],  // 4
		[x+w2/2,  y-h2/2, z2],  // 5
		[x+w2/2,  y+h2/2, z2],  // 6
		[x-w2/2,  y+h2/2, z2]]; // 7
	faces = [
		[0,1,2,3],  // bottom
		[4,5,1,0],  // front
		[7,6,5,4],  // top
		[5,6,2,1],  // right
		[6,7,3,2],  // back
		[7,4,0,3]]; // left  
	polyhedron(points, faces);
}

module wheel() {
	x = potiX;
	y = potiY;

	// wheel
	difference() {
		// wheel
		barrel(x, y, wheelD2, wheelZ2, wheelZ3);

		// cutout for poti shaft (with tolerance)
		box(x=x, y=y, w=10, h=4.5, z1=1, z2=potiZ4+0.1);
		box(x=x, y=y, w=4.5, h=10, z1=1, z2=potiZ4+0.1);
	}
	
	// shaft holder for poti shaft
	difference() {
		intersection() {
			// box that has 1mm wall on sides
			box(x=x, y=y, w=8, h=8, z1=wheelZ1, z2=wheelZ2+1);

			// make round corners
			barrel(x, y, wheelD1, wheelZ1, wheelZ2+1);
		}

		// subtract rounded hole
		intersection() {
			box(x=x, y=y, w=potiD2, h=potiD2, z1=wheelZ1-1, z2=wheelZ3-1);
			barrel(x, y, 3.8*2, wheelZ1-1, wheelZ3-1);
		}
	}
	
	// poti shaft cutaway
	difference() {
		// fill shaft cutaway which is 1.5mm
		union() {
			box(x=x, y=y+2.5, w=1, h=2, z1=potiZ3, z2=wheelZ3-2);
			box(x=x, y=y+2.0, w=2, h=1, z1=potiZ3, z2=wheelZ3-2);
		}

		// subtract slanted corners to ease insertion of poti shaft
		translate([x, y+1.6, potiZ3-0.5])
			rotate([30, 0, 0])
				box(x=0, y=0, w=4, h=1, z1=0, z2=2);
	}
}

// for adding wheel base
module wheelBase(x, y) {
	// base in z-direction: 1mm air, 2mm wall
	barrel(x, y, wheelD2+(wheelGap+2)*2, wheelZ2-1-2, wheelZ3);
	
	// shaft radial: 5mm wheel shaft, air gap, 1mm wall
	// zhaft z-direction: 2mm wall, 1mm air, wheel
	barrel(x, y, wheelD1+(wheelGap+1)*2, wheelZ1-2-1, wheelZ3);

	// shaft radial: poti shaft, no air gap, 1.5mm wall
	barrel(x, y, potiD1+1.5*2, potiZ1, wheelZ3);
}

// for subtracting from wheel base
module potiCutout(x, y) {
	// cutout for wheel: 3mm wheel, 1mm air
	barrel(x, y, wheelD2+wheelGap*2, wheelZ2-1, coverZ2+1);

	// hole for poti shaft bearing
	barrel(x, y, potiD1+0.1*2, pcbZ2, wheelZ2);

	// hole for poti shaft
	barrel(x, y, potiD2+0.1*2, pcbZ2, wheelZ2);

	// hole for wheel shaft
	barrel(x, y, wheelD1+wheelGap*2, wheelZ1-1, wheelZ2);
}

// snap lock between cover and base
module snap(x, l) {
	translate([x, 0, coverZ1+1])
		rotate([0, 45, 0])
			cuboid(x1=-0.65, x2=0.65, y1=-l/2, y2=l/2, z1=-0.65, z2=0.65);
}

module usb() {
	color([0.5, 0.5, 0.5])
	box(x=usbX, y=pcbY1+1, w=usbWidth, h=10, z1=pcbZ2-0.1, z2=usbZ2);		
}

module usbPlug() {
	usbPlugZ1 = (usbZ1+usbZ2)/2-usbPlugThickness/2;
	color([0, 0, 0])
	box(x=usbX, y=pcbY1-10, w=usbPlugWidth, h=14,
		z1=usbPlugZ1, z2=usbPlugZ1+usbPlugThickness);		
}

module e73() {
	color([0.0, 0.6, 0.0])
	box(x=e73X, y=pcbY1+1, w=e73Width, h=10, z1=pcbZ2-0.1, z2=e73Z2);			
}

module drill(x, y, d) {
	h1 = d + 1;
	w1 = h1;
	h2 = d + 0.2;
	w2 = h2;
	frustum(x, y, w2, h2, w1, h1, pcbZ1-1.5, pcbZ1+0.1);
	box(x, y, w2, h2, pcbZ2-4, pcbZ1);	
}

// generated from .drl files using drill2cad.py
module drills() {
	drill(-10, -19.9, 0.5);
	drill(-8.1, -19.9, 0.5);
	drill(-22.6, 10.62, 0.8);
	drill(-22.6, 3, 0.8);
	drill(-20.06, 10.62, 0.8);
	drill(-20.06, 3, 0.8);
	drill(-17.52, 10.62, 0.8);
	drill(-17.52, 3, 0.8);
	drill(-24.796, -28.204, 0.8);
	drill(-24.796, -31.796, 0.8);
	drill(-21.204, -28.204, 0.8);
	drill(-21.1, -33.8, 0.8);
	drill(-21.1, -35.8, 0.8);
	drill(-23, -12.75, 1);
	drill(-23, -15.29, 1);
	drill(-16.2, 22.25, 1);
	drill(-14, 29.75, 1);
	drill(19.5, -15, 1);
	drill(19.5, -29.5, 1);
	drill(22, -29.5, 1);
	drill(24.5, -15, 1);
	drill(24.5, -29.5, 1);
	drill(-32.3, -23.9, 1);
	drill(-32.3, -26.44, 1);
	drill(-32.02, -35.15, 1);
	drill(-29.48, -35.15, 1);
	drill(-26.94, -35.15, 1);
	drill(-24.4, -35.15, 1);
	drill(-12.5, -1.5, 1.1);
	drill(-12.5, -6.5, 1.1);
	drill(-7.5, -1.5, 1.1);
	drill(-7.5, -6.5, 1.1);
	drill(-2.5, -1.5, 1.1);
	drill(-2.5, -6.5, 1.1);
	drill(2.5, -1.5, 1.1);
	drill(2.5, -6.5, 1.1);
	drill(7.5, -1.5, 1.1);
	drill(7.5, -6.5, 1.1);
	drill(12.5, -1.5, 1.1);
	drill(12.5, -6.5, 1.1);
	drill(19.1, -36.5, 1.1);
	drill(24.9, -36.5, 1.1);
	drill(-10, 30, 1.1);
	drill(-10, 25, 1.1);
	drill(19.4, 35.2, 1.1);
	drill(19.4, 19.8, 1.1);
	drill(-10, 15, 1.1);
	drill(-10, 10, 1.1);
	drill(-5, 15, 1.1);
	drill(-5, 10, 1.1);
	drill(0, 15, 1.1);
	drill(0, 10, 1.1);
	drill(5, 15, 1.1);
	drill(5, 10, 1.1);
	drill(10, 15, 1.1);
	drill(10, 10, 1.1);
	drill(15.9, -22, 2.2);
	drill(28.1, -22, 2.2);
}

module base() {
color([0.3, 0.3, 1]) {
	difference() {
		union() {
			difference() {
				// base with slanted walls
				union() {
					box(x=0, y=0, w=76-coverFit, h=76-coverFit,
						z1=0, z2=pcbZ1);
				
					// cover snap
					box(-(76-coverFit)/2+1, 0, 2, 76-coverFit,
						0, coverZ1+coverOverlap);
					box((76-coverFit)/2-1, 0, 2, 76-coverFit,
						0, coverZ1+coverOverlap);
				}
				
				// subtract inner volume
				box(x=0, y=0, w=72-coverFit, h=72-coverFit, z1=2, z2=coverZ2);
			
				// subtract snap lock
				snap(-76/2, 44);
				snap(76/2, 44);			
			}
		}
		
		// subtract drills
		drills();

		// subtract mounting screw holes
		for (i = [-2 : 2]) {
			//cone(cos(i) * 60 - 30, sin(i) * 60, 2.8, 6, -0.1, 2.1);
			//cone(30 - cos(i) * 60, sin(i) * 60, 2.8, 6, -0.1, 2.1);
			cone(-30, i, 2.8, 6, -0.1, 2.1);
			cone(30, i, 2.8, 6, -0.1, 2.1);
			cone(i, -30, 2.8, 6, -0.1, 2.1);
			cone(i, 30, 2.8, 6, -0.1, 2.1);
		}

		// subtract cable hole
		box(0, 0, 40, 50, pcbZ1 - clampsHeight, pcbZ1);
	}

	// poti support
	barrel(potiX, potiY, 2*2, 0, pcbZ1);
} // color
}

module cover() {
color([1, 0, 0]) {
	difference() {
		union() {
			difference() {
				// box
				difference() {
					box(x=0, y=0, w=80, h=80, z1=coverZ1, z2=coverZ2);
					
					// subtract inner volume, leave 2mm wall
					box(x=0, y=0, w=76, h=76, z1=coverZ1-1, z2=coverZ2-2);
				}
			}
			
			intersection() {
				// poti base
				wheelBase(x=potiX, y=potiY);
			
				// cut away poti base outside of cover and at display
				cuboid(x1=-40, y1=-40, x2=40, y2=panelY1, z1=pcbZ2, z2=coverZ2-2);
			}
			
			// lower display holder
			box(x=panelX, y=panelY1-0.5, w=panelWidth+2, h=3,
				z1=panelZ1, z2=coverZ2);
			box(x=panelX, y=panelY1, w=panelWidth+2, h=2,
				z1=panelZ2-2, z2=panelZ2);
		
			// upper display holder (not needed bacause of power supply)
			//box(panelX, panelY2-0.5,
			//	10, 1,
			//	pcbZ2, panelZ1);
			
			// lower pcb support
			box(pcbX, pcbY1, pcbWidth, 3, pcbZ2, pcbZ2+2);

			// upper pcb support
			box(pcbX, pcbY2, pcbWidth, 3, pcbZ2, pcbZ2+2);
		
			// display cable support
			box(0, -12, displayCableWidth1, 2,
				wheelZ2-3, coverZ2);

			// pir lens border
			barrel(lensX, lensY,
				lensD1 + 1.5,
				lensZ1, lensZ2);
			box(lensX - lensD2/2, lensY,
				2, 4,
				lensZ0-1, lensZ2);

			// photo diode border
			barrel(photoX, photoY,
				photoD+1,
				photoZ1, photoZ2);

			// speaker border
			box(speakerX, speakerY,
				speakerWidth+2, speakerHeight+2,
				speakerZ2-1, coverZ2);
			
			// speaker holder
			box(speakerX, (-40+speakerY)/2,
				10, speakerY--40,
				speakerZ1-1, speakerZ1);
			translate([speakerX, speakerY, speakerZ1])
				sphere(r = 1);
		}
		
		// subtract poti cutout for wheel and axis
		potiCutout(x=potiX, y=potiY);		

		// subtract pir lens cutout
		barrel(lensX, lensY,
			lensD1,
			lensZ1-1, lensZ2+1);
		barrel(lensX, lensY,
			lensD2,
			lensZ0, lensZ1);		
		
		// subtract photo diode cutout
		barrel(photoX, photoY,
			photoD,
			photoZ1-1, photoZ2+1);

		// subtract speaker cutout
		box(speakerX, speakerY,
			speakerWidth, speakerHeight,
			speakerZ2-2, speakerZ2);
		box(speakerX, speakerY,
			speakerWidth-2, speakerHeight-1,
			speakerZ2-2, coverZ2-2);

		// subtract display screen window
		frustum(x=screenX, y=screenY,
			w1=screenWidth-4, h1=screenHeight-4,
			w2=screenWidth+4, h2=screenHeight+4,
			z1=coverZ2-3, z2=coverZ2+1);

		// subtract display panel (glass carrier)
		box(x=panelX, y=panelY, w=panelWidth, h=panelHeight,
			z1=panelZ2, z2=panelZ3);

		// subtract display cable cutout
		box(x=panelX, y=panelY1, w=displayCableWidth2, h=6,
			z1=panelZ3-10, z2=panelZ3+0.2);
		cuboid(x1=panelX1, y1=panelY1, x2=panelX2, y2=panelY1+5,
			z1=panelZ2, z2=panelZ3+0.2);
		frustum(x=screenX, y=panelY1, 
			w1=displayCableWidth1, h1=45,
			w2=displayCableWidth1, h2=6,
			z1=panelZ3-10, z2=panelZ3-0.5);
	
		// subtract components on pcb
		//usbPlug();
		power();
		e73();
		usb();
		//clamps();
	}		

	// snap lock between cover and base
	snap(-(76)/2, 40);
	snap((76)/2, 40);
} // color
}

// cover for fabrication
module coverForProduction() {
	cover();
	box(0, -29.5, 1.5, 20, coverZ1, coverZ1+1.5);
	translate([-potiX, 6, coverZ1-wheelZ1]) {
		wheel();
	}
}


module pcb() {
	color([0, 0.6, 0, 0.3])
	box(x=pcbX, y=pcbY, w=pcbWidth, h=pcbHeight,
		z1=pcbZ1, z2=pcbZ2);
}

module poti() {
	x = potiX;
	y = potiY;
	color([0.5, 0.5, 0.5]) {
		box(x, y, 13.4, 12.4, pcbZ2, potiZ1);
		box(x, y, 15, 6, pcbZ2, potiZ1);
		barrel(x, y, potiD1, pcbZ2, potiZ2);
		difference() {
			barrel(x, y, potiD2, pcbZ2, potiZ4);
			box(x, y+potiD2/2, 10, 3, potiZ3, potiZ4+0.5);
		}
	}
}

module relays() {
	color([1, 1, 1]) {
		cuboid(-23.2, 22.8, 37.5, 37.5, pcbZ2, pcbZ2+9.2);
	}
}

module power() {
	color([0, 0, 0]) {
		cuboid(powerX1, powerY1, powerX2, powerY2, pcbZ2-0.1, powerZ2);
	}	
}

module clamps() {
	color([0.8, 0.8, 0.8]) {
		cuboid(clampsX1, clampsY1, clampsX2, clampsY2,
			pcbZ1 - clampsHeight, pcbZ1);
	}		
}
	
	
// casing parts that need to be printed
//base();
cover();
//wheel();
//coverForProduction();

// reference parts
//pcb();
//poti();
//usb();
//usbPlug();
//e73();
//relays();
//power();
//clamps();
//barrel(-9, -19.7, 3, pcbZ2, pcbZ2 + 8); // crystal

$fn = 128;

// production constraints
minWall = 1;
minGap = 0.8;

// pcb
pcbZ1 = 0;
pcbZ2 = 1.6;
pinZ = pcbZ1 + 4; // pins soldered into through-hole pads

// dimensions
Z = 12; // overall height in Z-direction
rockerZ = 16; // height of rocker surface

// actuation angle
angle = 5.4; // maximum actuation angle
tanAngle = tan(angle);


// axis
axisD = 2.5;
axisX2 = /*37.2*/36.8 / 2; // outer rocker axis
axisX1 = 37.2/2 - 15.85; // inner rocker axis
axisZ = Z - 1.3;
axisCoutoutWidth = 7;
axisCutoutZ = axisZ - 2.5;

// body
bodyWidth = 40;

// switch actuator
actuatorWidth = 3.2;
actuatorX = 17 / 2;
actuatorY = 20 / 2;
actuatorZ = axisZ - 1.3 + 2;


// micro switch Omron D2F-01
switchWidth = 5.8 + 0.2; // tolerance
switchHeight = 12.8;
switchX = switchHeight / 2;
switchY = actuatorY;
switchZ2 = pcbZ2+6.5;//actuatorZ - 1 - 1;
switchZ1 = switchZ2 - 5;
switchZ0 = switchZ1 - 1.5;
switchZ3 = switchZ2 + 1; // actuator
switchesX2 = switchX + switchHeight / 2;
switchesWidth = switchesX2 * 2;
switchesY2 = switchY + switchWidth / 2;


// mount
mountWidth = 48;
mountHeight = 48;
mountRight = mountWidth / 2;
mountTop = mountHeight / 2;

// mount part 1 (sides)
mount1Width = 4;
mount1Height = 18.2;
mount1X = mountRight - mount1Width / 2;
mount1Y = mount1Height / 2;

// mount part 2 (corners, rotated by 45°)
mount2Width = 9.5;
mount2Height = 2.25;
mount2Y2 = 58.1 / 2;
mount2Y = mount2Y2 - mount2Height / 2;

// mount part 3 (top/bottom)
mount3Width = 18.2 * 2;
mount3Height = 3;//mountTop - barY2 - minGap;
mount3Y = mountTop - mount3Height / 2;

// mount part 4 (spring support)
mount4Width = 5;
mount4Height = mountTop - switchesY2;
mount4Y = (mountTop + switchesY2) / 2;

// mount Z
mountZ1 = pcbZ2;
mountZ2 = mountZ1 + 2; // thickness of frame (Eltako E-Design)


// clamp part 1 (sides)
clamp1Width = 1.5;
clamp1Height = mount1Height;
clamp1X = mountRight - clamp1Width / 2;
clamp1Y = mount1Y;

// clamp part 2 (corners and frame clamps, rotated by 45°)
clamp2Width1 = mount2Width;
clamp2Width2 = 6.5;
clamp2Height = 1.5;
clamp2Y = mount2Y2 - clamp2Height / 2;

// clamp Z
clampZ1 = mountZ2 + minGap;
clampZ2 = Z - mountTop * tanAngle; // fits under rockers at top/bottom edge
clampZ3 = Z - mount1Height * tanAngle;


// middle carrier
carrierWidth = mountWidth;
carrierHeight = (switchY - switchWidth / 2) * 2;
carrierZ1 = pcbZ2;
carrierZ2 = pinZ + 1;

// top above switches
topWidth = switchesWidth;
topHeight = switchWidth + 2;
topY = switchY;
topY1 = switchY - topHeight/2;
topY2 = switchY + topHeight/2;

// screws
screwX = 20.5;
screwY = -4;
screwD = 2.5;
screwMountSize = 6; // diameter of screw mount

// actuator bar (front)
barWidth = (32 + 2) / 2;
barX = barWidth / 2;
barY1 = 34.4 / 2;
barY2 = 40 / 2;
barY = (barY1 + barY2) / 2;
barZ2 = axisZ - 1.3;
barZ1 = barZ2 - 1.5; // thickness of bar
//echo(barZ1);
barHeight = barY2 - barY1;
barTravel = barY2 * tanAngle;
//echo(barTravel);

// spring below actuator bar
springZ = (pcbZ2 + barZ1) / 2; // "middle" of spring
springZ1 = springZ - barTravel / 2; // lower block
springZ2 = springZ + barTravel / 2; // upper block


// led (Kingbright WP710A10LZGCK)
//ledX = 15.75;
//ledY = 23;//22;
ledX = 15.5;
ledY = 0;
ledD1 = 2.9;
ledD2 = 3.2;
ledZ2 = rockerZ - 2.5 - ledY * tanAngle - 0.1; // tolerance
ledZ1 = ledZ2 - 3.6; 
ledZ0 = ledZ2 - 5.4;


// lever arm
leverX1 = switchesX2 + 1; // 1mm left/right tolerance
leverX2 = axisX2 - minWall - minGap;
leverX = (leverX1 + leverX2) / 2;
leverY1 = ledD2 / 2;
leverY2 = topY2 + minGap;
leverY = (leverY1 + leverY2) / 2;
leverWidth = leverX2 - leverX1;
leverHeight = leverY2 - leverY1;

// lever front
front1Y1 = topY2 + minGap;
front1Y2 = barY1 - minGap;
front1Y = (front1Y1 + front1Y2) / 2;
front1Height = front1Y2 - front1Y1;
front2Y1 = topY2 + minGap;
front2Y2 = 33.3 / 2;
front2Y = (front2Y1 + front2Y2) / 2;
front2Height = front2Y2 - front2Y1;

// lever Z
leverZ1 = pinZ + front1Y1 * tanAngle;
leverZ2 = Z - 4;



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

module axis(x, y, d, l, z) {
	translate([x-l/2, y, z])
		rotate([0, 90, 0])
			cylinder(r=d/2, h=l);
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

module slopeX(x, y, w, h, z1, z2, z3, z4, rl=1) {
	// https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/Primitive_Solids#polyhedron

	if (rl < 0) {
		slopeX(x, y, w, h, z2, z1, z4, z3);
	} else {
		x1 = x - w / 2;
		x2 = x + w / 2;
		y1 = y - h / 2;
		y2 = y + h / 2;
		
		points = [
			// lower square
			[x1,  y1, z1],  // 0
			[x2,  y1, z2],  // 1
			[x2,  y2, z2],  // 2
			[x1,  y2, z1],  // 3
			// upper square
			[x1,  y1, z3],  // 4
			[x2,  y1, z4],  // 5
			[x2,  y2, z4],  // 6
			[x1,  y2, z3]]; // 7
		faces = [
			[0,1,2,3],  // bottom
			[4,5,1,0],  // front
			[7,6,5,4],  // top
			[5,6,2,1],  // right
			[6,7,3,2],  // back
			[7,4,0,3]]; // left  
		polyhedron(points, faces);
	}
}

module slopeY(x, y, w, h, z1, z2, z3, z4, tb=1) {
	// https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/Primitive_Solids#polyhedron

	if (tb < 0) {
		slopeY(x, y, w, h, z2, z1, z4, z3);
	} else {
		x1 = x - w / 2;
		x2 = x + w / 2;
		y1 = y - h / 2;
		y2 = y + h / 2;	
		
		points = [
			// lower square
			[x1,  y1, z1],  // 0
			[x2,  y1, z1],  // 1
			[x2,  y2, z2],  // 2
			[x1,  y2, z2],  // 3
			// upper square
			[x1,  y1, z3],  // 4
			[x2,  y1, z3],  // 5
			[x2,  y2, z4],  // 6
			[x1,  y2, z4]]; // 7
		faces = [
			[0,1,2,3],  // bottom
			[4,5,1,0],  // front
			[7,6,5,4],  // top
			[5,6,2,1],  // right
			[6,7,3,2],  // back
			[7,4,0,3]]; // left  
		polyhedron(points, faces);
	}
}

module drill(x, y, d) {
	shift = 0;
	h1 = 2;
	w1 = h1 + shift;
	h2 = d + 0.1;
	w2 = h2 + shift;
	frustum(x, y, w1, h1, w2, h2, pcbZ2-0.1, pcbZ2+1.5);
	box(x, y, w2, h2, pcbZ2, pinZ);
}

module drills() {
	drill(-19.3, 25.5, 0.8);
	drill(-17.3, 25.5, 0.8);
	drill(-15.3, 25.5, 0.8);
	drill(-13.3, 25.5, 0.8);
	drill(-11.3, 25.5, 0.8);
	drill(-15.5, 10, 0.9);
	drill(-13.7, 10, 0.9);
	drill(13.7, 10, 0.9);
	drill(15.5, 10, 0.9);
	drill(-22.54, 10, 1);
	drill(-22.54, 4.92, 1);
	drill(-22.54, 2.38, 1);
	drill(-22.54, -0.16, 1);
	drill(-17.46, 10, 1);
	drill(-17.46, 4.92, 1);
	drill(-17.46, 2.38, 1);
	drill(-17.46, -0.16, 1);
	drill(-11.76, 2.54, 1);
	drill(-11.76, -2.54, 1);
	drill(-9.22, 2.54, 1);
	drill(-9.22, -2.54, 1);
	drill(-6.68, 2.54, 1);
	drill(-6.68, -2.54, 1);
	drill(-1.6, 2.54, 1);
	drill(-1.6, -2.54, 1);
	drill(1.6, 2.54, 1);
	drill(1.6, -2.54, 1);
	drill(6.68, 2.54, 1);
	drill(6.68, -2.54, 1);
	drill(9.22, 2.54, 1);
	drill(9.22, -2.54, 1);
	drill(11.76, 2.54, 1);
	drill(11.76, -2.54, 1);
	drill(17.46, 10, 1);
	drill(17.46, 4.92, 1);
	drill(17.46, 2.38, 1);
	drill(17.46, -0.16, 1);
	drill(22.54, 10, 1);
	drill(22.54, 4.92, 1);
	drill(22.54, 2.38, 1);
	drill(22.54, -0.16, 1);
	drill(-12.5, -16, 1.1);
	drill(-12.5, -21, 1.1);
	drill(-11.48, 10, 1.1);
	drill(-11.48, -10, 1.1);
	drill(-7.5, 21, 1.1);
	drill(-7.5, 16, 1.1);
	drill(-7.5, -16, 1.1);
	drill(-7.5, -21, 1.1);
	drill(-6.4, 10, 1.1);
	drill(-6.4, -10, 1.1);
	drill(-2.5, 21, 1.1);
	drill(-2.5, 16, 1.1);
	drill(-2.5, -16, 1.1);
	drill(-2.5, -21, 1.1);
	drill(-1.32, 10, 1.1);
	drill(-1.32, -10, 1.1);
	drill(1.32, 10, 1.1);
	drill(1.32, -10, 1.1);
	drill(2.5, 21, 1.1);
	drill(2.5, 16, 1.1);
	drill(2.5, -16, 1.1);
	drill(2.5, -21, 1.1);
	drill(6.4, 10, 1.1);
	drill(6.4, -10, 1.1);
	drill(7.5, 21, 1.1);
	drill(7.5, 16, 1.1);
	drill(7.5, -16, 1.1);
	drill(7.5, -21, 1.1);
	drill(11.48, 10, 1.1);
	drill(11.48, -10, 1.1);
	drill(12.5, -16, 1.1);
	drill(12.5, -21, 1.1);
}

module switchPins(x, y, angle) {
	translate([x, y, 0]) {
		rotate([0, 0, angle]) {
					
			// holes
			axis(0, 6.5/2, 1.9, 1*2, switchZ1);
			axis(0, -6.5/2, 1.9, 1*2, switchZ1);
		}
	}
}

module catwalk(x, w) {
	for (tb = [1, -1]) {
		slopeY(x, carrierHeight/4*tb,
			w, carrierHeight/2,
			pcbZ2, pcbZ2,
			Z, Z-carrierHeight/2*tanAngle,
			tb);
	}
}

module body() {
	
	// width of both switches
	switchesWidth = switchX * 2 + switchHeight + 0.1;
	
	// catwalks
	leftCatwalkWidth = 2;
	leftCatwalkX = axisX1 + leftCatwalkWidth / 2;
	middleCatwalkWidth = 2;
	middleCatwalkX = leverX1 - minGap - middleCatwalkWidth / 2;
	rightCatwalkWidth = bodyWidth / 2 - axisX2 + minWall;
	rightCatwalkX = bodyWidth / 2  - rightCatwalkWidth / 2;

	// switch actuators
	for (rl = [1, -1]) {
		box(actuatorX*rl, 0,
			actuatorWidth, (actuatorY+1.5)*2,
			actuatorZ-1, actuatorZ);
		
		for (tb = [1, -1]) {
			box(actuatorX*rl, actuatorY*tb,
				actuatorWidth, 1,
				switchZ3, actuatorZ);
		}
	}
	
	// switch cover
	difference() {
		union() {
			for (tb = [1, -1]) {
				// top
				box(0, topY*tb,
					topWidth, topHeight,
					switchZ2, switchZ2 + 1);
				
				// side
				box(0, (topY2-0.5)*tb,
					topWidth, 1,
					switchZ1, switchZ2 + 1);
			}
		}
		
		// right / left
		for (rl = [1, -1]) {
			// cutaway space for actuator
			box(actuatorX*rl, 0,
				actuatorWidth+minGap, switchesY2*2,//(actuatorY+1+minGap)*2,
				switchZ2-0.1, Z);
		}
	}

	// carrier, axis and screw mount blocks
	difference() {
		// additive components
		union() {
			// axis
			axis(0, 0, axisD, carrierWidth, axisZ);
			
			// carrier
			box(0, 0, carrierWidth, carrierHeight, carrierZ1, carrierZ2);
			
			for (rl = [1, -1]) {			
				// left catwalk
				catwalk(leftCatwalkX*rl, leftCatwalkWidth);
				
				// middle catwalk
				catwalk(middleCatwalkX*rl, middleCatwalkWidth);		

				// right catwalk and screw mount block with axis cutout
				difference() {
					union() {
						// right catwalk
						catwalk(rightCatwalkX*rl, rightCatwalkWidth);
					
						// screw mount block
						barrel(screwX*rl, screwY, screwMountSize,
							pcbZ2, pcbZ1+9);
						// Z-(-screwY+screwMountSize/2)*tanAngle); 
					}
					
					// subtract axis cutout
					box((axisX2+1)*rl, 0, 2, axisCoutoutWidth,
						axisCutoutZ, Z+1);
				}
			}
		}

		// right / left
		for (rl = [1, -1]) {
			// cut away where lever is attached to carrier
			box(leverX*rl, 0,
				leverWidth+2*minGap, minGap+1+ledD2+1+minGap,
				pcbZ2+1.5, Z+1);			
		}
	}
		
	// support for leds
	for (rl = [1, -1]) {
		box(ledX*rl, ledY,
			1.8, ledD2-minGap*2,
			pcbZ2, ledZ0);
	}

	
	// pins for fixing the switches
	switchPins(switchX, -switchY+switchWidth/2, -90);
	switchPins(switchX, switchY-switchWidth/2, -90);
	switchPins(-switchX, -switchY+switchWidth/2, 90);
	switchPins(-switchX, switchY-switchWidth/2, 90);
}


module lever(rl, angle) {
	// top/bottom
	for (tb = [1, -1]) {
		// connection to carrier
		box(leverX*rl, (leverY1+0.5)*tb,
			leverWidth, 1, pcbZ2, Z);
		
		// rotatable part
		translate([0, 0, axisZ]) {
			rotate([angle, 0, 0])
			translate([0, 0, -axisZ]) {
				// lever arm
				difference() {
					slopeY(leverX*rl, leverY*tb,
						leverWidth, leverHeight,
						leverZ2, leverZ1,
						Z, Z, tb);
					
					// cut away gap so that lever can rotate
					axis(leverX*rl, (leverY1+1+0.5)*tb,
						1, leverWidth+1, Z-2);
					box(leverX*rl, (leverY1+1+0.5)*tb,
						leverWidth+1, 1, 0, Z-2);
				}

				// lever front
				slopeY(barX*rl, front1Y*tb,
					barWidth, front1Height,
					leverZ1, leverZ1+front1Height*tanAngle, // avoid thru-hole pins
					barZ2, barZ2,
					tb);
				box(barX*rl, front2Y*tb,
					barWidth, front2Height,
					barZ1-0.4, Z);

				// bar (increase height to connect to lever)
				box(barX*rl, (barY-0.5)*tb,
					barWidth, barHeight+1,
					barZ1, barZ2);

				// spring upper block
				box(mount4Width/4*rl, barY*tb,
					mount4Width/2, barHeight,
					springZ2, barZ2);
			}
		}

		// spring
		slopeX(barX*rl, barY*tb,
			barWidth, barHeight,
			pcbZ2, springZ-1.3,
			pcbZ2+1.5, springZ+0.2,
			rl);
		slopeX(barX*rl, barY*tb,
			barWidth, barHeight,
			barZ1-1.5, springZ-0.2,
			barZ1, springZ+1.3,
			rl);
	}	
}

module mount() {
	difference() {
		union() {
			// top/bottom
			for (tb = [1, -1]) {
				// right/left
				for (rl = [1, -1]) {
					// mount
					box(mount1X*rl, mount1Y*tb,
						mount1Width, mount1Height,
						mountZ1, mountZ2);
					
					// clamp
					slopeY(clamp1X*rl, clamp1Y*tb,
						clamp1Width, clamp1Height,
						clampZ1, clampZ1,
						Z, clampZ3, tb);
				}
				
				// top/bottom part of mount
				box(0, mount3Y*tb,
					mount3Width, mount3Height,
					mountZ1, mountZ2);
			
				// lower spring support
				box(0, mount4Y*tb,
					mount4Width, mount4Height,
					mountZ1, springZ1);
				
			}
			
			// corner parts of mount and clamp
			for (angle = [45, 135, 225, 315]) {
				rotate([0, 0, angle]) {
					// mount
					box(0, mount2Y,
						mount2Width, mount2Height,
						mountZ1, mountZ2);
					
					// clamp
					box(0, clamp2Y,
						clamp2Width1, clamp2Height,
						clampZ1, clampZ2);					
					slopeY(0, mount2Y2+0.5,
						clamp2Width2, 1.0,
						mountZ2+0.2, mountZ2,
						clampZ2, clampZ1);
				}
			}			
		}
		
		// cutaway for parts on pcb (stm32)
		box(85.3-100, mount3Y-mount3Height+0.2,
			8, mount3Height,
			mountZ1-1, mountZ1+0.6);
		
		// cutaway for leds
		//for (rl = [1, -1]) {
		//	barrel(ledX*rl, ledY, ledD2+2.5, mountZ2+0.1, ledZ2+1);			
		//}
		
		// bottom cutaway
		//box(0, -mount3Y,
		//	mount3Width, mount3Height+1,
		//	clampZ1-0.1, clampZ2+0.1);
		
	}
}


// Omron D2F-01, only for visualization
module switch(x, y, angle) {
	translate([x, y, 0]) {
		rotate([0, 0, angle]) {
			// body
			color([0, 0, 0]) {
				difference() {
					box(0, 0, switchWidth, switchHeight, switchZ0, switchZ2);
					
					// holes
					axis(0, 6.5/2, 2, 6, switchZ1);
					axis(0, -6.5/2, 2, 6, switchZ1);
				}
			}
			
			// actuator
			color([1, 0, 0]) {
				box(0, 5.2-6.5/2, 2.9, 1.2, switchZ2, switchZ3);
			}
			
			// wires
			//barrel(0, 0, 0.9, switchZ0-3.5, switchZ0);
			barrel(0, 5.08, 0.9, switchZ0-3.5, switchZ0);
			//barrel(0, -5.08, 0.9, switchZ0-3.5, switchZ0);
		}
	}
}

// actuators, only for visualization
module actuators(angle) {
	color ([1, 1, 1])
	translate([0, 0, axisZ])
	rotate([angle, 0, 0])
	translate([0, 0, -axisZ]) {
		box(actuatorX, actuatorY,
			actuatorWidth, 1,
			actuatorZ, actuatorZ+1);
		box(-actuatorX, actuatorY,
			actuatorWidth, 1,
			actuatorZ, actuatorZ+1);
		box(actuatorX, -actuatorY,
			actuatorWidth, 1,
			actuatorZ, actuatorZ+1);
		box(-actuatorX, -actuatorY,
			actuatorWidth, 1,
			actuatorZ, actuatorZ+1);
		
		box(0, barY,
			32, barHeight,
			barZ2, barZ2+1);
		box(0, -barY,
			32, barHeight,
			barZ2, barZ2+1);

	}
}

module rockers(angle) {
	color ([1, 1, 1])
	translate([0, 0, axisZ])
	rotate([angle, 0, 0])
	translate([0, 0, -axisZ]) {
		box(0, 0,
			63, 63,
			15, 16);
	}
}

module led(angle) {
	// light pipe
	color ([1, 1, 1])
	translate([0, 0, axisZ])
	rotate([angle, 0, 0])
	translate([0, 0, -axisZ]) {
		barrel(ledX, ledY, ledD1, 16-2.5, 16);
	}

	color([0, 1, 0]) {		
		barrel(ledX, ledY, ledD1, ledZ0, ledZ2);
		barrel(ledX, ledY, ledD2, ledZ0, ledZ1);
	}
}


// switch base
difference() {
	union() {
		body();
		lever(1, 0);
		lever(-1, 0);
		mount();
	}
		
				
	// holes for screws
	barrel(screwX, screwY, screwD, pcbZ1, Z);
	barrel(-screwX, screwY, screwD, pcbZ1, Z);

	// holes for pcb drills
	drills();
}


// micro switches
//switch(switchX, -switchY, -90);
switch(switchX, switchY, -90);
//switch(-switchX, -switchY, 90);
//switch(-switchX, switchY, 90);

actuators(0);
//rockers(0);

// screws
//barrel(20.5, 8.5, 3, pcbZ1, pcbZ1+15);
//barrel(-20.5, 8.5, 2, pcbZ1, pcbZ1+10);


// measure overall height
//color([0, 0, 0]) box(0, 0, 10, 10, 0, 11.3);
//color([0, 0, 0]) box(0, 0, 10, 10, 0, 17.5);
//color([0, 0, 0]) box(0, 0, 10, 10, 0, 16-2.3);

// led
led(-angle);

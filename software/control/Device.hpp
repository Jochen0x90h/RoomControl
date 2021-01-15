#pragma once


/**
 * Device id type
 */
using DeviceId = uint32_t;


/**
 * Type of device endpoint such as button, relay or temperature sensor
 */
 enum class EndpointType : uint8_t {
	// generic binary sensor with two states
	BINARY_SENSOR = 1,
	
	// button, returns to released state when not pressed
	BUTTON = 2,
	
	// switch with two stable states
	SWITCH = 3,

	// rocker with two sides, returns to released state when not pressed
	ROCKER = 4,
	
	
	// generic relay with two states
	RELAY = 10,
	
	// light, only on/off
	LIGHT = 11,

	// two interlocked relays for blind, only one can be on
	BLIND = 12,
	
	
	// temperature sensor (16 bit value, 1/20 Kelvin)
	TEMPERATURE_SENSOR = 20,
	
	// air pressure sensor
	AIR_PRESSURE_SENSOR = 21,
	
	// air humidity sensor
	AIR_HUMIDITY_SENSOR = 22,
	
	// air voc (volatile organic compounds) content sensor
	AIR_VOC_SENSOR = 23,


	// brightness sensor
	BRIGHTNESS_SENSOR = 30,
	
	// motion detector
	MOTION_DETECTOR = 31,


	// active energy counter of an electicity meter
	ACTIVE_ENERGY_COUNTER = 40,
	
	// active power measured by an electricity meter
	ACTIVE_POWER_SENSOR = 41,
 };
 

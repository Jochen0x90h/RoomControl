#include <cstdint>
#include "Array.hpp"


/*
	Play sounds via built-in speaker
*/
namespace Sound {

/**
 * Types of sounds
 */
enum class Type : uint8_t {
	NONE,

	// event sound, e.g. door opening/closing
	EVENT,

	// activation/deactivation sound, e.g. air conditioning or intruder alarm activated/deactivated
	ACTIVATION,
	DEACTIVATION,

	// sound when an information appears on the display
	INFORMATION,

	// sound when a warning appears on the display
	WARNING,

	// dorbell
	DOORBELL,

	// telephone call
	CALL,

	// alarm, e.g. intruder alarm
	ALARM,
};

/**
 * Initialize sound driver
 */
void init();

/**
 * Get types of available sounds
 * @return array of types
 */
Array<Type> getTypes();

/**
 * Starts playing a sound in the background
 * @param index index of sound
 */
void play(int index);

/**
 * Stop a playing sound
 * @param index index of sound
 */
void stop(int index);

/**
 * Check if a sound is playing
 * @param index index of sound
 * @return true if playing
 */
bool isPlaying(int index);

} // Sound

#include <cstdint>


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
 * Initialize sound
 */
void init();

/**
 * Get number of sound files
 * @return
 */
int count();

/**
 * Starts playing a sound in the background
 * @param index
 */
void play(int index);

/**
 * Stop a playing sound
 */
void stop(int index);

} // Sound

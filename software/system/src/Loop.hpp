#pragma once


namespace Loop {

/**
 * Initialize the event loop
 */
void init();

/**
 * Run the event loop
 */
void run();

// busy waiting, only for debug purposes, not very precise
void sleepBlocking(int us);

} // namespace Loop

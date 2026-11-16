/*
 * Injects key presses ("taps") into the typewriter.
 *
 * A tap presses all keys but the last one first (modifiers such as Shift or
 * Code), waits until the typewriter has seen them, presses the last key for
 * a number of typewriter scan cycles, releases it, releases the modifiers
 * and finally pauses for the given gap.
 */
#pragma once

#include "Matrix.h"

#define MAX_TAP_KEYS 4

namespace Tapper {

enum Result {
	NONE,						/* idle or still working */
	DONE,						/* tap finished, gap is over */
	NO_SCAN						/* typewriter does not scan the keyboard */
};

/* Start a tap; false if one is running or count is invalid. */
bool start(const Key * keys, uint8_t count, uint16_t gapMs);

/* Advance the state machine; reports the end of a tap exactly once. */
Result poll();

/* Release everything at once. */
void abort();

bool busy();
void setHoldScans(uint8_t scans);
uint8_t holdScans();

}

/*
 * The side facing the typewriter mainboard: the Mega pretends to be the
 * keyboard.
 *
 * Whenever the typewriter strobes a column low, the rows of all "pressed"
 * keys of that column are pulled low (open drain, like a real switch).
 * "Pressed" is the physical keyboard (if passthrough is on) OR'ed with the
 * keys injected by the Pi. This happens in a pin change interrupt, so it is
 * fast enough for the 200 us strobes no matter what the main loop does.
 *
 * Pins: rows 0,1,2,3,6,7,8,9 -> D22..D29 (PA0..PA7)
 *       cols 0..7            -> A8..A15  (PK0..PK7)
 *       cols 8,9             -> D53, D52 (PB0, PB1)
 */
#pragma once

#include "Matrix.h"

namespace TypewriterPort {

void begin();

/* The two layers of the matrix. Call commit() to make changes visible. */
void setPhysical(uint8_t col, uint8_t rowMask);
void setInjected(const Key & key, bool down);
void clearInjected();
void setPassthrough(bool on);
bool passthrough();
void commit();

/* Count complete strobes of one column (to time injected key presses). */
void watchColumn(uint8_t col);
void watchOff();
uint8_t watchHits();

}

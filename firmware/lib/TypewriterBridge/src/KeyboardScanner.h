/*
 * The physical keyboard: scans the key matrix and debounces it.
 *
 * Pins: rows 0,1,2,3,6,7,8,9 -> D37..D30 (PC0..PC7), inputs with pull-up
 *       cols 0..7            -> D49..D42 (PL0..PL7), driven low one at a time
 *       cols 8,9             -> D41, D40 (PG0, PG1)
 */
#pragma once

#include "Matrix.h"

namespace KeyboardScanner {

/* Called for every key that went down or up */
typedef void (*KeyHandler) (bool down, uint8_t row, uint8_t col);

void begin(KeyHandler handler);

/* Scan if it is time to. Returns true if any key changed. */
bool poll();

/* Debounced state of one column */
uint8_t rows(uint8_t col);

}

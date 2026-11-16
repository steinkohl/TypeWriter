/*
 * Common definitions of the KX-R191 key matrix.
 *
 * A key sits at (row, col): row = pin of the right keyboard connector,
 * col = pin of the left one. Row pins 4 (GND) and 5 (caps lock LED) are no
 * matrix lines, so the eight real rows fit into one byte / one AVR port:
 * a "row mask" has one bit per real row.
 */
#pragma once

#include <Arduino.h>

#define NUM_COLS 10
#define NUM_ROWS 10				/* connector pins, including 4 and 5 */

/* A key as it is stored in the matrix */
struct Key {
	uint8_t col;
	uint8_t rowMask;
};

/* connector row pin -> bit in a row mask (-1: not a matrix line) */
static inline int8_t rowToBit(uint8_t row)
{
	static const int8_t table[NUM_ROWS] = { 0, 1, 2, 3, -1, -1, 4, 5, 6, 7 };
	return row < NUM_ROWS ? table[row] : -1;
}

/* bit in a row mask -> connector row pin */
static inline uint8_t bitToRow(uint8_t bit)
{
	static const uint8_t table[8] = { 0, 1, 2, 3, 6, 7, 8, 9 };
	return table[bit & 7];
}

#include "KeyboardScanner.h"

#include <avr/io.h>

#define SCAN_INTERVAL_US 2000	/* scan period */
#define SETTLE_US 30			/* settle time after selecting a column */
#define DEBOUNCE_SCANS 4		/* identical scans before a change counts */

namespace {

KeyboardScanner::KeyHandler onKey;
uint8_t state[NUM_COLS];		/* debounced */
uint8_t raw[NUM_COLS];
uint8_t stable[NUM_COLS];
unsigned long lastScan;

inline void select(uint8_t col)
{
	/* PORTL / PORTG bits are 0: switching to output drives the line low */
	if (col < 8) {
		DDRL = _BV(col);
	} else {
		DDRG |= _BV(col - 8);
	}
}

inline void deselect()
{
	DDRL = 0;
	DDRG &= ~0x03;
}

bool scan()
{
	bool changed = false;

	for (uint8_t c = 0; c < NUM_COLS; c++) {
		select(c);
		delayMicroseconds(SETTLE_US);
		uint8_t now = ~PINC;
		deselect();

		if (now != raw[c]) {
			raw[c] = now;
			stable[c] = 0;
			continue;
		}
		if (stable[c] < DEBOUNCE_SCANS) {
			stable[c]++;
			continue;
		}
		uint8_t diff = now ^ state[c];
		if (!diff) {
			continue;
		}
		state[c] = now;
		changed = true;
		if (onKey) {
			for (uint8_t b = 0; b < 8; b++) {
				if (diff & _BV(b)) {
					onKey(now & _BV(b), bitToRow(b), c);
				}
			}
		}
	}
	return changed;
}

}

namespace KeyboardScanner {

void begin(KeyHandler handler)
{
	onKey = handler;
	/* rows: inputs with pull-up */
	DDRC = 0;
	PORTC = 0xFF;
	/* columns: open drain, released */
	PORTL = 0;
	DDRL = 0;
	PORTG &= ~0x03;
	DDRG &= ~0x03;
}

bool poll()
{
	unsigned long now = micros();
	if (now - lastScan < SCAN_INTERVAL_US) {
		return false;
	}
	lastScan = now;
	return scan();
}

uint8_t rows(uint8_t col)
{
	return state[col];
}

}

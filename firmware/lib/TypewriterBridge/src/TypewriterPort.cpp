#include "TypewriterPort.h"

#include <avr/interrupt.h>
#include <avr/io.h>

namespace {

/* ---- shared with the interrupt handler ---- */
volatile uint8_t matrix[NUM_COLS];	/* what the typewriter shall see */
volatile uint16_t prevCols;			/* columns strobed at the last interrupt */
volatile uint16_t watchMask;		/* column whose strobes are counted */
volatile uint8_t watchArmed;
volatile uint8_t hits;				/* complete strobes of the watched column */

/* ---- main loop only ---- */
uint8_t physical[NUM_COLS];
uint8_t injected[NUM_COLS];
bool passthroughOn = true;

/*
 * Mirror the matrix to the row lines for the currently strobed column(s).
 * Runs in the pin change interrupts and, with interrupts disabled, whenever
 * the matrix changed.
 */
inline void mirror()
{
	uint8_t lo = ~PINK;					/* cols 0..7, active low */
	uint8_t hi = ~PINB & 0x03;			/* cols 8, 9 */
	uint8_t rows = 0;

	if (lo) {
		for (uint8_t c = 0; c < 8; c++) {
			if (lo & _BV(c)) {
				rows |= matrix[c];
			}
		}
	}
	if (hi & 0x01) {
		rows |= matrix[8];
	}
	if (hi & 0x02) {
		rows |= matrix[9];
	}
	/* PORTA is always 0: output = pulled low, input = released */
	DDRA = rows;

	uint16_t now = lo | ((uint16_t) hi << 8);
	if (watchMask) {
		if (now & ~prevCols & watchMask) {
			watchArmed = 1;				/* strobe of the watched column began */
		} else if (watchArmed && (prevCols & ~now & watchMask)) {
			watchArmed = 0;				/* ... and ended: one full strobe */
			if (hits < 255) {
				hits++;
			}
		}
	}
	prevCols = now;
}

}

ISR(PCINT0_vect)
{
	mirror();
}

ISR(PCINT2_vect)
{
	mirror();
}

namespace TypewriterPort {

void begin()
{
	/* rows: open drain, released */
	PORTA = 0;
	DDRA = 0;
	/* columns: inputs with pull-up */
	DDRK = 0;
	PORTK = 0xFF;
	DDRB &= ~0x03;
	PORTB |= 0x03;

	/* pin change interrupts on all column lines */
	PCMSK2 = 0xFF;
	PCMSK0 = 0x03;
	PCICR = _BV(PCIE2) | _BV(PCIE0);
}

void setPhysical(uint8_t col, uint8_t rowMask)
{
	physical[col] = rowMask;
}

void setInjected(const Key & key, bool down)
{
	if (down) {
		injected[key.col] |= key.rowMask;
	} else {
		injected[key.col] &= ~key.rowMask;
	}
}

void clearInjected()
{
	memset(injected, 0, sizeof(injected));
}

void setPassthrough(bool on)
{
	passthroughOn = on;
}

bool passthrough()
{
	return passthroughOn;
}

void commit()
{
	uint8_t sreg = SREG;
	cli();
	for (uint8_t c = 0; c < NUM_COLS; c++) {
		matrix[c] = injected[c] | (passthroughOn ? physical[c] : 0);
	}
	mirror();
	SREG = sreg;
}

void watchColumn(uint8_t col)
{
	uint8_t sreg = SREG;
	cli();
	watchMask = (uint16_t) 1 << col;
	watchArmed = 0;
	hits = 0;
	SREG = sreg;
}

void watchOff()
{
	uint8_t sreg = SREG;
	cli();
	watchMask = 0;
	SREG = sreg;
}

uint8_t watchHits()
{
	return hits;
}

}

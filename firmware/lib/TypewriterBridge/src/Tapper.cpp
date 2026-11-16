#include "Tapper.h"

#include "TypewriterPort.h"

#define DEFAULT_HOLD_SCANS 2
#define NO_SCAN_TIMEOUT_MS 500

namespace {

enum State { IDLE, LEAD, HOLD, TRAIL, GAP };

State state = IDLE;
Key keys[MAX_TAP_KEYS];
uint8_t count;
uint16_t gap;
uint8_t hold = DEFAULT_HOLD_SCANS;
unsigned long since;

void enter(State s)
{
	state = s;
	since = millis();
}

const Key & mainKey()
{
	return keys[count - 1];
}

void pressMain()
{
	TypewriterPort::setInjected(mainKey(), true);
	TypewriterPort::watchColumn(mainKey().col);
	TypewriterPort::commit();
	enter(HOLD);
}

void releaseAll(State next)
{
	TypewriterPort::clearInjected();
	TypewriterPort::watchOff();
	TypewriterPort::commit();
	enter(next);
}

}

namespace Tapper {

bool start(const Key * newKeys, uint8_t newCount, uint16_t gapMs)
{
	if (state != IDLE || newCount == 0 || newCount > MAX_TAP_KEYS) {
		return false;
	}
	memcpy(keys, newKeys, newCount * sizeof(Key));
	count = newCount;
	gap = gapMs;

	if (count == 1) {
		pressMain();
		return true;
	}
	/* modifiers first */
	for (uint8_t i = 0; i < count - 1; i++) {
		TypewriterPort::setInjected(keys[i], true);
	}
	TypewriterPort::watchColumn(keys[0].col);
	TypewriterPort::commit();
	enter(LEAD);
	return true;
}

Result poll()
{
	if (state == IDLE) {
		return NONE;
	}
	if (state == GAP) {
		if (millis() - since < gap) {
			return NONE;
		}
		state = IDLE;
		return DONE;
	}
	if (millis() - since > NO_SCAN_TIMEOUT_MS) {
		abort();
		return NO_SCAN;
	}

	uint8_t hits = TypewriterPort::watchHits();
	switch (state) {
	case LEAD:					/* modifiers down, wait until seen */
		if (hits >= 1) {
			pressMain();
		}
		break;
	case HOLD:					/* main key down */
		if (hits >= hold) {
			if (count == 1) {
				releaseAll(GAP);
			} else {
				TypewriterPort::setInjected(mainKey(), false);
				TypewriterPort::watchColumn(mainKey().col);
				TypewriterPort::commit();
				enter(TRAIL);
			}
		}
		break;
	case TRAIL:					/* main key up, modifiers still down */
		if (hits >= 1) {
			releaseAll(GAP);
		}
		break;
	default:
		break;
	}
	return NONE;
}

void abort()
{
	releaseAll(IDLE);
}

bool busy()
{
	return state != IDLE;
}

void setHoldScans(uint8_t scans)
{
	hold = scans;
}

uint8_t holdScans()
{
	return hold;
}

}

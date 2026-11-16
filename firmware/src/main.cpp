/*
 * TypewriterBridge - Arduino Mega 2560 firmware
 *
 * The Mega sits between the keyboard and the mainboard of a Panasonic KX-R191:
 *
 *   keyboard matrix  <-->  Mega  <-->  typewriter keyboard connectors
 *                           ^
 *                           | USB serial
 *                           v
 *                      Raspberry Pi 5
 *
 * All real-time work happens here; all intelligence (character mapping,
 * layout, print jobs) lives on the Pi. Without a Pi the typewriter simply
 * works as before, because passthrough is on after reset.
 *
 * The work is done by lib/TypewriterBridge:
 *   KeyboardScanner scans the physical keyboard
 *   TypewriterPort  plays keyboard towards the typewriter
 *   Tapper          presses keys on behalf of the Pi
 *   Protocol        serial commands and events (docs/protocol.md)
 *
 * Wiring: docs/wiring.md
 */

#include <Arduino.h>

#include <KeyboardScanner.h>
#include <Protocol.h>
#include <TypewriterPort.h>

/* Hand the state of the physical keyboard to the typewriter side. */
static void forwardKeyboard()
{
	for (uint8_t col = 0; col < NUM_COLS; col++) {
		TypewriterPort::setPhysical(col, KeyboardScanner::rows(col));
	}
	TypewriterPort::commit();
}

void setup()
{
	TypewriterPort::begin();
	KeyboardScanner::begin(Protocol::reportKey);
	Protocol::begin();
}

void loop()
{
	Protocol::poll();
	if (KeyboardScanner::poll()) {
		forwardKeyboard();
	}
}

/*
 * Serial protocol towards the Raspberry Pi (see docs/protocol.md).
 *
 * Reads command lines, drives TypewriterPort / Tapper accordingly and
 * reports results and key events.
 */
#pragma once

#include <Arduino.h>

#define FW_VERSION "1"
#define BAUDRATE 115200

namespace Protocol {

void begin();

/* Handle incoming commands and report finished taps. */
void poll();

/* Report a key of the physical keyboard ("D<row><col>" / "U<row><col>"). */
void reportKey(bool down, uint8_t row, uint8_t col);

}

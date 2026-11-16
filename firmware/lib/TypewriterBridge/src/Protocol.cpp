#include "Protocol.h"

#include "Matrix.h"
#include "Tapper.h"
#include "TypewriterPort.h"

#define DEFAULT_GAP_MS 75

namespace {

char line[40];
uint8_t lineLen;
bool lineOverflow;

const char *skipSpaces(const char *p)
{
	while (*p == ' ') {
		p++;
	}
	return p;
}

bool isDigitChar(char c)
{
	return c >= '0' && c <= '9';
}

void ok()
{
	Serial.println(F("ok"));
}

/* T <row><col>[<row><col>...] [gap_ms]; "ok" follows when the tap is done */
void cmdTap(const char *p)
{
	if (Tapper::busy()) {
		Serial.println(F("err busy"));
		return;
	}
	Key keys[MAX_TAP_KEYS];
	uint8_t n = 0;
	p = skipSpaces(p);
	while (isDigitChar(p[0])) {
		if (!isDigitChar(p[1]) || n >= MAX_TAP_KEYS) {
			Serial.println(F("err keys"));
			return;
		}
		int8_t bit = rowToBit(p[0] - '0');
		if (bit < 0) {
			Serial.println(F("err row"));
			return;
		}
		keys[n].col = p[1] - '0';
		keys[n].rowMask = _BV(bit);
		n++;
		p += 2;
	}
	if (n == 0 || (*p != ' ' && *p != '\0')) {
		Serial.println(F("err keys"));
		return;
	}
	p = skipSpaces(p);
	Tapper::start(keys, n, *p ? (uint16_t) atoi(p) : DEFAULT_GAP_MS);
}

/* P 0|1: keyboard passthrough */
void cmdPassthrough(const char *p)
{
	p = skipSpaces(p);
	if (*p != '0' && *p != '1') {
		Serial.println(F("err arg"));
		return;
	}
	TypewriterPort::setPassthrough(*p == '1');
	TypewriterPort::commit();
	ok();
}

/* H <n>: typewriter scan cycles a key is held */
void cmdHoldScans(const char *p)
{
	int n = atoi(skipSpaces(p));
	if (n < 1 || n > 50) {
		Serial.println(F("err arg"));
		return;
	}
	Tapper::setHoldScans(n);
	ok();
}

void cmdStatus()
{
	Serial.print(F("status fw=" FW_VERSION " passthrough="));
	Serial.print(TypewriterPort::passthrough()? 1 : 0);
	Serial.print(F(" scans="));
	Serial.print(Tapper::holdScans());
	Serial.print(F(" busy="));
	Serial.println(Tapper::busy()? 1 : 0);
	ok();
}

void handleLine(const char *p)
{
	p = skipSpaces(p);
	switch (*p) {
	case '\0':
		break;
	case 'T':
		cmdTap(p + 1);
		break;
	case 'P':
		cmdPassthrough(p + 1);
		break;
	case 'H':
		cmdHoldScans(p + 1);
		break;
	case 'X':					/* cancel: release all injected keys */
		Tapper::abort();
		ok();
		break;
	case '?':
		cmdStatus();
		break;
	default:
		Serial.println(F("err cmd"));
		break;
	}
}

void readSerial()
{
	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			if (lineOverflow) {
				Serial.println(F("err long"));
			} else {
				line[lineLen] = '\0';
				handleLine(line);
			}
			lineLen = 0;
			lineOverflow = false;
		} else if (lineLen < sizeof(line) - 1) {
			line[lineLen++] = c;
		} else {
			lineOverflow = true;
		}
	}
}

}

namespace Protocol {

void begin()
{
	Serial.begin(BAUDRATE);
	Serial.println(F("hello typewriter-bridge " FW_VERSION));
}

void poll()
{
	readSerial();

	switch (Tapper::poll()) {
	case Tapper::DONE:
		ok();
		break;
	case Tapper::NO_SCAN:
		Serial.println(F("err noscan"));
		break;
	default:
		break;
	}
}

void reportKey(bool down, uint8_t row, uint8_t col)
{
	Serial.print(down ? 'D' : 'U');
	Serial.print((char) ('0' + row));
	Serial.println((char) ('0' + col));
}

}

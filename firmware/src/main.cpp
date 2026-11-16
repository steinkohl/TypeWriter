/*
 * TypewriterBridge - Arduino Mega 2560 firmware
 *
 * The Mega sits between the keyboard and the mainboard of a Panasonic KX-R191:
 *
 *   keyboard matrix  <-->  Mega  <-->  typewriter keyboard connectors
 *                           ^
 *                           | USB serial (115200 baud)
 *                           v
 *                      Raspberry Pi 5
 *
 * - The Mega scans the physical keyboard itself.
 * - Towards the typewriter it behaves like a keyboard: whenever the typewriter
 *   strobes a column low, the Mega pulls the rows of all "pressed" keys of
 *   that column low (open drain, like a real switch would).
 * - "Pressed" is the physical keyboard state (if passthrough is on) OR'ed
 *   with keys injected by the Pi (printing).
 *
 * All real-time work happens here; all intelligence (character mapping,
 * layout, print jobs) lives on the Pi. Without a Pi the typewriter simply
 * works as before, because passthrough is on after reset.
 *
 * Serial protocol: see docs/protocol.md
 *
 * Wiring (see docs/wiring.md):
 *   typewriter rows 0,1,2,3,6,7,8,9 -> D22..D29 (PA0..PA7)
 *   typewriter cols 0..7            -> A8..A15  (PK0..PK7)
 *   typewriter cols 8,9             -> D53, D52 (PB0, PB1)
 *   keyboard   rows 0,1,2,3,6,7,8,9 -> D37..D30 (PC0..PC7)
 *   keyboard   cols 0..7            -> D49..D42 (PL0..PL7)
 *   keyboard   cols 8,9             -> D41, D40 (PG0, PG1)
 *   row pins 4 (GND) and 5 (caps lock LED) go straight from the typewriter
 *   to the keyboard; pin 4 additionally to Mega GND.
 */

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#define FW_VERSION "1"
#define BAUDRATE 115200

#define NUM_COLS 10
#define NUM_ROWS 10				/* connector pins; 4 and 5 are no matrix lines */

#define SCAN_INTERVAL_US 2000	/* physical keyboard scan period */
#define SETTLE_US 30			/* settle time after selecting a column */
#define DEBOUNCE_SCANS 4		/* identical scans before a change is accepted */
#define NOSCAN_TIMEOUT_MS 500	/* typewriter does not strobe -> give up tap */
#define MAX_TAP_KEYS 4
#define DEFAULT_HOLD_SCANS 2
#define DEFAULT_GAP_MS 75

/* connector row pin -> bit in the row ports (-1: not a matrix line) */
static const int8_t ROW_BIT[NUM_ROWS] = { 0, 1, 2, 3, -1, -1, 4, 5, 6, 7 };
/* bit in the row ports -> connector row pin */
static const uint8_t BIT_ROW[8] = { 0, 1, 2, 3, 6, 7, 8, 9 };

/* ---- state shared with the interrupt handler --------------------------- */

/* What the typewriter shall see: one row bitmask per column */
static volatile uint8_t matrix[NUM_COLS];
static volatile uint16_t prevCols;		/* columns strobed at the last interrupt */
static volatile uint16_t watchMask;		/* column whose strobes are counted */
static volatile uint8_t watchArmed;
static volatile uint8_t watchHits;		/* complete strobes of the watched column */

/* ---- main loop state --------------------------------------------------- */

static uint8_t phys[NUM_COLS];			/* debounced physical keyboard */
static uint8_t physRaw[NUM_COLS];
static uint8_t physStable[NUM_COLS];
static uint8_t inject[NUM_COLS];		/* keys pressed by the Pi */
static bool passthrough = true;
static uint8_t holdScans = DEFAULT_HOLD_SCANS;

typedef struct {
	uint8_t col;
	uint8_t rowMask;
} Key;

typedef enum { TAP_IDLE, TAP_LEAD, TAP_HOLD, TAP_TRAIL, TAP_GAP } TapState;

static TapState tapState = TAP_IDLE;
static Key tapKeys[MAX_TAP_KEYS];
static uint8_t tapCount;
static uint16_t tapGap;
static unsigned long tapSince;

/* ---- typewriter side --------------------------------------------------- */

/*
 * Mirror the matrix to the typewriter rows for the currently strobed
 * column(s). Called from the pin change interrupts and (with interrupts
 * disabled) whenever the matrix changed.
 */
static inline void mirror(void)
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
	/* PORTA is always 0: output = pulled low, input = released (open drain) */
	DDRA = rows;

	uint16_t now = lo | ((uint16_t) hi << 8);
	if (watchMask) {
		if (now & ~prevCols & watchMask) {
			watchArmed = 1;				/* strobe of the watched column began */
		} else if (watchArmed && (prevCols & ~now & watchMask)) {
			watchArmed = 0;				/* ... and ended: one full strobe seen */
			if (watchHits < 255) {
				watchHits++;
			}
		}
	}
	prevCols = now;
}

ISR(PCINT0_vect)
{
	mirror();
}

ISR(PCINT2_vect)
{
	mirror();
}

static void updateMatrix(void)
{
	uint8_t sreg = SREG;
	cli();
	for (uint8_t c = 0; c < NUM_COLS; c++) {
		matrix[c] = inject[c] | (passthrough ? phys[c] : 0);
	}
	mirror();
	SREG = sreg;
}

static void watchColumn(uint8_t col)
{
	uint8_t sreg = SREG;
	cli();
	watchMask = (uint16_t) 1 << col;
	watchArmed = 0;
	watchHits = 0;
	SREG = sreg;
}

static void watchOff(void)
{
	uint8_t sreg = SREG;
	cli();
	watchMask = 0;
	SREG = sreg;
}

/* ---- physical keyboard ------------------------------------------------- */

static inline void kbdSelect(uint8_t col)
{
	/* PORTL / PORTG bits are 0: switching to output drives the line low */
	if (col < 8) {
		DDRL = _BV(col);
	} else {
		DDRG |= _BV(col - 8);
	}
}

static inline void kbdDeselect(void)
{
	DDRL = 0;
	DDRG &= ~0x03;
}

static void printKey(char prefix, uint8_t rowBit, uint8_t col)
{
	Serial.print(prefix);
	Serial.print((char) ('0' + BIT_ROW[rowBit]));
	Serial.println((char) ('0' + col));
}

static void scanKeyboard(void)
{
	bool changed = false;

	for (uint8_t c = 0; c < NUM_COLS; c++) {
		kbdSelect(c);
		delayMicroseconds(SETTLE_US);
		uint8_t rows = ~PINC;
		kbdDeselect();

		if (rows != physRaw[c]) {
			physRaw[c] = rows;
			physStable[c] = 0;
			continue;
		}
		if (physStable[c] < DEBOUNCE_SCANS) {
			physStable[c]++;
			continue;
		}
		uint8_t diff = rows ^ phys[c];
		if (!diff) {
			continue;
		}
		phys[c] = rows;
		changed = true;
		for (uint8_t b = 0; b < 8; b++) {
			if (diff & _BV(b)) {
				printKey((rows & _BV(b)) ? 'D' : 'U', b, c);
			}
		}
	}
	if (changed) {
		updateMatrix();
	}
}

/* ---- injected key taps ------------------------------------------------- */

static void injectKey(const Key * k, bool down)
{
	if (down) {
		inject[k->col] |= k->rowMask;
	} else {
		inject[k->col] &= ~k->rowMask;
	}
}

static void tapEnter(TapState s)
{
	tapState = s;
	tapSince = millis();
}

static void tapAbort(void)
{
	memset(inject, 0, sizeof(inject));
	watchOff();
	updateMatrix();
	tapState = TAP_IDLE;
}

static void tapPressMain(void)
{
	const Key *k = &tapKeys[tapCount - 1];
	injectKey(k, true);
	watchColumn(k->col);
	updateMatrix();
	tapEnter(TAP_HOLD);
}

/*
 * A tap presses all keys but the last one first (modifiers), waits until the
 * typewriter has seen them, then presses the last key for holdScans scan
 * cycles, releases it, and finally releases the modifiers.
 */
static void tapStart(void)
{
	if (tapCount > 1) {
		for (uint8_t i = 0; i < tapCount - 1; i++) {
			injectKey(&tapKeys[i], true);
		}
		watchColumn(tapKeys[0].col);
		updateMatrix();
		tapEnter(TAP_LEAD);
	} else {
		tapPressMain();
	}
}

static void tapRun(void)
{
	if (tapState == TAP_IDLE) {
		return;
	}
	if (tapState == TAP_GAP) {
		if (millis() - tapSince >= tapGap) {
			tapState = TAP_IDLE;
			Serial.println(F("ok"));
		}
		return;
	}
	if (millis() - tapSince > NOSCAN_TIMEOUT_MS) {
		tapAbort();
		Serial.println(F("err noscan"));
		return;
	}

	uint8_t hits = watchHits;
	switch (tapState) {
	case TAP_LEAD:
		if (hits >= 1) {
			tapPressMain();
		}
		break;
	case TAP_HOLD:
		if (hits >= holdScans) {
			injectKey(&tapKeys[tapCount - 1], false);
			if (tapCount > 1) {
				watchColumn(tapKeys[tapCount - 1].col);
				updateMatrix();
				tapEnter(TAP_TRAIL);
			} else {
				watchOff();
				updateMatrix();
				tapEnter(TAP_GAP);
			}
		}
		break;
	case TAP_TRAIL:
		if (hits >= 1) {
			memset(inject, 0, sizeof(inject));
			watchOff();
			updateMatrix();
			tapEnter(TAP_GAP);
		}
		break;
	default:
		break;
	}
}

/* ---- serial commands --------------------------------------------------- */

static char line[40];
static uint8_t lineLen;
static bool lineOverflow;

static const char *skipSpaces(const char *p)
{
	while (*p == ' ') {
		p++;
	}
	return p;
}

static bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

/* T <row><col>[<row><col>...] [gap_ms] */
static void cmdTap(const char *p)
{
	if (tapState != TAP_IDLE) {
		Serial.println(F("err busy"));
		return;
	}
	p = skipSpaces(p);
	uint8_t n = 0;
	while (isDigit(p[0])) {
		if (!isDigit(p[1]) || n >= MAX_TAP_KEYS) {
			Serial.println(F("err keys"));
			return;
		}
		uint8_t row = p[0] - '0';
		uint8_t col = p[1] - '0';
		if (ROW_BIT[row] < 0) {
			Serial.println(F("err row"));
			return;
		}
		tapKeys[n].col = col;
		tapKeys[n].rowMask = _BV(ROW_BIT[row]);
		n++;
		p += 2;
	}
	if (n == 0 || (*p != ' ' && *p != '\0')) {
		Serial.println(F("err keys"));
		return;
	}
	p = skipSpaces(p);
	tapGap = *p ? (uint16_t) atoi(p) : DEFAULT_GAP_MS;
	tapCount = n;
	tapStart();
}

static void printStatus(void)
{
	Serial.print(F("status fw=" FW_VERSION " passthrough="));
	Serial.print(passthrough ? 1 : 0);
	Serial.print(F(" scans="));
	Serial.print(holdScans);
	Serial.print(F(" busy="));
	Serial.println(tapState != TAP_IDLE ? 1 : 0);
}

static void handleLine(const char *p)
{
	p = skipSpaces(p);
	switch (*p) {
	case '\0':
		break;
	case 'T':
		cmdTap(p + 1);			/* answers "ok" when the tap is finished */
		break;
	case 'P':					/* P 0|1: keyboard passthrough */
		p = skipSpaces(p + 1);
		if (*p != '0' && *p != '1') {
			Serial.println(F("err arg"));
			break;
		}
		passthrough = (*p == '1');
		updateMatrix();
		Serial.println(F("ok"));
		break;
	case 'H':{					/* H <n>: scan cycles a key is held */
			int n = atoi(skipSpaces(p + 1));
			if (n < 1 || n > 50) {
				Serial.println(F("err arg"));
				break;
			}
			holdScans = n;
			Serial.println(F("ok"));
			break;
		}
	case 'X':					/* cancel: release all injected keys */
		tapAbort();
		Serial.println(F("ok"));
		break;
	case '?':
		printStatus();
		Serial.println(F("ok"));
		break;
	default:
		Serial.println(F("err cmd"));
		break;
	}
}

static void readSerial(void)
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

/* ---- setup / loop ------------------------------------------------------ */

void setup()
{
	/* typewriter rows: open drain, released */
	PORTA = 0;
	DDRA = 0;
	/* typewriter columns: inputs with pull-up */
	DDRK = 0;
	PORTK = 0xFF;
	DDRB &= ~0x03;
	PORTB |= 0x03;
	/* keyboard rows: inputs with pull-up */
	DDRC = 0;
	PORTC = 0xFF;
	/* keyboard columns: open drain, released */
	PORTL = 0;
	DDRL = 0;
	PORTG &= ~0x03;
	DDRG &= ~0x03;

	/* pin change interrupts on all typewriter column lines */
	PCMSK2 = 0xFF;
	PCMSK0 = 0x03;
	PCICR = _BV(PCIE2) | _BV(PCIE0);

	Serial.begin(BAUDRATE);
	Serial.println(F("hello typewriter-bridge " FW_VERSION));
}

void loop()
{
	static unsigned long lastScan;

	readSerial();
	tapRun();

	unsigned long now = micros();
	if (now - lastScan >= SCAN_INTERVAL_US) {
		lastScan = now;
		scanKeyboard();
	}
}

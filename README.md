# Typewriter as a printer – Panasonic KX-R191

An electronic daisy-wheel typewriter (Panasonic KX-R191) that

- can still be used normally **with its own keyboard**, and
- shows up on the network as a **printer**.

![Finished build](images/final.jpg)

## Architecture

```
 Keyboard ──► Arduino Mega 2560 ──► typewriter mainboard
                     ▲
                     │ USB (serial)
                     ▼
              Raspberry Pi 5  ◄── network: print jobs (port 9100 / CUPS)
```

| Part | Role | Code |
|------|------|------|
| **Arduino Mega** | Real time: scans the keyboard matrix, emulates a keyboard towards the typewriter, passes key presses through and taps the keys the Pi asks for. Knows no characters, only matrix positions. | `firmware/` |
| **Raspberry Pi 5** | The brain: accepts print jobs, wraps text, translates characters into key sequences (umlauts, accents, substitutes) and drives the Mega. | `pi/` |

The keyboard is unplugged from the mainboard and connected to the Mega; the
Mega is connected to the keyboard connectors of the mainboard. Details:
[docs/wiring.md](docs/wiring.md). The protocol between Pi and Mega is described
in [docs/protocol.md](docs/protocol.md).

**Behaviour**

- No print job: the keyboard is passed through 1:1 – even without a Pi
  (passthrough is always on after power-up).
- While a job is printing, the keyboard is locked.
  **Pressing the Code key cancels the job.**
- If the typewriter is switched off, the Mega notices (`err noscan`) and the
  job is dropped instead of being typed into the void.

## Setup

### 1. Wiring

See [docs/wiring.md](docs/wiring.md).

### 2. Mega firmware

The firmware is a [PlatformIO](https://platformio.org/) project. On the Pi
(or any other computer):

```bash
pipx install platformio
```

```bash
cd firmware && pio run -t upload
```

The serial port is detected automatically; otherwise append
`--upload-port /dev/ttyACM0`. If `typewriterd` is already running, stop it
first with `sudo systemctl stop typewriterd`, because the service holds the
port. In VS Code, opening the `firmware/` folder with the PlatformIO extension
is enough.

Firmware layout: `firmware/src/main.cpp` only wires together the modules of
the library `firmware/lib/TypewriterBridge`:

| Module | Role |
|--------|------|
| `KeyboardScanner` | scan and debounce the physical keyboard |
| `TypewriterPort` | emulate the keyboard towards the typewriter (interrupt) |
| `Tapper` | press keys on behalf of the Pi |
| `Protocol` | serial commands and events |
| `Matrix.h` | shared definitions of the key matrix |

After flashing, the typewriter must work normally again with its own
keyboard – that is the first test, without any Pi software.

### 3. Service on the Pi

```bash
cd pi && sudo ./install.sh --cups
```

This installs `typewriterd` as a systemd service (listening on port 9100) and –
with `--cups` – a CUPS queue "Typewriter" that converts documents to plain
text. Settings (serial port, line width, delays) go into
`/etc/default/typewriterd`, e.g.:

```
TYPEWRITERD_OPTS="--device /dev/ttyACM0 --width 65 --key-gap 90"
```

All options: `typewriterd --help`.

## Printing

```bash
echo "Hello world" | typewriter-print
```

```bash
typewriter-print letter.txt --host raspberrypi.local
```

```bash
nc raspberrypi.local 9100 < letter.txt
```

```bash
lp -d Typewriter letter.txt
```

From other computers: add a printer of type "AppSocket/JetDirect" with the
address `socket://<pi>:9100` and the "Generic Text-Only" driver, or use the
Pi's shared CUPS queue.

Jobs are plain text (UTF-8). Long lines are wrapped at 70 characters.
Characters the daisy wheel does not have are replaced
(“” → ", – → -, € → EUR, otherwise `?`); é/è/ê etc. are typed using the accent
dead key. These control characters are also allowed in the text:

| Character | Typewriter function |
|-----------|---------------------|
| `\f` (0x0C) | page feed |
| 0x0E | bold on/off |
| 0x0F | underline on/off |
| 0x11 | automatic line feed |
| 0x12 / 0x13 | set left / right margin |

## Testing without hardware

```bash
cd pi && python3 -m unittest discover -s tests
```

```bash
cd pi && python3 -m typewriter.daemon --simulate --port 9100
```

In simulation mode the service "types" into the terminal instead of onto paper.

## Background

The KX-R191 does have a printer port, but it needs a converter (Panasonic
KX-R60) that is no longer available. Instead, this project emulates the
keyboard (idea: https://hackaday.io/project/18506/instructions).

![Mainboard](images/circuit-board.jpg)

### The keyboard matrix

The keyboard has two connectors with 10 pins each and is a simple switch
matrix: left connector = columns, right connector = rows. Connecting e.g. pin 3
on the left with pin 0 on the right makes the typewriter type an "e".

```
+-----+-----+-------+-----------+-----+----------+-----+-----+-------+-------+----------------+
| R\L |  0  |   1   |     2     |  3  |    4     |  5  |  6  |   7   |   8   |       9        |
+-----+-----+-------+-----------+-----+----------+-----+-----+-------+-------+----------------+
|   0 | 1   | 2     | r         | e   | g        | h   |     | #     |       |                |
|   1 | ⇼   |       | q         | w   | f        | d   |     | b     | n     |                |
|   2 | 4   | 3     | t         | z   | k        | j   |     | ,     | m     |                |
|   3 | 5   | 6     | Tab       |     | a        | s   |     | v     | c     | Space          |
|   4 |                                      GND                                              |
|   5 |                                  CAPS LOCK LED                                        |
|   6 | 8   | 7     | i         | u   | l        | ö   |     | .     | -     | Tab+           |
|   7 | 9   | 0     | o         | p   | CapsLock |     |     | y     | x     | Code           |
|   8 | ´   | ß     | +         | ü   | Return   | ä   |     | Shift |       | QuickEraseLine |
|   9 | ←   | Print | Backspace |     |          |     |     |       | Reloc | ↨              |
+-----+-----+-------+-----------+-----+----------+-----+-----+-------+-------+----------------+
```

(Columns = pins of the left connector, rows = pins of the right connector.)

### How the typewriter scans the keyboard

The mainboard pulls each column low in turn for about 200 µs and checks
whether a row goes low with it – if so, the key at that crossing is pressed.

The Mega keeps an internal matrix of "which keys should be pressed right now"
(physical keyboard OR keys tapped by the Pi). A pin change interrupt on the
column lines immediately pulls the matching rows low on every strobe. That way
held keys, Shift, key repeat etc. work exactly as with the original keyboard.

## Status

The firmware compiles for the Mega 2560, and the Pi side is tested against a
simulated Mega. This rebuild has **not yet** been tried on the real machine –
the timings (`--key-gap`, `--return-gap`, `--hold-scans`) may need tuning.

Based on the original "Typewriter Arduino project" by Xou (printing via the
Arduino only, without keyboard passthrough).

# Wiring

The keyboard ribbon cable is **unplugged** from the typewriter mainboard. The
Arduino Mega sits in between: the keyboard is connected to the Mega, and the
Mega is connected to the two keyboard connectors of the mainboard.

```
 Keyboard (matrix)                Arduino Mega 2560              KX-R191 mainboard
 ┌───────────────┐   10 columns  ┌─────────────────┐  10 columns ┌──────────────────┐
 │ left connector├──────────────►│ D49..D40        │◄────────────┤ left connector   │
 │               │               │        A8..A15, │             │                  │
 │               │    8 rows     │        D53, D52 │   8 rows    │                  │
 │ right conn.   ├──────────────►│ D37..D30        ├────────────►│ right connector  │
 │               │               │        D22..D29 │             │                  │
 │   pin 4 (GND) ├───────────────┼──── GND ────────┼─────────────┤ pin 4 (GND)      │
 │   pin 5 (LED) ├───────────────┼─────────────────┼─────────────┤ pin 5 (caps LED) │
 └───────────────┘               └────────┬────────┘             └──────────────────┘
                                          │ USB
                                  ┌───────┴────────┐
                                  │ Raspberry Pi 5 │
                                  └────────────────┘
```

"Column" = pin of the left connector, "row" = pin of the right connector
(numbered as in the matrix table in the README, 0 = top).

## Typewriter side (mainboard)

| Connector | Pin | Mega pin | Port |
|-----------|-----|----------|------|
| left (columns) | 0 … 7 | A8 … A15 | PK0 … PK7 |
| left (columns) | 8 | D53 | PB0 |
| left (columns) | 9 | D52 | PB1 |
| right (rows) | 0, 1, 2, 3 | D22, D23, D24, D25 | PA0 … PA3 |
| right (rows) | 6, 7, 8, 9 | D26, D27, D28, D29 | PA4 … PA7 |
| right | 4 (GND) | GND | – |
| right | 5 (caps lock LED) | – (straight to the keyboard) | – |

## Keyboard side

| Connector | Pin | Mega pin | Port |
|-----------|-----|----------|------|
| left (columns) | 0 … 7 | D49, D48, D47, D46, D45, D44, D43, D42 | PL0 … PL7 |
| left (columns) | 8 | D41 | PG0 |
| left (columns) | 9 | D40 | PG1 |
| right (rows) | 0, 1, 2, 3 | D37, D36, D35, D34 | PC0 … PC3 |
| right (rows) | 6, 7, 8, 9 | D33, D32, D31, D30 | PC4 … PC7 |
| right | 4 (GND) | GND (shared with the typewriter) | – |
| right | 5 (caps lock LED) | – (straight to the mainboard) | – |

## Notes

- **Column 6** has no keys and can be left unconnected on both sides.
- **Pins 4 and 5** of the right connector (ground and caps lock LED) are
  connected 1:1 from the mainboard to the keyboard. The typewriter itself
  keeps driving the LED. Also connect pin 4 to the Mega's GND.
- The pins are tied to fixed ports (the firmware reads/writes whole ports so
  it reacts fast enough to the typewriter's 200 µs strobes). To use other
  pins, adapt the port accesses in `firmware/lib/TypewriterBridge/src/`
  (`TypewriterPort.cpp`, `KeyboardScanner.cpp`).
- Towards the typewriter the Mega behaves like a switch (open drain): it only
  pulls rows to ground or releases them, it never actively drives 5 V.
- 5 V logic on the mainboard is assumed (the previous build ran that way). If
  in doubt, measure a column pin first.
- The Mega is powered via USB from the Pi. The Pi 5 itself has **no** direct
  connection to the typewriter (its GPIOs are 3.3 V only).

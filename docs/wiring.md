# Verdrahtung

Das Tastatur-Flachbandkabel wird von der Hauptplatine der Schreibmaschine
**abgezogen**. Der Arduino Mega sitzt dazwischen: Die Tastatur hängt am Mega,
und der Mega hängt an den beiden Tastatur-Steckern der Hauptplatine.

```
 Tastatur (Matrix)                Arduino Mega 2560              Hauptplatine KX-R191
 ┌───────────────┐   10 Spalten  ┌─────────────────┐  10 Spalten ┌──────────────────┐
 │ linker Stecker├──────────────►│ D49..D40        │◄────────────┤ linker Stecker   │
 │               │               │        A8..A15, │             │                  │
 │               │    8 Zeilen   │        D53, D52 │   8 Zeilen  │                  │
 │ rechter Steck.├──────────────►│ D37..D30        ├────────────►│ rechter Stecker  │
 │               │               │        D22..D29 │             │                  │
 │   Pin 4 (GND) ├───────────────┼──── GND ────────┼─────────────┤ Pin 4 (GND)      │
 │   Pin 5 (LED) ├───────────────┼─────────────────┼─────────────┤ Pin 5 (Caps-LED) │
 └───────────────┘               └────────┬────────┘             └──────────────────┘
                                          │ USB
                                  ┌───────┴────────┐
                                  │ Raspberry Pi 5 │
                                  └────────────────┘
```

„Spalte“ = Pin des linken Steckers, „Zeile“ = Pin des rechten Steckers
(Nummerierung wie in der Matrix-Tabelle im README, 0 = oben).

## Seite Schreibmaschine (Hauptplatine)

| Stecker | Pin | Mega-Pin | Port |
|---------|-----|----------|------|
| links (Spalten) | 0 … 7 | A8 … A15 | PK0 … PK7 |
| links (Spalten) | 8 | D53 | PB0 |
| links (Spalten) | 9 | D52 | PB1 |
| rechts (Zeilen) | 0, 1, 2, 3 | D22, D23, D24, D25 | PA0 … PA3 |
| rechts (Zeilen) | 6, 7, 8, 9 | D26, D27, D28, D29 | PA4 … PA7 |
| rechts | 4 (GND) | GND | – |
| rechts | 5 (Caps-Lock-LED) | – (direkt zur Tastatur) | – |

## Seite Tastatur

| Stecker | Pin | Mega-Pin | Port |
|---------|-----|----------|------|
| links (Spalten) | 0 … 7 | D49, D48, D47, D46, D45, D44, D43, D42 | PL0 … PL7 |
| links (Spalten) | 8 | D41 | PG0 |
| links (Spalten) | 9 | D40 | PG1 |
| rechts (Zeilen) | 0, 1, 2, 3 | D37, D36, D35, D34 | PC0 … PC3 |
| rechts (Zeilen) | 6, 7, 8, 9 | D33, D32, D31, D30 | PC4 … PC7 |
| rechts | 4 (GND) | GND (gemeinsam mit Schreibmaschine) | – |
| rechts | 5 (Caps-Lock-LED) | – (direkt zur Hauptplatine) | – |

## Hinweise

- **Spalte 6** hat keine Tasten und kann auf beiden Seiten offen bleiben.
- **Pin 4 und 5** des rechten Steckers (Masse und Caps-Lock-LED) werden 1:1
  von der Hauptplatine zur Tastatur durchverbunden. Die LED steuert weiterhin
  die Schreibmaschine selbst. Pin 4 zusätzlich mit GND des Mega verbinden.
- Die Pins sind fest an die Ports gebunden (die Firmware liest/schreibt ganze
  Ports, damit sie schnell genug auf die 200-µs-Abfrageimpulse der
  Schreibmaschine reagiert). Wer andere Pins will, muss die Port-Zugriffe in
  `firmware/TypewriterBridge/TypewriterBridge.ino` anpassen.
- Zur Schreibmaschine hin verhält sich der Mega wie ein Schalter (Open Drain):
  Er zieht Zeilen nur auf Masse oder lässt sie los, er treibt nie aktiv 5 V.
- Vorausgesetzt wird 5-V-Logik auf der Hauptplatine (so lief auch der
  bisherige Aufbau). Im Zweifel vorher an einem Spaltenpin nachmessen.
- Der Mega wird über USB vom Pi versorgt. Der Pi 5 selbst hat **keine**
  direkte Verbindung zur Schreibmaschine (seine GPIOs vertragen nur 3,3 V).

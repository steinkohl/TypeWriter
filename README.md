# Schreibmaschine als Drucker – Panasonic KX-R191

Eine elektronische Typenrad-Schreibmaschine (Panasonic KX-R191), die

- sich weiterhin ganz normal **über ihre eigene Tastatur** bedienen lässt und
- im Netzwerk als **Drucker** auftaucht.

![Fertiger Aufbau](images/final.jpg)

## Aufbau

```
 Tastatur ──► Arduino Mega 2560 ──► Hauptplatine der Schreibmaschine
                     ▲
                     │ USB (seriell)
                     ▼
              Raspberry Pi 5  ◄── Netzwerk: Druckaufträge (Port 9100 / CUPS)
```

| Teil | Aufgabe | Code |
|------|---------|------|
| **Arduino Mega** | Echtzeit: liest die Tastaturmatrix aus, spielt der Schreibmaschine eine Tastatur vor, reicht Tastendrücke durch und tippt die Tasten, die der Pi verlangt. Kennt keine Buchstaben, nur Matrixpositionen. | `firmware/` |
| **Raspberry Pi 5** | Das Hirn: nimmt Druckaufträge an, bricht Text um, übersetzt Zeichen in Tastenfolgen (Umlaute, Akzente, Ersatzzeichen), steuert den Mega. | `pi/` |

Die Tastatur wird dazu von der Hauptplatine abgezogen und an den Mega gesteckt;
der Mega hängt an den Tastatursteckern der Hauptplatine. Details:
[docs/wiring.md](docs/wiring.md). Das Protokoll zwischen Pi und Mega steht in
[docs/protocol.md](docs/protocol.md).

**Verhalten**

- Kein Druckauftrag: Die Tastatur wird 1:1 durchgereicht – auch ohne Pi
  (nach dem Einschalten ist das Durchreichen immer an).
- Während eines Druckauftrags ist die Tastatur gesperrt.
  **Code-Taste drücken bricht den Druck ab.**
- Ist die Schreibmaschine ausgeschaltet, merkt das der Mega (`err noscan`)
  und der Auftrag wird verworfen statt ins Leere getippt.

## Inbetriebnahme

### 1. Verdrahten

Siehe [docs/wiring.md](docs/wiring.md).

### 2. Firmware auf den Mega

Die Firmware ist ein [PlatformIO](https://platformio.org/)-Projekt. Auf dem Pi
(oder jedem anderen Rechner):

```bash
pipx install platformio
```

```bash
cd firmware && pio run -t upload
```

Der serielle Port wird automatisch erkannt; sonst `--upload-port /dev/ttyACM0`
anhängen. Läuft `typewriterd` schon, vorher mit
`sudo systemctl stop typewriterd` anhalten, weil der Dienst den Port belegt.
In VS Code genügt es, den Ordner `firmware/` mit der PlatformIO-Erweiterung zu
öffnen.

Aufbau der Firmware: `firmware/src/main.cpp` verdrahtet nur die Module der
Bibliothek `firmware/lib/TypewriterBridge`:

| Modul | Aufgabe |
|-------|---------|
| `KeyboardScanner` | echte Tastatur abfragen und entprellen |
| `TypewriterPort` | der Schreibmaschine die Tastatur vorspielen (Interrupt) |
| `Tapper` | Tasten im Auftrag des Pi drücken |
| `Protocol` | serielle Kommandos und Meldungen |
| `Matrix.h` | gemeinsame Definitionen der Tastenmatrix |

Danach muss die Schreibmaschine über ihre Tastatur wieder normal funktionieren –
das ist der erste Test, noch ganz ohne Pi-Software.

### 3. Dienst auf dem Pi

```bash
cd pi && sudo ./install.sh --cups
```

Das installiert `typewriterd` als systemd-Dienst (lauscht auf Port 9100) und –
mit `--cups` – eine CUPS-Warteschlange „Schreibmaschine“, die Dokumente in
reinen Text umwandelt. Einstellungen (serieller Port, Zeilenbreite, Pausen)
kommen nach `/etc/default/typewriterd`, z. B.:

```
TYPEWRITERD_OPTS="--device /dev/ttyACM0 --width 65 --key-gap 90"
```

Alle Optionen: `typewriterd --help`.

## Drucken

```bash
echo "Hallo Welt" | typewriter-print
```

```bash
typewriter-print brief.txt --host raspberrypi.local
```

```bash
nc raspberrypi.local 9100 < brief.txt
```

```bash
lp -d Schreibmaschine brief.txt
```

Von anderen Rechnern: Drucker als „AppSocket/JetDirect“ mit Adresse
`socket://<pi>:9100` und Treiber „Generic Text-Only“ einrichten, oder die
freigegebene CUPS-Warteschlange des Pi benutzen.

Gedruckt wird reiner Text (UTF-8). Lange Zeilen werden auf 70 Zeichen
umgebrochen. Zeichen, die das Typenrad nicht hat, werden ersetzt
(„“ → ", – → -, € → EUR, sonst `?`); é/è/ê usw. werden über die Akzent-Tottaste
getippt. Im Text sind außerdem diese Steuerzeichen erlaubt:

| Zeichen | Funktion der Schreibmaschine |
|---------|------------------------------|
| `\f` (0x0C) | Seitenvorschub |
| 0x0E | Fettdruck an/aus |
| 0x0F | Unterstreichen an/aus |
| 0x11 | automatischer Zeilenvorschub |
| 0x12 / 0x13 | linken / rechten Rand setzen |

## Ohne Hardware testen

```bash
cd pi && python3 -m unittest discover -s tests
```

```bash
cd pi && python3 -m typewriter.daemon --simulate --port 9100
```

Im Simulationsmodus „tippt“ der Dienst ins Terminal statt auf Papier.

## Hintergrund

Die KX-R191 hat zwar einen Druckeranschluss, braucht dafür aber einen heute
nicht mehr erhältlichen Konverter (Panasonic KX-R60). Stattdessen wird hier die
Tastatur emuliert (Idee: https://hackaday.io/project/18506/instructions).

![Hauptplatine](images/circuit-board.jpg)

### Die Tastaturmatrix

Die Tastatur hat zwei Stecker mit je 10 Pins und ist eine einfache
Schaltermatrix: linker Stecker = Spalten, rechter Stecker = Zeilen. Verbindet
man z. B. Pin 3 links mit Pin 0 rechts, tippt die Schreibmaschine ein „e“.

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

(Spalten = Pins des linken Steckers, Zeilen = Pins des rechten Steckers.)

### Wie die Schreibmaschine die Tastatur abfragt

Die Hauptplatine zieht reihum jede Spalte für ca. 200 µs auf Masse und schaut,
ob dabei eine Zeile mit auf Masse geht – dann ist die Taste an dieser Kreuzung
gedrückt.

Der Mega hält intern eine Matrix „welche Tasten sollen gerade gedrückt sein“
(echte Tastatur ODER vom Pi getippte Tasten). Ein Pin-Change-Interrupt auf den
Spaltenleitungen zieht bei jedem Abfrageimpuls sofort die passenden Zeilen auf
Masse. Dadurch funktionieren auch gehaltene Tasten, Shift, Tastenwiederholung
usw. genau wie mit der Originaltastatur.

## Stand

Firmware kompiliert für den Mega 2560, die Pi-Seite ist gegen einen simulierten
Mega getestet. Am echten Gerät wurde dieser Umbau **noch nicht** erprobt – die
Zeiten (`--key-gap`, `--return-gap`, `--hold-scans`) müssen ggf. angepasst werden.

Basiert auf dem ursprünglichen „Typewriter Arduino project“ von Xou (nur Drucken
über den Arduino, ohne Tastatur-Durchreichung).

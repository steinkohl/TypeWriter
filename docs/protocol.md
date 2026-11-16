# Serielles Protokoll Pi ⇄ Mega

USB-seriell, 115200 Baud, 8N1. ASCII-Zeilen, abgeschlossen mit `\n`.
Eine Taste wird als zwei Ziffern `<Zeile><Spalte>` geschrieben (z. B. `03` = „e“,
`87` = Shift), siehe Matrix-Tabelle im README.

Der Pi schickt immer genau ein Kommando und wartet auf `ok` oder `err …`
(Stop-and-Wait, damit ist die Flusskontrolle erledigt).

## Pi → Mega

| Kommando | Bedeutung |
|----------|-----------|
| `T <Tasten> [Pause_ms]` | Taste antippen. Bei mehreren Tasten (max. 4) sind alle bis auf die letzte Modifier: Sie werden zuerst gedrückt und zuletzt losgelassen. `T 8703 75` = Shift+e, danach 75 ms Pause. `ok` kommt erst, wenn die Taste losgelassen und die Pause vorbei ist. |
| `P 0` / `P 1` | Durchreichen der echten Tastatur aus/an (nach Reset: an). |
| `H <n>` | Anzahl Abfragezyklen der Schreibmaschine, die eine Taste gehalten wird (1–50, Standard 2). |
| `X` | Abbrechen: alle vom Pi gedrückten Tasten loslassen. |
| `?` | Status: `status fw=1 passthrough=1 scans=2 busy=0`, danach `ok`. |

## Mega → Pi

| Zeile | Bedeutung |
|-------|-----------|
| `hello typewriter-bridge <Version>` | nach Reset (der Mega startet neu, sobald der Pi den Port öffnet) |
| `ok` | Kommando ausgeführt |
| `err <Grund>` | `noscan` (Schreibmaschine fragt die Tastatur nicht ab – ausgeschaltet?), `busy`, `keys`, `row`, `arg`, `cmd`, `long` |
| `D<Zeile><Spalte>` | Taste der echten Tastatur gedrückt (kommt jederzeit, auch bei `P 0`) |
| `U<Zeile><Spalte>` | Taste losgelassen |

## Zum Ausprobieren

```bash
cd firmware && pio device monitor
```

Dann z. B. `T 03` eingeben: Die Schreibmaschine tippt ein „e“.

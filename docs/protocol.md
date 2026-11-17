# Serial protocol Pi ⇄ Mega

USB serial, 115200 baud, 8N1. ASCII lines terminated by `\n`.
A key is written as two digits `<row><col>` (e.g. `03` = "e", `87` = Shift),
see the matrix table in the README.

The Pi always sends exactly one command and waits for `ok` or `err …`
(stop-and-wait, which also takes care of flow control).

## Pi → Mega

| Command | Meaning |
|---------|---------|
| `T <keys> [gap_ms]` | Tap a key. With several keys (max. 4), all but the last are modifiers: they are pressed first and released last. `T 8703 75` = Shift+e, then a 75 ms pause. `ok` is sent only once the key has been released and the pause is over. |
| `P 0` / `P 1` | Physical keyboard passthrough off/on (on after reset). |
| `H <n>` | Number of typewriter scan cycles a key is held (1–50, default 2). |
| `X` | Cancel: release all keys pressed by the Pi. |
| `?` | Status: `status fw=1 passthrough=1 scans=2 busy=0`, then `ok`. |

## Mega → Pi

| Line | Meaning |
|------|---------|
| `hello typewriter-bridge <version>` | after reset (the Mega restarts when the Pi opens the port) |
| `ok` | command done |
| `err <reason>` | `noscan` (typewriter is not scanning the keyboard – switched off?), `busy`, `keys`, `row`, `arg`, `cmd`, `long` |
| `D<row><col>` | key of the physical keyboard pressed (sent at any time, also with `P 0`) |
| `U<row><col>` | key released |

## Trying it by hand

```bash
cd firmware && pio device monitor
```

Then type e.g. `T 03`: the typewriter types an "e".

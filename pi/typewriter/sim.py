"""A fake Mega + typewriter for tests and ``typewriterd --simulate``.

Speaks the firmware's serial protocol and turns the key taps it receives
back into text, so the whole Pi side can run without hardware.
"""

from __future__ import annotations

import queue
import sys

from . import keymap


def _reverse() -> dict[tuple[keymap.Key, ...], str]:
    table: dict[tuple[keymap.Key, ...], str] = {}
    for ch, key in keymap.BASE.items():
        table[(key,)] = ch
    for ch, key in keymap.SHIFTED.items():
        table[(keymap.SHIFT, key)] = ch
    for ch, key in keymap.CODED.items():
        table[(keymap.CODE, key)] = ch
    for ch, key in keymap.FUNCTIONS.items():
        table[(keymap.CODE, key)] = ch
    table[(keymap.RETURN,)] = "\n"
    table[(keymap.DEAD_ACCENT,)] = "´"
    table[(keymap.SHIFT, keymap.DEAD_ACCENT)] = "`"
    table[(keymap.CURSOR_LEFT,)] = "\b"
    return table


class FakeMega:
    """File-like object usable as the ``port`` of :class:`typewriter.link.Link`."""

    def __init__(self, echo: bool = False, scanning: bool = True):
        self._out: queue.Queue[bytes] = queue.Queue()
        self._reverse = _reverse()
        self._echo = echo
        self.scanning = scanning  # False: typewriter switched off
        self.passthrough = True
        self.hold_scans = 2
        self.typed: list[str] = []
        self.commands: list[str] = []
        self.gaps: list[int] = []
        self._say("hello typewriter-bridge sim")

    # -- serial port interface --------------------------------------------
    def readline(self) -> bytes:
        try:
            return self._out.get(timeout=0.05)
        except queue.Empty:
            return b""

    def write(self, data: bytes) -> int:
        for line in data.decode("ascii").splitlines():
            self.commands.append(line)
            self._command(line.strip())
        return len(data)

    def close(self) -> None:
        pass

    # -- test helpers ------------------------------------------------------
    def press(self, key: keymap.Key, down: bool = True) -> None:
        """Simulate a key of the physical keyboard."""
        self._say(f"{'D' if down else 'U'}{key[0]}{key[1]}")

    @property
    def text(self) -> str:
        return "".join(self.typed)

    # -- firmware behaviour ------------------------------------------------
    def _say(self, line: str) -> None:
        self._out.put(line.encode("ascii") + b"\n")

    def _command(self, line: str) -> None:
        if not line:
            return
        cmd, _, arg = line.partition(" ")
        arg = arg.strip()
        if cmd == "T":
            self._tap(arg)
        elif cmd == "P" and arg in ("0", "1"):
            self.passthrough = arg == "1"
            self._say("ok")
        elif cmd == "H" and arg.isdigit() and 1 <= int(arg) <= 50:
            self.hold_scans = int(arg)
            self._say("ok")
        elif cmd == "X":
            self._say("ok")
        elif cmd == "?":
            self._say(
                f"status fw=sim passthrough={int(self.passthrough)} "
                f"scans={self.hold_scans} busy=0"
            )
            self._say("ok")
        else:
            self._say("err cmd")

    def _tap(self, arg: str) -> None:
        code, _, gap = arg.partition(" ")
        if not code or len(code) % 2 or not code.isdigit() or len(code) > 8:
            self._say("err keys")
            return
        keys = tuple((int(code[i]), int(code[i + 1])) for i in range(0, len(code), 2))
        if any(row in (4, 5) for row, _ in keys):
            self._say("err row")
            return
        if not self.scanning:
            self._say("err noscan")
            return
        ch = self._reverse.get(keys, "�")
        self.typed.append(ch)
        self.gaps.append(int(gap) if gap.strip() else 75)
        if self._echo:
            sys.stdout.write(ch)
            sys.stdout.flush()
        self._say("ok")

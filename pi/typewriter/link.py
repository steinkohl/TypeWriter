"""Serial link to the Mega (protocol: docs/protocol.md)."""

from __future__ import annotations

import logging
import queue
import threading
from typing import Callable, Iterable

from .keymap import Key

log = logging.getLogger(__name__)

BAUDRATE = 115200
KeyHandler = Callable[[bool, Key], None]


class LinkError(Exception):
    pass


class Link:
    """Stop-and-wait command channel plus asynchronous key events.

    ``port`` is anything with ``readline()``, ``write()`` and ``close()``;
    ``readline()`` must return ``b""`` after a short timeout.
    """

    def __init__(self, port, on_key: KeyHandler | None = None):
        self._port = port
        self._on_key = on_key
        self._replies: queue.Queue[str] = queue.Queue()
        self._hello = threading.Event()
        self._lock = threading.Lock()
        self._closed = False
        self.status: dict[str, str] = {}
        self._reader = threading.Thread(target=self._read_loop, name="link-reader", daemon=True)
        self._reader.start()

    @classmethod
    def open(cls, device: str, on_key: KeyHandler | None = None, timeout: float = 10.0) -> "Link":
        import serial  # pyserial; imported late so tests run without it

        port = serial.Serial(device, BAUDRATE, timeout=0.2)
        link = cls(port, on_key)
        # Opening the port resets the Mega; wait for its greeting.
        if not link.wait_hello(timeout):
            link.close()
            raise LinkError(f"no greeting from firmware on {device}")
        return link

    # -- reader thread -----------------------------------------------------
    def _read_loop(self) -> None:
        try:
            while not self._closed:
                raw = self._port.readline()
                if not raw:
                    continue
                line = raw.decode("ascii", "replace").strip()
                if line:
                    self._handle(line)
        except Exception as exc:  # port vanished (USB unplugged, ...)
            if not self._closed:
                log.error("serial link lost: %s", exc)
                self._replies.put(f"err link {exc}")

    def _handle(self, line: str) -> None:
        if line == "ok" or line.startswith("err"):
            self._replies.put(line)
        elif len(line) == 3 and line[0] in "DU" and line[1:].isdigit():
            if self._on_key:
                try:
                    self._on_key(line[0] == "D", (int(line[1]), int(line[2])))
                except Exception:
                    log.exception("key handler failed")
        elif line.startswith("hello"):
            log.info("firmware: %s", line)
            self._hello.set()
        elif line.startswith("status"):
            self.status = dict(item.split("=", 1) for item in line.split()[1:] if "=" in item)
        else:
            log.debug("firmware says: %s", line)

    # -- commands ----------------------------------------------------------
    def wait_hello(self, timeout: float) -> bool:
        return self._hello.wait(timeout)

    def command(self, line: str, timeout: float = 2.0) -> None:
        with self._lock:
            while not self._replies.empty():  # stale replies
                self._replies.get_nowait()
            try:
                self._port.write(line.encode("ascii") + b"\n")
            except Exception as exc:
                raise LinkError(f"write failed: {exc}") from exc
            try:
                reply = self._replies.get(timeout=timeout)
            except queue.Empty:
                raise LinkError(f"no reply to {line!r}") from None
            if reply != "ok":
                raise LinkError(f"{line!r} -> {reply}")

    def tap(self, keys: Iterable[Key], gap_ms: int) -> None:
        """Press a key (preceded by its modifiers) and wait until it is done."""
        code = "".join(f"{row}{col}" for row, col in keys)
        self.command(f"T {code} {int(gap_ms)}", timeout=3.0 + gap_ms / 1000)

    def set_passthrough(self, on: bool) -> None:
        self.command(f"P {1 if on else 0}")

    def set_hold_scans(self, scans: int) -> None:
        self.command(f"H {scans}")

    def cancel(self) -> None:
        self.command("X")

    def query_status(self) -> dict[str, str]:
        self.command("?")
        return self.status

    def close(self) -> None:
        self._closed = True
        try:
            self._port.close()
        except Exception:
            pass

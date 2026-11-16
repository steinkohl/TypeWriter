"""typewriterd - the brain on the Raspberry Pi.

Owns the serial link to the Mega and offers the typewriter as a network
printer: every TCP connection on port 9100 (raw / AppSocket / "JetDirect")
is one plain-text print job. CUPS, ``nc`` and ``typewriter-print`` can all
talk to that.

While no job is running, the Mega passes the physical keyboard through to
the typewriter. During a job the keyboard is locked; pressing the Code key
cancels the job.
"""

from __future__ import annotations

import argparse
import logging
import queue
import socketserver
import threading
import time
from dataclasses import dataclass

from . import keymap
from .link import Link, LinkError
from .text import decode, prepare

log = logging.getLogger("typewriterd")

MAX_JOB_BYTES = 1 << 20


@dataclass
class Config:
    device: str = "/dev/ttyACM0"
    host: str = "0.0.0.0"
    port: int = 9100
    width: int = 70
    key_gap_ms: int = 75
    return_gap_ms: int = 300
    return_gap_per_col_ms: int = 0
    hold_scans: int = 2
    leading_newline: bool = True
    simulate: bool = False


class JobCancelled(Exception):
    pass


class Typewriter:
    """Types print jobs, one after the other."""

    def __init__(self, config: Config, port=None):
        self.config = config
        self._port = port  # injected fake port (tests / --simulate)
        self._link: Link | None = None
        self._jobs: queue.Queue[str] = queue.Queue()
        self._cancel = threading.Event()
        self._printing = False
        self._stop = threading.Event()

    # -- public ------------------------------------------------------------
    def submit(self, data: bytes) -> None:
        text = prepare(decode(data), self.config.width)
        if not text:
            log.info("empty job ignored")
            return
        log.info("job queued: %d characters", len(text))
        self._jobs.put(text)

    def stop(self) -> None:
        self._stop.set()
        self._cancel.set()

    def wait_idle(self) -> None:
        self._jobs.join()

    def run(self) -> None:
        """Worker loop: keep the link up and type queued jobs."""
        while not self._stop.is_set():
            try:
                text = self._jobs.get(timeout=0.5)
            except queue.Empty:
                self._ensure_link(quiet=True)
                continue
            try:
                self._print(text)
            except JobCancelled:
                log.warning("job cancelled from the keyboard")
            except LinkError as exc:
                log.error("job aborted: %s", exc)
                self._drop_link()
            finally:
                self._jobs.task_done()
        self._drop_link()

    # -- link handling -----------------------------------------------------
    def _ensure_link(self, quiet: bool = False) -> Link | None:
        if self._link:
            return self._link
        try:
            if self._port is not None:
                link = Link(self._port, self._on_key)
                if not link.wait_hello(5):
                    raise LinkError("no greeting from firmware")
            else:
                link = Link.open(self.config.device, self._on_key)
            link.set_hold_scans(self.config.hold_scans)
            link.set_passthrough(True)
        except Exception as exc:
            if not quiet:
                log.error("cannot reach the Mega: %s", exc)
            else:
                log.debug("cannot reach the Mega: %s", exc)
                self._stop.wait(3)
            return None
        log.info("connected to the Mega")
        self._link = link
        return link

    def _drop_link(self) -> None:
        if self._link:
            self._link.close()
            self._link = None

    def _on_key(self, down: bool, key: keymap.Key) -> None:
        log.debug("key %s %s", "down" if down else "up", key)
        if down and key == keymap.CODE and self._printing:
            self._cancel.set()

    # -- printing ----------------------------------------------------------
    def _print(self, text: str) -> None:
        link = self._ensure_link()
        if link is None:
            raise LinkError("typewriter not connected, job dropped")
        cfg = self.config
        if cfg.leading_newline:
            text = "\n" + text
        taps = keymap.taps_for_text(text)
        log.info("printing: %d key presses", len(taps))
        self._cancel.clear()
        self._printing = True
        link.set_passthrough(False)
        started = time.monotonic()
        try:
            column = 0
            for tap in taps:
                if self._cancel.is_set():
                    link.cancel()
                    raise JobCancelled()
                if tap.is_return:
                    gap = cfg.return_gap_ms + cfg.return_gap_per_col_ms * column
                    column = 0
                else:
                    gap = cfg.key_gap_ms
                    column += 1
                link.tap(tap.keys, gap)
        finally:
            self._printing = False
            try:
                link.set_passthrough(True)
            except LinkError:
                pass
        log.info("job done in %.0f s", time.monotonic() - started)


class _JobHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        self.request.settimeout(30)
        chunks: list[bytes] = []
        size = 0
        try:
            while size <= MAX_JOB_BYTES:
                chunk = self.request.recv(65536)
                if not chunk:
                    break
                chunks.append(chunk)
                size += len(chunk)
        except OSError as exc:
            log.warning("receiving job from %s failed: %s", self.client_address[0], exc)
            return
        if size > MAX_JOB_BYTES:
            log.warning("job from %s too large, rejected", self.client_address[0])
            return
        log.info("job received from %s (%d bytes)", self.client_address[0], size)
        self.server.typewriter.submit(b"".join(chunks))  # type: ignore[attr-defined]


class JobServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address, typewriter: Typewriter):
        super().__init__(address, _JobHandler)
        self.typewriter = typewriter


def parse_args(argv=None) -> Config:
    d = Config()
    p = argparse.ArgumentParser(description="Panasonic KX-R191 typewriter print server")
    p.add_argument("--device", default=d.device, help="serial port of the Mega (%(default)s)")
    p.add_argument("--host", default=d.host, help="listen address (%(default)s)")
    p.add_argument("--port", type=int, default=d.port, help="listen port (%(default)s)")
    p.add_argument("--width", type=int, default=d.width, help="characters per line (%(default)s)")
    p.add_argument("--key-gap", type=int, default=d.key_gap_ms, metavar="MS",
                   help="pause after each character (%(default)s)")
    p.add_argument("--return-gap", type=int, default=d.return_gap_ms, metavar="MS",
                   help="pause after a carriage return (%(default)s)")
    p.add_argument("--return-gap-per-col", type=int, default=d.return_gap_per_col_ms, metavar="MS",
                   help="extra return pause per character of the line (%(default)s)")
    p.add_argument("--hold-scans", type=int, default=d.hold_scans,
                   help="typewriter scan cycles a key is held (%(default)s)")
    p.add_argument("--no-leading-newline", action="store_true",
                   help="do not start a job with a carriage return")
    p.add_argument("--simulate", action="store_true",
                   help="no hardware: print to the terminal instead")
    p.add_argument("-v", "--verbose", action="store_true")
    a = p.parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if a.verbose else logging.INFO,
        format="%(levelname)s %(name)s: %(message)s",
    )
    return Config(
        device=a.device, host=a.host, port=a.port, width=a.width, key_gap_ms=a.key_gap,
        return_gap_ms=a.return_gap, return_gap_per_col_ms=a.return_gap_per_col,
        hold_scans=a.hold_scans, leading_newline=not a.no_leading_newline, simulate=a.simulate,
    )


def main(argv=None) -> None:
    config = parse_args(argv)
    port = None
    if config.simulate:
        from .sim import FakeMega

        port = FakeMega(echo=True)
    typewriter = Typewriter(config, port)
    server = JobServer((config.host, config.port), typewriter)
    threading.Thread(target=server.serve_forever, name="job-server", daemon=True).start()
    log.info("listening on %s:%d", config.host, config.port)
    try:
        typewriter.run()
    except KeyboardInterrupt:
        pass
    finally:
        typewriter.stop()
        server.shutdown()


if __name__ == "__main__":
    main()

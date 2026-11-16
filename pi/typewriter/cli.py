"""typewriter-print - send a text file (or stdin) to typewriterd."""

from __future__ import annotations

import argparse
import socket
import sys


def main(argv=None) -> None:
    p = argparse.ArgumentParser(description="Print text on the typewriter")
    p.add_argument("file", nargs="?", help="text file (default: stdin)")
    p.add_argument("--host", default="localhost", help="host running typewriterd (%(default)s)")
    p.add_argument("--port", type=int, default=9100)
    a = p.parse_args(argv)

    if a.file:
        with open(a.file, "rb") as f:
            data = f.read()
    else:
        data = sys.stdin.buffer.read()
    with socket.create_connection((a.host, a.port), timeout=10) as sock:
        sock.sendall(data)


if __name__ == "__main__":
    main()

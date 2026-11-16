"""Prepare incoming print data for the typewriter."""

from __future__ import annotations

import textwrap

from .keymap import FUNCTIONS

_KEEP = set(FUNCTIONS) | {"\n"}


def decode(data: bytes) -> str:
    """Print jobs should be UTF-8; fall back to Latin-1 for old senders."""
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("latin-1")


def prepare(text: str, width: int = 70, tabsize: int = 8) -> str:
    """Normalise line ends, drop control characters and wrap to ``width``.

    Form feeds and the typewriter function codes (see keymap.FUNCTIONS)
    are kept. The result ends with a newline unless it is empty or ends
    with a form feed.
    """
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    out: list[str] = []
    for line in text.split("\n"):
        line = "".join(c for c in line if c >= " " and c != "\x7f" or c in _KEEP or c == "\t")
        line = line.expandtabs(tabsize).rstrip(" ")
        # control codes do not move the carriage: do not count them
        printable = sum(1 for c in line if c not in FUNCTIONS)
        if printable <= width:
            out.append(line)
        else:
            out += textwrap.wrap(
                line, width, replace_whitespace=False, break_long_words=True
            ) or [""]
    while out and out[-1] == "":
        out.pop()
    result = "\n".join(out)
    if result and not result.endswith("\x0c"):
        result += "\n"
    return result

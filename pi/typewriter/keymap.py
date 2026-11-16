"""Key matrix of the Panasonic KX-R191 (German layout) and character mapping.

A key is a ``(row, col)`` tuple: ``row`` is the pin of the right keyboard
connector, ``col`` the pin of the left one. This is the only place that knows
which character lives where; the Mega firmware just presses matrix positions.
"""

from __future__ import annotations

import unicodedata
from dataclasses import dataclass

Key = tuple[int, int]

# --- special keys ---------------------------------------------------------
SHIFT: Key = (8, 7)
CODE: Key = (7, 9)
SPACE: Key = (3, 9)
RETURN: Key = (8, 4)
DEAD_ACCENT: Key = (8, 0)  # ´ (with shift: `)
CURSOR_LEFT: Key = (9, 0)  # moves back without erasing
CAPS_LOCK: Key = (7, 4)

# --- unshifted layer ------------------------------------------------------
BASE: dict[str, Key] = {
    "1": (0, 0), "2": (0, 1), "r": (0, 2), "e": (0, 3), "g": (0, 4), "h": (0, 5), "#": (0, 7),
    "q": (1, 2), "w": (1, 3), "f": (1, 4), "d": (1, 5), "b": (1, 7), "n": (1, 8),
    "4": (2, 0), "3": (2, 1), "t": (2, 2), "z": (2, 3), "k": (2, 4), "j": (2, 5), ",": (2, 7),
    "m": (2, 8),
    "5": (3, 0), "6": (3, 1), "a": (3, 4), "s": (3, 5), "v": (3, 7), "c": (3, 8), " ": SPACE,
    "8": (6, 0), "7": (6, 1), "i": (6, 2), "u": (6, 3), "l": (6, 4), "ö": (6, 5), ".": (6, 7),
    "-": (6, 8),
    "9": (7, 0), "0": (7, 1), "o": (7, 2), "p": (7, 3), "y": (7, 7), "x": (7, 8),
    "ß": (8, 1), "+": (8, 2), "ü": (8, 3), "ä": (8, 5),
}  # fmt: skip

# --- shifted layer (letters are derived from BASE) ------------------------
SHIFTED: dict[str, Key] = {
    "!": (0, 0), '"': (0, 1), "'": (0, 7), "$": (2, 0), "§": (2, 1), ";": (2, 7),
    "%": (3, 0), "&": (3, 1), "(": (6, 0), "/": (6, 1), ":": (6, 7), "_": (6, 8),
    ")": (7, 0), "=": (7, 1), "?": (8, 1), "*": (8, 2),
}  # fmt: skip
SHIFTED.update({ch.upper(): key for ch, key in BASE.items() if ch.upper() != ch and ch != "ß"})

# --- Code layer -----------------------------------------------------------
CODED: dict[str, Key] = {"µ": (3, 1)}

# Typewriter functions (Code + key), usable as control characters in a job.
FUNCTIONS: dict[str, Key] = {
    "\x0c": RETURN,  # form feed: page feed
    "\x0e": (1, 7),  # SO:  bold on/off
    "\x0f": (6, 3),  # SI:  underline on/off
    "\x11": (7, 0),  # DC1: auto line feed
    "\x12": (0, 0),  # DC2: set left margin
    "\x13": (0, 1),  # DC3: set right margin
}

# Characters the daisy wheel does not have, replaced by something close.
FALLBACK: dict[str, str] = {
    "“": '"', "”": '"', "„": '"', "«": '"', "»": '"',
    "‘": "'", "’": "'", "‚": "'", "`": "'",
    "–": "-", "—": "-", "−": "-", "‐": "-", "‑": "-",
    "…": "...", " ": " ", "•": "-", "€": "EUR",
    "@": "(at)", "[": "(", "]": ")", "{": "(", "}": ")", "<": "(", ">": ")",
    "|": "/", "\\": "/", "~": "-", "^": "'",
}  # fmt: skip

_COMBINING = {"́": "acute", "̀": "grave", "̂": "circumflex"}


@dataclass(frozen=True)
class Tap:
    """One key press: all ``keys`` but the last are modifiers held meanwhile."""

    keys: tuple[Key, ...]
    is_return: bool = False


def _plain(ch: str) -> Tap | None:
    if ch in BASE:
        return Tap((BASE[ch],))
    if ch in SHIFTED:
        return Tap((SHIFT, SHIFTED[ch]))
    if ch in CODED:
        return Tap((CODE, CODED[ch]))
    return None


def _accent(kind: str) -> list[Tap]:
    acute = Tap((DEAD_ACCENT,))
    grave = Tap((SHIFT, DEAD_ACCENT))
    if kind == "acute":
        return [acute]
    if kind == "grave":
        return [grave]
    # No circumflex on the wheel: strike ´ and ` over each other. A dead key
    # only prints with the next character, so print ´ on a space, step back,
    # and let ` combine with the letter.
    return [acute, Tap((SPACE,)), Tap((CURSOR_LEFT,)), grave]


def taps_for_char(ch: str) -> list[Tap] | None:
    """Key presses that print ``ch``, or ``None`` if it cannot be printed."""
    if ch == "\n":
        return [Tap((RETURN,), is_return=True)]
    if ch in FUNCTIONS:
        return [Tap((CODE, FUNCTIONS[ch]), is_return=(ch == "\x0c"))]
    tap = _plain(ch)
    if tap:
        return [tap]
    decomposed = unicodedata.normalize("NFD", ch)
    if len(decomposed) == 2 and decomposed[1] in _COMBINING and decomposed[0].isalpha():
        letter = _plain(decomposed[0])
        if letter:
            return _accent(_COMBINING[decomposed[1]]) + [letter]
    return None


def taps_for_text(text: str, unknown: str = "?") -> list[Tap]:
    """Key presses for a whole text. Unprintable characters are replaced."""
    taps: list[Tap] = []
    for ch in unicodedata.normalize("NFC", text):
        found = taps_for_char(ch)
        if found is None:
            replacement = FALLBACK.get(ch)
            if replacement is None:
                # e.g. ç -> c, ñ -> n: drop the marks we cannot print
                stripped = "".join(
                    c for c in unicodedata.normalize("NFKD", ch) if not unicodedata.combining(c)
                )
                replacement = stripped if stripped and stripped != ch else unknown
            found = []
            for r in replacement:
                found += taps_for_char(r) or taps_for_char(unknown) or []
        taps += found
    return taps

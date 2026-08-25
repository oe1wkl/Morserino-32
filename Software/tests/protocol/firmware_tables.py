#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
firmware_tables.py -- the expected answers, read out of the firmware source.

A test that hand-copies the Koch sequences is a test that quietly stops
checking anything the day someone edits a sequence. So the ground truth is
parsed from the firmware itself:

  * the four character sequences and the adaptiveProbabilities[] capacity
    from MorsePreferences.h
  * the prosign display table from cleanUpProSigns() in m32_v6.ino

Exactly one piece of firmware logic is MIRRORED here rather than read:
`licw_sequence()`, the Python twin of Koch::setupLICWkochChars(). It is a
dozen lines of pure array arithmetic and has not changed since it was written,
but it is a copy -- if the firmware's carousel rotation ever changes, this
must change with it. Everything the mirror feeds is still cross-checked
against invariants that do not depend on it (see run_tests.py, tier 1).

Run this file directly to dump what it found:

    python3 firmware_tables.py
"""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "..", "src", "Version 6 and newer"))
PREFS_H = os.path.join(SRC, "MorsePreferences.h")
MAIN_INO = os.path.join(SRC, "m32_v6.ino")

# Koch Sequence preference values (MorsePreferences.cpp, pliste[posKochSeq])
SEQ_M32, SEQ_LCWO, SEQ_CWAC, SEQ_LICW, SEQ_CUSTOM = range(5)

SEQUENCE_NAMES = {
    SEQ_M32: "M32",
    SEQ_LCWO: "LCWO",
    SEQ_CWAC: "CW Academy",
    SEQ_LICW: "LICW Carousel",
    SEQ_CUSTOM: "Custom Chars",
}

# Which source constant backs each built-in sequence.
SEQUENCE_CONSTANT = {
    SEQ_M32: "morserinoKochChars",
    SEQ_LCWO: "lcwoKochChars",
    SEQ_CWAC: "cwacKochChars",
}

# LICW Carousel entry points (pliste[posCarouselStart]): 0..13, 0-5 are BC1.
LICW_ENTRY_POINTS = list(range(14))
LICW_FIRST_BC2 = 6


class SourceError(RuntimeError):
    pass


def _read(path):
    if not os.path.exists(path):
        raise SourceError("cannot find firmware source: %s" % path)
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def koch_constants():
    """{name: sequence string} for every `const char* const ...KochChars`."""
    text = _read(PREFS_H)
    found = dict(re.findall(
        r'const\s+char\s*\*\s*const\s+(\w*[Kk]ochChars)\s*=\s*"((?:[^"\\]|\\.)*)"\s*;',
        text))
    missing = [n for n in ("morserinoKochChars", "lcwoKochChars",
                           "cwacKochChars", "licwAllKochChars") if n not in found]
    if missing:
        raise SourceError("MorsePreferences.h no longer defines: %s" % ", ".join(missing))
    return found


def probability_capacity():
    """Size of adaptiveProbabilities[], which caps every sequence's maximum."""
    text = _read(PREFS_H)
    match = re.search(r'uint8_t\s+adaptiveProbabilities\s*\[\s*(\d+)\s*\]', text)
    if not match:
        raise SourceError("cannot find adaptiveProbabilities[] in MorsePreferences.h")
    return int(match.group(1))


def prosign_table():
    """{single char: display form} as cleanUpProSigns() renders them."""
    text = _read(MAIN_INO)
    start = text.find("String cleanUpProSigns")
    if start < 0:
        raise SourceError("cannot find cleanUpProSigns() in m32_v6.ino")
    block = text[start:start + 2000]
    pairs = re.findall(r"\{\s*'(.)'\s*,\s*\"([^\"]*)\"\s*,\s*\d+\s*\}", block)
    if not pairs:
        raise SourceError("cannot parse the prosign table in cleanUpProSigns()")
    return dict(pairs)


def licw_sequence(all_chars, start):
    """MIRROR of Koch::setupLICWkochChars() -- see the module docstring.

    Returns (sequence, length). BC1 entry points (start < 6) rotate within the
    first 18 characters; BC2 entry points keep those 18 as a core and rotate
    the remaining 26 behind them.
    """
    if start < LICW_FIRST_BC2:                       # BC 1
        seg_len = 18 - 3 * start
        out = all_chars[3 * start:3 * start + seg_len]
        if start > 0:
            out += all_chars[0:3 * start]
        length = 18
    else:                                            # BC 2
        out = all_chars[0:18]
        seg_len = 44 - 3 * start
        out += all_chars[3 * start:3 * start + seg_len]
        if start > LICW_FIRST_BC2:
            out += all_chars[18:18 + (3 * start - 18)]
        length = 44
    return out, length


def expected_bounds(sequence, carousel_start=0, custom_length=None):
    """(minimum, maximum) the firmware should report for this sequence.

    Mirrors setKochChars()/setCustomChars(): every sequence but LICW BC2 has a
    minimum of 1, and the maximum is the sequence length capped to the
    adaptiveProbabilities[] capacity.
    """
    capacity = probability_capacity()
    if sequence == SEQ_LICW:
        _, length = licw_sequence(koch_constants()["licwAllKochChars"], carousel_start)
        return (19 if length > 18 else 1), length
    if sequence == SEQ_CUSTOM:
        if custom_length is None:
            raise ValueError("custom_length is required for the custom sequence")
        return 1, min(custom_length, capacity)
    return 1, capacity


def expected_characters(sequence, carousel_start=0, custom_chars=None):
    """The exact `characters` array GET kochlesson should return.

    The whole sequence, from its first character, with prosigns expanded --
    "value" and "maximum" are absolute positions in it, so the array has to be
    absolute too or a client cannot line the three up.
    """
    constants = koch_constants()
    if sequence == SEQ_LICW:
        chars, length = licw_sequence(constants["licwAllKochChars"], carousel_start)
        chars = chars[:length]
    elif sequence == SEQ_CUSTOM:
        if custom_chars is None:
            raise ValueError("custom_chars is required for the custom sequence")
        chars = custom_chars[:probability_capacity()]
    else:
        _, maximum = expected_bounds(sequence)
        chars = constants[SEQUENCE_CONSTANT[sequence]][:maximum]

    table = prosign_table()
    return [table.get(c, c) for c in chars]


if __name__ == "__main__":
    caps = probability_capacity()
    print("adaptiveProbabilities[] capacity : %d" % caps)
    print("prosign display table            : %s" % prosign_table())
    print()
    for seq in (SEQ_M32, SEQ_LCWO, SEQ_CWAC):
        lo, hi = expected_bounds(seq)
        print("%-14s min=%-3d max=%-3d  %s"
              % (SEQUENCE_NAMES[seq], lo, hi, "".join(expected_characters(seq))))
    print()
    for start in LICW_ENTRY_POINTS:
        lo, hi = expected_bounds(SEQ_LICW, start)
        band = "BC1" if start < LICW_FIRST_BC2 else "BC2"
        print("LICW %-3s start=%-2d min=%-3d max=%-3d  %s"
              % (band, start, lo, hi,
                 "".join(expected_characters(SEQ_LICW, start))))

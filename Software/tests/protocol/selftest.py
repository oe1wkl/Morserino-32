#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
selftest.py -- check the checks, with no device attached.

run_tests.py needs a Morserino on the end of a cable, which means it cannot run
in CI and cannot prove, on its own, that its assertions would actually catch
anything. This replays real `GET kochlesson` replies captured from a device and
asserts the verdict the harness reaches on each:

  * a known-good reading must pass every check
  * the known-BAD reading -- captured from firmware before commit 9e3ffb6, when
    the characters array started at "minimum" instead of at the first character
    of the sequence -- must fail, and fail on the invariant that describes the
    bug (len(characters) == maximum)

So a green selftest means the harness still has teeth, whether or not any
hardware is around. Add a fixture here whenever a new protocol bug is found:
the captured reply is the regression test.

    python3 selftest.py        # exits non-zero if the harness lost its teeth
"""

import sys

import firmware_tables as fw
from run_tests import Report, check_reading


# --------------------------------------------------------------- fixtures

# M32 sequence, firmware V9.0 BETA, protocol 1.4. Correct in every respect.
GOOD_M32 = {
    "value": 5, "minimum": 1, "maximum": 51,
    "characters": ["m", "k", "r", "s", "u", "a", "p", "t", "l", "o", "w", "i",
                   ".", "n", "j", "e", "f", "0", "y", "v", ",", "g", "5", "/",
                   "q", "9", "z", "h", "3", "8", "b", "?", "4", "2", "7", "c",
                   "1", "d", "6", "x", "-", "=", "<sk>", "+", "<as>", "<kn>",
                   "<ka>", "<ve>", "<bk>", "@", ":"],
}

# LICW Carousel, entry point 6 ("BC2: k m y"), captured BEFORE 9e3ffb6.
# The array starts at position 19 instead of position 1, so the 18 characters
# of the BC1 core are missing and it is 26 long where "maximum" says 44.
BAD_LICW_BC2_TRUNCATED = {
    "value": 19, "minimum": 19, "maximum": 44,
    "characters": ["k", "m", "y", "5", "9", ",", "q", "x", "v", "7", "3", "?",
                   "+", "<sk>", "=", "1", "6", ".", "z", "j", "/", "2", "8",
                   "<bk>", "4", "0"],
}

# What that same reading should look like now.
GOOD_LICW_BC2 = {
    "value": 19, "minimum": 19, "maximum": 44,
    "characters": fw.expected_characters(fw.SEQ_LICW, 6),
}


def verdict(reading, sequence, carousel_start=0):
    """Run the harness's per-reading checks and return (passed, failed labels)."""
    report = Report()
    check_reading(report, reading, "fixture",
                  fw.expected_characters(sequence, carousel_start),
                  fw.expected_bounds(sequence, carousel_start))
    return report.passed, [label for label, _ in report.failures]


def main():
    problems = []

    print("-- a correct M32 reading must pass everything")
    passed, failed = verdict(GOOD_M32, fw.SEQ_M32)
    if failed:
        problems.append("the good M32 fixture failed: %s" % failed)
    print("   %d checks passed, %d failed\n" % (passed, len(failed)))

    print("-- a correct LICW BC2 reading must pass everything")
    passed, failed = verdict(GOOD_LICW_BC2, fw.SEQ_LICW, 6)
    if failed:
        problems.append("the good LICW fixture failed: %s" % failed)
    print("   %d checks passed, %d failed\n" % (passed, len(failed)))

    print("-- the truncated LICW reading must be REJECTED")
    passed, failed = verdict(BAD_LICW_BC2_TRUNCATED, fw.SEQ_LICW, 6)
    if not failed:
        problems.append("the harness accepted the known-bad LICW reading -- "
                        "it would no longer catch the 9e3ffb6 bug")
    elif not any("len(characters) == maximum" in label for label in failed):
        problems.append("the known-bad reading was rejected, but not by the "
                        "len(characters) == maximum invariant: %s" % failed)
    print("   %d checks passed, %d failed (failing is the point here)\n"
          % (passed, len(failed)))

    print("=" * 70)
    if problems:
        for problem in problems:
            print("SELFTEST FAILURE: %s" % problem)
        print("=" * 70)
        return 1
    print("selftest OK -- the checks still catch what they were written for")
    print("=" * 70)
    return 0


if __name__ == "__main__":
    sys.exit(main())

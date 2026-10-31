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
from trainer_events import check_cw_done, check_trainer_result, sounded_dits


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


# Protocol 1.6 trainer events. Written from the firmware source (MorseJSON::jsonCwDone /
# jsonTrainerResult) before the first bench run; replace with replies captured from a
# classic M32 and an M32 Pocket once trainer_events.py has run on both.
GOOD_EVENTS = [
    # captured from an M32 Pocket (10.0, protocol 1.6) on 2026-10-30
    (check_cw_done, {"cw": {"event": "done", "reason": "end"}}),
    (check_cw_done, {"cw": {"event": "done", "reason": "stopped"}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "729", "answer": "729",
                                        "ok": True, "repeat": 0, "wpm": 19, "latency": 1045, "duration": 3493}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "126", "answer": "1ech*",
                                        "ok": False, "repeat": 0, "wpm": 19, "latency": 748, "duration": 3674}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "126", "answer": "",
                                        "ok": False, "repeat": 2, "wpm": 19, "timeout": True}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "338", "answer": "",
                                        "ok": False, "repeat": 1, "wpm": 19, "latency": 837, "duration": 2594}}),
    # captured from a classic M32 (heltec_wifi_lora_32_V2, 10.0, protocol 1.6) on 2026-10-31: punctuation in the
    # prompt, and one word asked four times - timeout, timeout, wrong, timeout
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "8k", "answer": "8k",
                                        "ok": True, "repeat": 0, "wpm": 18, "latency": 857, "duration": 1928}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": ",:y@", "answer": "",
                                        "ok": False, "repeat": 1, "wpm": 18, "timeout": True}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": ",:y@", "answer": "7yyy",
                                        "ok": False, "repeat": 2, "wpm": 18, "latency": 553, "duration": 4017}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "learn", "prompt": "u", "answer": "4",
                                        "ok": False, "repeat": 0, "wpm": 18, "koch": 5, "latency": 923, "duration": 820}}),
    # Koch Learn New Chr, captured from the Pocket on 2026-10-31 (koch field fixed)
    (check_trainer_result, {"trainer": {"event": "result", "mode": "learn", "prompt": "b", "answer": "b",
                                        "ok": True, "repeat": 0, "wpm": 19, "koch": 17, "latency": 790, "duration": 651}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "learn", "prompt": "b", "answer": "z",
                                        "ok": False, "repeat": 0, "wpm": 19, "koch": 17, "latency": 1578, "duration": 778}}),
    # written from the source: the koch field in echo mode and prosigns are not captured yet
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "cq", "answer": "cq",
                                        "ok": True, "repeat": 0, "wpm": 18, "koch": 12,
                                        "latency": 820, "duration": 2100}}),
    (check_trainer_result, {"trainer": {"event": "result", "mode": "echo", "prompt": "<sk>", "answer": "<ve>",
                                        "ok": False, "repeat": 1, "wpm": 18,
                                        "latency": 430, "duration": 900}}),
]

# Each must be rejected -- the name says what the validator has to notice.
BAD_EVENTS = [
    ("unknown reason", check_cw_done, {"cw": {"event": "done", "reason": "finished"}}),
    ("missing reason", check_cw_done, {"cw": {"event": "done"}}),
    ("ok contradicts the texts", check_trainer_result,
     {"trainer": {"event": "result", "mode": "echo", "prompt": "cq", "answer": "cg", "ok": True,
                  "repeat": 0, "wpm": 18, "latency": 800, "duration": 900}}),
    ("timeout with timing", check_trainer_result,
     {"trainer": {"event": "result", "mode": "echo", "prompt": "cq", "answer": "", "ok": False,
                  "repeat": 0, "wpm": 18, "timeout": True, "latency": 0, "duration": 0}}),
    ("answer without timing", check_trainer_result,
     {"trainer": {"event": "result", "mode": "echo", "prompt": "cq", "answer": "cq", "ok": True,
                  "repeat": 0, "wpm": 18}}),
    ("timing as text", check_trainer_result,
     {"trainer": {"event": "result", "mode": "echo", "prompt": "cq", "answer": "cq", "ok": True,
                  "repeat": 0, "wpm": 18, "latency": "820", "duration": 2100}}),
    # the firmware sent exactly this before 2026-10-31: a learn result without its lesson
    ("learn without koch", check_trainer_result,
     {"trainer": {"event": "result", "mode": "learn", "prompt": "b", "answer": "b", "ok": True,
                  "repeat": 0, "wpm": 19, "latency": 1418, "duration": 652}}),
    # an unanswered learn repetition sends nothing - this fixture stood among the GOOD ones until the bench showed it
    ("learn timeout", check_trainer_result,
     {"trainer": {"event": "result", "mode": "learn", "prompt": "r", "answer": "", "ok": False,
                  "repeat": 2, "wpm": 18, "koch": 3, "timeout": True}}),
]


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

    print("-- well-formed protocol 1.6 trainer events must pass")
    for check, obj in GOOD_EVENTS:
        found = check(obj)
        if found:
            problems.append("a good trainer event was rejected: %s -> %s" % (obj, found))
    print("   %d fixtures\n" % len(GOOD_EVENTS))

    print("-- malformed trainer events must be REJECTED")
    for name, check, obj in BAD_EVENTS:
        if not check(obj):
            problems.append("the validator accepted a bad trainer event (%s): %s" % (name, obj))
    print("   %d fixtures\n" % len(BAD_EVENTS))

    print("-- the cw/done timing yardstick: PARIS is 50 dits with its word space")
    if sounded_dits("paris", 3, 7) + 7 != 50:
        problems.append("sounded_dits('paris') = %d, PARIS must be 43 + 7" % sounded_dits("paris", 3, 7))
    if sounded_dits("paris paris", 3, 7) != 93:
        problems.append("sounded_dits('paris paris') = %d, want 43 + 7 + 43" % sounded_dits("paris paris", 3, 7))
    print()

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

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_tests.py -- protocol conformance tests against a connected Morserino.

Drives the real firmware over USB serial and checks that what it reports about
the Koch character sequences actually hangs together. Written after three
related bugs in one week, each of which this would have caught:

  * the word/abbrev tables ignoring the Koch lesson under Custom Chars
  * PUT kochlesson neither rebuilding those tables nor persisting the value
  * GET kochlesson truncating "characters" for LICW Carousel BC2, so a client
    lining it up against "value" marked the wrong characters as learned

The checks come in three tiers:

  tier 1  invariants -- relations that must hold whatever the sequences are
          (len(characters) == maximum, minimum <= value <= maximum, the array
          not moving when the lesson changes). These need no knowledge of the
          firmware's tables and so can never go stale.

  tier 2  ground truth -- the exact array, derived from the firmware's own
          source by firmware_tables.py rather than hand-copied here.

  tier 3  the lesson setter -- round trip, range rejection, and survival of a
          reboot.

Usage:

    ./run_tests.py                     # auto-detect the port, run everything
    ./run_tests.py --port /dev/cu.usbmodem1101
    ./run_tests.py --skip-persistence  # no reboot (it costs ~15 s)
    ./run_tests.py --include-custom-chars
    ./run_tests.py -v                  # echo the wire traffic

Exits non-zero if anything failed. Close any serial monitor first -- the port
takes one user at a time.
"""

import argparse
import sys
import traceback

import firmware_tables as fw
from m32_link import M32Link, ProtocolError, find_port


class Report:
    """Collects results so one failure does not hide the rest."""

    def __init__(self):
        self.passed = 0
        self.failures = []
        self.skipped = []

    def check(self, ok, label, detail=""):
        if ok:
            self.passed += 1
            print("  ok    %s" % label)
        else:
            self.failures.append((label, detail))
            print("  FAIL  %s" % label)
            if detail:
                for line in str(detail).splitlines():
                    print("          %s" % line)
        return ok

    def equal(self, got, want, label):
        if got == want:
            return self.check(True, label)
        return self.check(False, label, "expected: %r\ngot:      %r" % (want, got))

    def skip(self, label, why):
        self.skipped.append((label, why))
        print("  skip  %s  (%s)" % (label, why))

    def summary(self):
        print()
        print("=" * 70)
        print("%d passed, %d failed, %d skipped"
              % (self.passed, len(self.failures), len(self.skipped)))
        if self.failures:
            print()
            print("Failures:")
            for label, _ in self.failures:
                print("  - %s" % label)
        print("=" * 70)
        return 1 if self.failures else 0


# ---------------------------------------------------------------- the checks

def check_reading(report, reading, label, want_chars, want_bounds):
    """Tier 1 + tier 2 on one GET kochlesson reply."""
    value = reading.get("value")
    minimum = reading.get("minimum")
    maximum = reading.get("maximum")
    chars = reading.get("characters")

    if not isinstance(chars, list):
        report.check(False, "%s: characters is an array" % label, repr(chars))
        return

    # -- tier 1: invariants
    report.equal(len(chars), maximum,
                 "%s: len(characters) == maximum" % label)
    report.check(isinstance(minimum, int) and isinstance(maximum, int)
                 and isinstance(value, int)
                 and 1 <= minimum <= value <= maximum,
                 "%s: 1 <= minimum <= value <= maximum" % label,
                 "minimum=%r value=%r maximum=%r" % (minimum, value, maximum))
    report.check(all(isinstance(c, str) and c for c in chars),
                 "%s: no empty entries" % label)

    # -- tier 2: ground truth
    report.equal(chars, want_chars, "%s: characters match the firmware tables" % label)
    report.equal((minimum, maximum), want_bounds, "%s: bounds match" % label)


def check_lesson_independence(report, m32, label):
    """Tier 1: the character array describes the sequence, not the lesson.

    The bug that prompted all this was an array whose *length* depended on
    something other than the sequence. Moving the lesson from one end of its
    range to the other must not move the array at all.
    """
    reading = m32.get_kochlesson()
    low, high = reading["minimum"], reading["maximum"]
    if low == high:
        report.skip("%s: characters independent of lesson" % label,
                    "minimum == maximum, nothing to vary")
        return

    m32.set_kochlesson(low)
    at_low = m32.get_kochlesson()["characters"]
    m32.set_kochlesson(high)
    at_high = m32.get_kochlesson()["characters"]

    report.equal(at_low, at_high, "%s: characters independent of lesson" % label)


def check_lesson_setter(report, m32, label):
    """Tier 3: PUT kochlesson round trip and range rejection."""
    reading = m32.get_kochlesson()
    low, high = reading["minimum"], reading["maximum"]
    middle = (low + high) // 2

    for wanted in sorted({low, middle, high}):
        m32.set_kochlesson(wanted)
        report.equal(m32.get_kochlesson()["value"], wanted,
                     "%s: PUT kochlesson/%d round trips" % (label, wanted))

    for out_of_range in (low - 1, high + 1):
        if out_of_range < 0:
            continue
        reply = m32.set_kochlesson(out_of_range, allow_error=True)
        report.check("error" in reply,
                     "%s: PUT kochlesson/%d is refused" % (label, out_of_range),
                     "device accepted an out-of-range lesson")

    # leave something in range behind
    m32.set_kochlesson(middle)


def check_persistence(report, m32):
    """Tier 3: the lesson survives a reboot.

    Opening the serial port resets an ESP32-S3, so closing and reopening is a
    genuine power-cycle -- a value held only in RAM cannot survive it.
    """
    reading = m32.get_kochlesson()
    low, high = reading["minimum"], reading["maximum"]
    wanted = high if reading["value"] != high else low

    m32.set_kochlesson(wanted)
    print("  ...  rebooting the device to check the lesson persisted")
    m32.reboot()
    report.equal(m32.get_kochlesson()["value"], wanted,
                 "lesson %d survives a reboot" % wanted)


def check_replies_were_sent(report, m32):
    """Every command must put its own reply on the wire, unprompted.

    A client sends one command and waits for one reply. If the device composes
    an answer but does not transmit it until some later command arrives, that
    client waits forever -- and a client that times out and moves on then reads
    the stale answer as the reply to its NEXT command, so everything after is
    off by one. That is worse than no reply at all.

    m32_link nudges a silent command with a harmless read and counts what comes
    back: two objects means the answer was withheld and the nudge released it,
    one means no answer was ever produced.
    """
    withheld = [cmd for cmd, kind in m32.delayed_replies if kind == "withheld"]
    lost = [cmd for cmd, kind in m32.delayed_replies if kind == "lost"]

    detail = ""
    if withheld:
        detail += "withheld until a later command arrived:\n" + \
                  "\n".join("  %s" % c for c in sorted(set(withheld)))
    if lost:
        detail += ("\nno reply at all:\n" +
                   "\n".join("  %s" % c for c in sorted(set(lost))))
    report.check(not m32.delayed_replies,
                 "every command answered without being prompted", detail.strip())


# ------------------------------------------------------------------ sequences

def walk_builtin_sequences(report, m32):
    for seq in (fw.SEQ_M32, fw.SEQ_LCWO, fw.SEQ_CWAC):
        name = fw.SEQUENCE_NAMES[seq]
        print("\n-- %s" % name)
        m32.set_config("Koch Sequence", seq)
        check_reading(report, m32.get_kochlesson(), name,
                      fw.expected_characters(seq), fw.expected_bounds(seq))
        check_lesson_independence(report, m32, name)
        check_lesson_setter(report, m32, name)


def walk_licw_carousel(report, m32):
    m32.set_config("Koch Sequence", fw.SEQ_LICW)
    for start in fw.LICW_ENTRY_POINTS:
        band = "BC1" if start < fw.LICW_FIRST_BC2 else "BC2"
        name = "LICW %s start=%d" % (band, start)
        print("\n-- %s" % name)
        m32.set_config("LICW Carousel", start)
        check_reading(report, m32.get_kochlesson(), name,
                      fw.expected_characters(fw.SEQ_LICW, start),
                      fw.expected_bounds(fw.SEQ_LICW, start))
        check_lesson_independence(report, m32, name)


def walk_custom_chars(report, m32):
    """Opt-in: this one writes the user's custom character set."""
    probe = "mkrsuapt"
    name = "Custom Chars"
    print("\n-- %s (%r)" % (name, probe))
    m32.command("PUT customchars/set/%s" % probe)
    check_reading(report, m32.get_kochlesson(), name,
                  fw.expected_characters(fw.SEQ_CUSTOM, custom_chars=probe),
                  fw.expected_bounds(fw.SEQ_CUSTOM, custom_length=len(probe)))
    check_lesson_independence(report, m32, name)


# ----------------------------------------------------------------------- main

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port (default: auto-detect)")
    parser.add_argument("--skip-persistence", action="store_true",
                        help="skip the reboot check (saves ~15 s)")
    parser.add_argument("--include-custom-chars", action="store_true",
                        help="also test Custom Chars; this OVERWRITES the "
                             "device's custom character set and restores it "
                             "afterwards")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="echo every command and reply")
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        print("No Morserino found. Plug one in, or pass --port.", file=sys.stderr)
        return 2

    report = Report()
    print("Port: %s" % port)

    m32 = M32Link(port, verbose=args.verbose)
    try:
        m32.open()
    except Exception as exc:
        print("Could not open a protocol session: %s" % exc, file=sys.stderr)
        print("Is a serial monitor or the Configuration Tool still holding the "
              "port, or is the device switched off?", file=sys.stderr)
        return 2
    device = m32.device

    print("Device: %s, firmware %s, protocol %s"
          % (device.get("hardware"), device.get("firmware"), device.get("protocol")))

    # Everything below changes settings; remember them all before touching any.
    saved = {}
    try:
        saved["Koch Sequence"] = m32.get_config("Koch Sequence")
        saved["LICW Carousel"] = m32.get_config("LICW Carousel")
        saved["lesson"] = m32.get_kochlesson()["value"]
        if args.include_custom_chars:
            saved["customchars"] = m32.command("GET customchars")["customchars"]
        print("Saved current settings: %s" % saved)

        walk_builtin_sequences(report, m32)
        walk_licw_carousel(report, m32)
        if args.include_custom_chars:
            walk_custom_chars(report, m32)
        else:
            report.skip("Custom Chars", "not requested; pass --include-custom-chars")

        print()
        if args.skip_persistence:
            report.skip("lesson survives a reboot", "--skip-persistence")
        else:
            check_persistence(report, m32)

        print()
        check_replies_were_sent(report, m32)

    except Exception:
        print("\nAborted by an unexpected error:", file=sys.stderr)
        traceback.print_exc()
        report.failures.append(("harness error", "see traceback above"))
    finally:
        print("\n-- restoring your settings")

        # Each step stands alone: one failure here must not cost you the rest of
        # the restore, which is how a half-restored device gets handed back.
        def restore(label, action):
            try:
                action()
                print("  restored %s" % label)
            except Exception as exc:
                print("  COULD NOT RESTORE %s (%s) -- check Preferences on the "
                      "device" % (label, exc), file=sys.stderr)

        if "customchars" in saved:
            previous = saved["customchars"]
            if previous.get("active") and previous.get("characters"):
                restore("custom chars", lambda: m32.command(
                    "PUT customchars/set/%s" % previous["characters"],
                    timeout=m32.WRITE_TIMEOUT))
            else:
                restore("custom chars (cleared)", lambda: m32.command(
                    "PUT customchars/clear", timeout=m32.WRITE_TIMEOUT))
        # sequence first, then its entry point, then the lesson -- setting a
        # sequence re-derives the bounds and would clamp the lesson otherwise
        restore("Koch Sequence = %s" % saved.get("Koch Sequence"),
                lambda: m32.set_config("Koch Sequence", saved["Koch Sequence"]))
        restore("LICW Carousel = %s" % saved.get("LICW Carousel"),
                lambda: m32.set_config("LICW Carousel", saved["LICW Carousel"]))
        restore("Koch lesson = %s" % saved.get("lesson"),
                lambda: m32.set_kochlesson(saved["lesson"], allow_error=True))
        m32.close()

    return report.summary()


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Soak: PUT kochlesson/<n> + GET kochlesson, many rounds, counting replies that had to be nudged loose.

The measuring loop behind TODO D8 (2026-10-05): on the M32 Pocket's native USB (HWCDC) about one
longer reply in 900 stopped after 6-7 + 64*k bytes and was finished only by the next command. The
root cause turned out to be the missing zero-length packet after a full 64-byte packet (see
usb64_probe.py); this loop is the long-reply half of the proof that it is gone.

Each round sets the next lesson in the sequence's range and reads it back. A reply that does not
arrive within 3 s is nudged (M32Link.command) and counted as withheld (the nudge released it) or
lost (it never existed). The lesson is restored afterwards. Hands off the device while it runs
(~0.3 s per round).

    ./kochlesson_loop.py               # 1000 rounds
    ./kochlesson_loop.py --rounds 3000
"""

import argparse
import sys

from m32_link import M32Link, find_port

TIMEOUT = 3.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--rounds", type=int, default=1000)
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("no Morserino found on USB")
        return 2
    with M32Link(port) as m32:
        print("device: %s" % m32.device)
        start = m32.get_kochlesson()
        low, high, before = start["minimum"], start["maximum"], start["value"]
        span = high - low + 1
        try:
            for r in range(args.rounds):
                n = low + r % span
                done = len(m32.delayed_replies)
                m32.set_kochlesson(n)
                m32.command("GET kochlesson", timeout=TIMEOUT)   # nudges a withheld reply and records it
                for cmd, kind in m32.delayed_replies[done:]:
                    print("  round %4d  lesson %2d  %s: %s" % (r, n, kind.upper(), cmd))
                if (r + 1) % 100 == 0:
                    print("  %4d rounds, %d stuck so far" % (r + 1, len(m32.delayed_replies)))
        finally:
            m32.set_kochlesson(before)
    stuck = m32.delayed_replies
    print()
    print("  %d rounds, %d commands, %d stuck replies (%d withheld, %d lost)"
          % (args.rounds, 2 * args.rounds, len(stuck),
             sum(1 for _, k in stuck if k == "withheld"), sum(1 for _, k in stuck if k == "lost")))
    return 1 if stuck else 0


if __name__ == "__main__":
    sys.exit(main())

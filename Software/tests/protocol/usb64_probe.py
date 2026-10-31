#!/usr/bin/env python3
"""Probe: does the M32 Pocket withhold a reply whose length is a multiple of 64 bytes?

Found on the M1 bench (2026-10-30): the detailed GET control/speed reply,
{"control":{"name":"speed","value":19,"minimum":5,"maximum":60}}, is exactly 64 bytes and came
back only after the next command, in every run. Hypothesis: on the Pocket's USB-Serial/JTAG
(HWCDC) a transfer that ends on a full 64-byte packet is not closed by a zero-length packet, so
the host's read stays open until the next traffic.

The probe alternates speeds whose detailed reply is 63 bytes (5..9 WpM) and 64 bytes (10..60),
asks GET control/speed each time WITHOUT nudging, and counts the replies that did not arrive
within 2 s. The hypothesis predicts: every 64-byte reply withheld, no 63-byte one.
It restores the speed afterwards. Hands off the device while it runs (~1 minute).
"""

import json
import sys
import time

from m32_link import M32Link, find_port

ROUNDS = 10
SPEEDS = (9, 19, 8, 25)          # 63, 64, 63, 64 bytes


def get_speed_raw(m32):
    """GET control/speed without a nudge; return (reply length or None, bytes seen if withheld)."""
    m32._buffer = ""
    m32._serial.reset_input_buffer()
    m32._write_line("GET control/speed")
    try:
        raw = m32._read_object(2.0, keep_partial=True)
        return len(raw), None
    except TimeoutError:
        seen = len(m32._buffer)
        m32._write_line("GET capabilities")          # shake it loose, then drain both
        for _ in range(2):
            try:
                m32._read_object(2.0)
            except TimeoutError:
                break
        return None, seen


def main():
    port = find_port()
    if not port:
        print("no Morserino found on USB")
        return 2
    tally = {}                                        # reply length -> [arrived, withheld]
    with M32Link(port) as m32:
        print("device: %s" % m32.device)
        m32.command("PUT menu/stop")
        time.sleep(0.5)
        before = m32.command("GET control/speed")["control"]["value"]
        try:
            for r in range(ROUNDS):
                for wpm in SPEEDS:
                    m32.command("PUT control/speed/%d" % wpm)
                    expect = len(json.dumps({"control": {"name": "speed", "value": wpm,
                                                         "minimum": 5, "maximum": 60}},
                                            separators=(",", ":")))
                    got, seen = get_speed_raw(m32)
                    t = tally.setdefault(expect, [0, 0])
                    if got is None:
                        t[1] += 1
                        print("  %2d WpM  %d-byte reply WITHHELD (%d bytes had arrived)" % (wpm, expect, seen))
                    else:
                        t[0] += 1
                        print("  %2d WpM  %d-byte reply arrived" % (wpm, got))
        finally:
            m32.command("PUT control/speed/%d" % before)
    print()
    for n in sorted(tally):
        print("  %d-byte replies: %d arrived, %d withheld" % (n, tally[n][0], tally[n][1]))
    confirmed = tally.get(64, [0, 0])[1] > 0 and tally.get(63, [0, 0])[1] == 0
    print("\n  hypothesis %s" % ("CONFIRMED: full 64-byte packets wait for the next traffic"
                                  if confirmed else "not confirmed"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

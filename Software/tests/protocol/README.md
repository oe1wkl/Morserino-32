# M32 serial protocol — conformance tests

Bench tests that drive the **real firmware** over USB serial and check that
what it reports about the Koch character sequences actually hangs together,
plus an offline selftest that keeps the checks honest without hardware.

Written after three related bugs turned up in one week, each found by a person
noticing something odd rather than by anything automatic:

| Fix | What was wrong |
|---|---|
| `0b9e3e2` | under Custom Chars the word/abbrev tables ignored the Koch lesson |
| `292e417` | `PUT kochlesson` neither rebuilt those tables nor persisted the value |
| `9e3ffb6` | `GET kochlesson` truncated `characters` for LICW Carousel BC2 |

The third is the one that motivated this directory: it shipped in V7 and
survived every release since, because nothing ever compared the reply's shape
against what it claimed about itself.

```sh
python3 selftest.py       # no device needed — CI-friendly
./run_tests.py            # needs a Morserino on USB
```

## What runs where

### `selftest.py` — no hardware

Replays `GET kochlesson` replies **captured from real devices** and asserts the
verdict the harness reaches on each: the good ones must pass, and the reading
captured before `9e3ffb6` must fail — specifically on the
`len(characters) == maximum` invariant, the one that describes the bug.

This is what makes the rest trustworthy. A checking harness that is never
itself checked quietly rots into a pile of assertions that all happen to hold.
**When a new protocol bug is found, paste the bad reply in here as a fixture**;
the captured reply then *is* the regression test.

### `run_tests.py` — needs a device

Walks every Koch sequence — M32, LCWO, CW Academy, and LICW Carousel at all 14
entry points — and checks each reading in three tiers:

**Tier 1 — invariants.** Relations that must hold whatever the sequences
contain, so they can never go stale:

- `len(characters) == maximum` — the array covers the whole sequence
- `1 <= minimum <= value <= maximum`
- no empty entries
- **`characters` does not move when the lesson does.** The array describes the
  sequence; `value` describes the learner's position in it. Moving the lesson
  from one end of its range to the other must change nothing.

**Tier 2 — ground truth.** The exact expected array, derived by
`firmware_tables.py` from the firmware's own source rather than hand-copied.

**Tier 3 — the lesson setter.** `PUT kochlesson/<n>` round trips; out-of-range
values are refused; and the value **survives a reboot** — the harness closes
and reopens the port, which resets an ESP32-S3, so anything held only in RAM
cannot survive. That last check is what `292e417` fixed.

**Reply delivery.** Every command must put its own answer on the wire without
being prompted. See the finding below.

## What the first run found: withheld replies (now fixed)

The first full run turned this up, and it is what the reply-delivery check now
guards against:

> **Some commands composed a reply and did not transmit it until the next
> command arrived.** Seen on `PUT kochlesson/*` and `PUT config/*`, and
> intermittent — the same command succeeded on other runs.

**Cause:** on the M32 Pocket `Serial` is HWCDC, the ESP32-S3's hardware
USB-Serial-JTAG. Its `write()` hands bytes to a ring buffer that an ISR drains,
but the draining interrupt is only re-armed under `connected`; when it is not,
the bytes sit there until the next thing on the wire wakes the ISR. Arduino's
own `flush()` exists for this and says so in a comment: *"Now trigger the ISR to
read data from the ring buffer."* The firmware never called it.

**Fix:** `M32Tee::flushReply()`, called once per handled command line in
`serialEvent()`. Deliberately not called after every protocol object — the
asynchronous event stream and `echo()` are fire-and-forget, and flushing per
keyed character would put HWCDC's 100 ms `tx_timeout_ms` into the keying path
if a host stopped reading.

**Verified with this harness:** before, every run reported ten or more withheld
replies; after, three consecutive runs report none.

This is worse than a missing reply. A client that waits, times out, and moves
on will read the stale answer as the reply to its *next* command, and every
reply after that is off by one — which is exactly how the first version of this
harness got a `INVALID KOCH LESSON 52` error while asking about something else.
The Configuration Tool hides it: `setKochLesson()` wraps its wait in
`try{...}catch(e){}` and ignores the timeout.

The harness proves the distinction rather than inferring it from timing. When a
command goes unanswered it sends one harmless read and **counts the objects
that come back**: two means the original answer was sitting unsent and the
nudge released it (*withheld*), one means no answer was ever produced (*lost*).
Every case so far has been *withheld*. Timing cannot separate the two — both
arrive at the normal ~100 ms round trip — which is why an earlier version of
this check, based on latency, was not sound.

Root cause **not established**. A short-reply flush theory (the ESP32-S3
USB-Serial-JTAG FIFO holding back fewer than 64 bytes) was tested and does not
hold: isolated 23-byte `{"ok":...}` replies arrive fine. It appears to need
load — a rapid sequence of writing commands — to show up.

## Where the expectations come from

`firmware_tables.py` parses the firmware source, so editing a Koch sequence
does not silently disarm the tests:

| Read from source | Where |
|---|---|
| the four character sequences | `MorsePreferences.h` |
| `adaptiveProbabilities[]` capacity (every sequence's ceiling) | `MorsePreferences.h` |
| the prosign display table (`K` → `<sk>` …) | `cleanUpProSigns()` in `m32_v6.ino` |

**One piece of firmware logic is mirrored rather than read:** `licw_sequence()`,
the Python twin of `Koch::setupLICWkochChars()`. It is a dozen lines of array
arithmetic, but it is a copy — if the carousel rotation ever changes, change it
too. Tier 1 does not depend on the mirror, so even a stale mirror leaves the
invariant that caught the real bug intact.

Run it directly to see what it believes:

```sh
python3 firmware_tables.py
```

## Options

```
--port /dev/cu.usbmodem1101   explicit port (default: auto-detect)
--skip-persistence            skip the reboot check (saves ~15 s)
--include-custom-chars        also test Custom Chars — see the warning below
-v                            echo every command and reply
```

## Before you run it

- **Close any serial monitor and disconnect the Configuration Tool.** The port
  takes one user at a time; a WebSerial connection in Chrome holds it just as
  firmly as `pio device monitor`.
- The tests **change preferences** — Koch Sequence, LICW Carousel, Koch lesson
  — and restore them from a snapshot taken at the start, in a `finally`. If it
  is killed mid-run, check Preferences on the device.
- `--include-custom-chars` **overwrites the device's custom character set**. It
  saves and restores it, but it is opt-in for that reason.
- The persistence check reboots the device. Do not run it while something else
  is talking to the Morserino.

## Scope

Covered: everything `GET kochlesson` and `PUT kochlesson` do, across every
built-in sequence and carousel entry point, plus Custom Chars on request.

Not covered: the rest of the protocol. The structure is meant to be extended —
`m32_link.py` is a general-purpose client (handshake, brace-framed replies,
reboot) with nothing Koch-specific in it, so a new command needs only new
checks. `GET configs/details` paging, `GET capabilities`, snapshots and the
file commands are all worth the same treatment.

Also not covered: anything requiring the paddle, the display, or audio. The
CW keyer has its own host-side simulation in `devdocs/ultimatic/keyer_sim.cpp`,
and the QSO Bot matcher has unit tests in `../qso_bot_matcher/`.

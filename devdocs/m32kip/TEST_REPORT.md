# M32KIP Phase 5 — verification against spec §13

Status: **plan; instruments being built.** Firmware under test: branch `m32kip` from f8dd512 (Phase 4 + D16).

## What stands in for the missing equipment

Spec §13 assumes a logic analyser on both key lines and a Linux box running `tc netem`. Neither is available
(D11). What replaces them, and what that costs:

| Spec instrument | Stand-in | What it can and cannot show |
|---|---|---|
| Logic analyser on the Rig's TX-out | **Rig self-measurement** (`-D KIP_MEASURE=1`, command line only, like the Phase 0 spike): for every edge, the time the hardware-timer ISR actually put it on the key line, compared with the Keyer's timestamp for the same edge | Reproduced mark and space lengths against the *Keyer's* timeline — exactly D12b's acceptance. It cannot see the transmitter's own keying transistor (Phase 0 put the ISR at 45–96 µs), nor anything before the Keyer's capture point. |
| Logic analyser on the Keyer's key line | The Keyer's timestamps themselves, carried in every KEY packet | The Keyer's capture granularity is the loop (Phase 0: 0.24 ms classic, 1.7 ms Pocket floor). A capture error is invisible to this method. |
| `tc netem` box between the units | **Impairment relay on the Mac** (`Software/tests/kip/impair_relay.py`, stdlib only so Apple's `/usr/bin/python3` can reach the LAN): the Keyer's TRX Peer points at the Mac, the relay forwards both directions with delay, jitter, loss, reordering and burst loss | Impairment is added on top of the real WiFi path, so profile (a)'s 50 ms is "at least 50 ms". Timer resolution on macOS is ~1 ms, fine against 10–40 ms jitter. |
| STATS logging | The relay derives the session key from HELLO / HELLO_ACK (it holds the PSK) and logs every STATS as CSV | Everything the Rig reports; the drift log for §13.3. |

## The tests, and who runs them

| §13 | Test | How | Needs Willi |
|---|---|---|---|
| 1 | Bench loopback, 15 / 25 / 40 WPM, 10 min each | Pocket Remote Keyer → classic Remote Rig. Paced text over `PUT cw/play` keys the Pocket's own keyer (source 0); `KIP_MEASURE` reports mark/space error. Acceptance **1 ms** (D2), not 0.5 ms | No |
| 2 | Impairment profiles (a)–(e) | Same, through the relay | No |
| 3 | Drift, 2 h | Relay logs STATS; `KIP_MEASURE` counts offset steps and checks each lies in an idle gap | No (devices left running) |
| 4 | Safety: forged MAC, max key-down | Scripted from the Mac with `reference_peer.py` | No |
| 4 | Safety: WiFi pulled mid-mark | Hold a dah, switch the Keyer's access point off (or walk out of range) | **Yes** |
| 5 | Real Internet | Keyer on a mobile hotspot, Rig behind the home router with a UDP port forward, relay-free; STATS logged by the Rig over USB | **Yes** (hotspot, port forward) |
| 6 | Straight key, bug | Real keys; the glitch filter's chatter case (c) has no signal generator — covered by the host tests (653 → 669 checks, bouncing-key simulation) | **Yes** |
| 7 | Tune: 12 s key-down; iambic → straight key mid-session | Keyed by hand | **Yes** |
| D16 | Break-in compensation | `KIP_MEASURE` shows first marks lengthened by exactly Rig 1st Ext, others untouched | No; a listening check on a real transceiver is Willi's |

## Results

### Instrument validation, 2026-09-16 (clean link, 15 WPM, 2 min)

The first measured figures of the project, and the reason to trust the ones that follow:

| | count | within 0.2 ms | worst |
|---|---|---|---|
| Marks | 224 | 224 | −83 … +97 µs |
| Spaces (inside a character / between characters) | 208 | 208 | −91 … +79 µs |
| Idle gaps | 15 | 15 | −78 … +14 µs |

`short 0` (no mark ever shortened — design principle 5), `late 0`, no underruns, no protocol errors, no queue
overflows; playout delay settled at 145 ms, one offset step, one delay decrease. Against D2's relaxed **1 ms**
target the Rig reproduces the Keyer's timeline to about **0.1 ms**. (`dup ~5000` is D13 redundancy working as
designed; `dit 74` ms at 15 WPM is the generator's known 6 ms-short element, [[cw-timing-audit-2026-07]].)

### Two instrument faults found on the way — both mine, not the firmware's

1. **A stale interrupt timestamp.** When `poll()` retired an edge through its fallback write, that edge's alarm
   could still fire afterwards and leave its timestamp behind for the *next* edge to consume. One interval then
   read ~0 and the next a whole element too long. The aggregate looked alarming — a third of all marks "wrong" by
   exactly one dah (234 ms) or one word gap (569 ms), 136 "shortened" marks — while the Rig's own counters said
   `late 0, und 0` and the delay never moved. Errors landing on whole element lengths are the signature of a
   mispairing, not of a key line that moved. Fixed by dropping any pending fire when a new alarm is armed; the
   per-interval trace (`KIPT`, with I/P/N saying where each timestamp came from) is what proved it.
2. **Link detection that asked the wrong end.** The driver waited for the Keyer to announce a fresh handshake, so
   a Keyer that had reconnected by itself (D14) looked like a failed one — and it once keyed for two minutes into
   a Pocket that had fallen back to its menu. It now reads the Rig's own 10 s report, which is emitted only while
   a session exists. A network probe from inside the driver was tried first and always returned "no Rig": under
   this shell `/usr/bin/python3` is an `xcrun` shim that fails to load, and the driver's own interpreter is blocked
   from the local network by macOS ([[macos-local-network-python-trap]]).

### Link detection: three wrong answers before a right one

Worth recording, because each mistake cost a run and two of them produced *plausible* output:

1. **Wait for the Keyer to announce a handshake.** A Keyer that reconnects by itself (D14) never re-announces, so a
   healthy link read as a failure — and the driver once keyed for two minutes into a Pocket that had dropped back
   to its menu, which looks exactly like a successful run until you notice every count is zero.
2. **Ask the Rig, over the network.** Correct in principle (spec §8: `NACK(busy)` proves a session), but unusable
   from inside the driver: `/usr/bin/python3` is an `xcrun` shim that fails to load under this interpreter, so the
   probe always answered "no Rig" while the pair was demonstrably linked.
3. **Read the Rig's own 10 s report.** Silent when the Rig's *protocol session* is off — and the classic did exactly
   that: it served the Pocket's session for ten minutes while emitting nothing over USB, so the driver tore the link
   down three times and then measured nothing at all.
4. **What works:** demand positive evidence from each end in turn. The classic must report `Remote Rig` starting, and
   the Keyer — always restarted *after* it, so its announcement is fresh — must report the Rig's address. Anything
   else aborts the run rather than measuring silence.

**Bench hazard worth remembering:** killing the driver mid-run left the Pocket with a mode it thought was still
running — `GET menu` kept reporting `active: true`, `menu/stop` answered OK without effect, and *no* mode would
start afterwards, not even the plain CW Keyer. Only a reboot cleared it (esptool `--after hard_reset`, no
reflash). Stop a run with `PUT cw/stop` and `PUT menu/stop`, not by killing the driver.

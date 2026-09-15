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

*(to follow)*

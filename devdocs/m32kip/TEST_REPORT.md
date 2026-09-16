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

### §13.1 Bench loopback — clean link, 10 minutes per speed

Pocket Remote Keyer (source 0, keyed by `PUT cw/repeat`) → classic Remote Rig, same LAN, no impairment.
Acceptance is D2's **1 ms**, measured against the Keyer's own timestamps (D12b).

| WPM | marks | within 0.2 ms | worst mark | spaces | within 0.2 ms | worst space | shortened | late | underruns |
|---|---|---|---|---|---|---|---|---|---|
| 15 | 1959 | **1959** | −100 … +103 µs | 1828 | **1828** | −108 … +88 µs | **0** | 0 | 0 |
| 25 | 3249 | 3219 (99.1 %) | −90 … **+3587 µs** | 3033 | 3006 | **−3600** … +84 µs | **0** | 0 | 0 |

**25 WPM misses the 1 ms target on about 0.65 % of marks — the project's first acceptance shortfall.** Of 3249
marks, 21 exceeded 1 ms and 15 exceeded 2 ms; the worst was +3.59 ms, and the space that followed it was −3.60 ms.
That pairing is the signature of **one edge emitted late**: the mark is stretched and the following gap shortened by
the same amount, so nothing is lost, only displaced. Three things say the network is not responsible: `late 0` (no
edge ever arrived after its playout time), `und 0`, and a playout delay that sat at 135–150 ms throughout. No mark
was ever *shortened* (`short 0`), so design principle 5 holds even here.

The suspect is inside the Rig: the per-edge trace shows most edges are retired by the run loop's fallback write
rather than by the hardware-timer alarm, which makes emission depend on loop latency — and the Rig redraws its
display every 250 ms, at ~37 ms per redraw on the OLED. The Keyer already obeys a "never redraw near an edge" rule
(Phase 0); the Rig does not.

**The `KIPX` trace then attributed them, and every one points the same way.** Four outlier pairs in a one-minute
25 WPM run, each a lengthened mark followed by an equally shortened space:

| outlier | mark | following space |
|---|---|---|
| 1 | **+6919 µs (N)** | −6982 µs (N) |
| 2 | **+1623 µs (N)** | −1676 µs (I) |
| 3 | **+646 µs (N)** | −700 µs (N) |
| 4 | **+1555 µs (N)** | −1604 µs (N) |

`N` means the edge reached the key line through the run loop's fallback `digitalWrite`, not through the alarm ISR:
the alarm had not fired, and the loop only came round to it after a stall. **Every lengthened mark is an `N`.**

**But the instrument is a suspect in its own right, and must be cleared before the display is blamed.** The report
writes up to three protocol messages every 10 s — ~200 bytes each, ~17 ms of serial at 115200 — *from the same loop
that retires edges*, and this run had four outliers against six reports. The shipping firmware has the 250 ms redraw
but none of that serial traffic. The report interval is therefore a compile-time knob (`KIP_REPORT_S`): running at
60 s and comparing outlier rates rules the instrument in or out before any fix is designed.

**The instrument was cleared, and the real cause is the Rig's own STATS send.** Cutting the reports from every 10 s
to every 60 s — six times less serial traffic out of that loop — left the severe outliers untouched:

| reporting | keying | marks | > 2 ms | rate |
|---|---|---|---|---|
| every 10 s | 1 min | 326 | 1 | ~1 / min |
| every 60 s | 3 min | 831 | 6 | ~2 / min |

The magnitude is the tell: the stalls are **2–4 ms**, not the ~37 ms an OLED redraw costs. Phase 0 measured that
exact figure for something the Rig does in this loop every second — `writeTo()` blocks 2–8 ms — and this is the loop
that arms the alarm for the next edge. A STATS packet sent just before a key-up delays the arming, and the edge goes
out late by the length of the stall; the following space is short by the same amount, and nothing is lost. Roughly 60
sends a minute, a couple of which land next to an edge, matches the observed ~2/min.

**Fix:** the Rig now defers its STATS send while the next edge is due within 20 ms — the same rule Phase 0 imposed on
the Keyer ("never send from the keying loop"), which the Rig had never been held to. STATS is a 1 Hz report, so
deferring it costs nothing. The 250 ms display redraw is the same hazard in principle and remains a candidate, but it
would show as ~37 ms slips, which the traces do not contain; it is left alone until this fix is measured.

**Measured: deferring STATS changed nothing.** Three minutes at 25 WPM, before and after, same instrument settings:

| | marks | > 2 ms | worst |
|---|---|---|---|
| before | 831 | 6 | +4.38 ms |
| after | 833 | 6 | +4.62 ms |

So the packet send is not the cause either. Two suspects are now eliminated *by measurement* — the instrument's own
serial traffic, and the Rig's STATS send — which is worth as much as a confirmation. The deferral is kept: it is
correct by Phase 0's rule whatever else is going on.

**What the signature actually implicates: a stale alarm.** `N` means the edge fell due with *no alarm pending for
it*. The Rig armed the timer only when nothing was armed, so once the queue head changed — a redundant copy arriving
out of order, or the playout delay and clock offset shifting the whole timeline (§7.2, §7.3) — the alarm still
pointed at the old moment. The new head then fell due unnoticed and the run loop retired it on its next pass. That
accounts for all four observations at once: the `N`, the 2–4.6 ms magnitude, no lateness count (the edge itself
arrived on time), and no shortened mark. **Fix under test:** re-arm whenever the head's due time changes, not merely
when nothing is armed.

**Measured: re-arming changed nothing either.** Three minutes at 25 WPM each time, same instrument settings:

| | marks | > 1 ms | > 2 ms | worst |
|---|---|---|---|---|
| baseline | 831 | 7 | 6 | +4.38 ms |
| STATS deferred | 833 | 8 | 6 | +4.62 ms |
| + re-arm on head change | 833 | 8 | 4 | +3.94 ms |

Three hypotheses eliminated by measurement now: the instrument's serial traffic, the STATS send, and the stale
alarm. Both fixes are kept — each is correct on its own terms — but neither is *the* cause. The signature has not
budged: every outlier retired by the loop's fallback write, mark long and the following space equally short, no
lateness, no shortened mark.

**Last untested in-loop stall: the display.** 37 ms per redraw on the OLED, every 250 ms, in the loop that arms the
emitter. The magnitude argument against it (37 ms stalls should show as 37 ms slips) is not airtight, and it is
exactly the rule Phase 0 imposed on the Keyer and never on the Rig. Now gated the same way — never while the key is
down, never within 50 ms of the next edge — and under test.

No protocol errors, no queue overflows, no key-down limits. The playout delay adapted from 150 ms down to 100 ms
over the run (10 decreases), with 9 offset steps.

**The idle-gap outliers are the design, not a defect.** 122 of 130 idle gaps were within 0.2 ms; the other 8 ran up
to 6.0 ms short. An idle gap is the *only* place the Rig may take up clock drift (offset step, ≤ 1 ms, spec §7.2)
or shrink the playout delay (5 ms per step, §7.3) — so a gap carrying one of each comes out ~6 ms short, which is
what 10 decreases plus 9 offset steps predict. Marks and in-character spaces were never affected, which is exactly
the graded acceptance the host tests assert. A listener hears the rhythm inside words unchanged and a word gap
occasionally a hair shorter.

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

### Open question for Willi: a Remote Keyer restarted often enough stops linking

Seen twice on 2026-09-16, and it costs a run each time:

- After several stop/start cycles of Remote Keyer within one boot, the Pocket reports **"No answer"** — while the
  classic's Rig, at that same moment, answers a HELLO from this Mac **instantly** and reports no session. So the Rig
  is listening and reachable; the Keyer's packets are not getting there (or its answers are not getting back).
- Once in that state the Pocket will not start **any** mode: `menu/stop` answers OK, `menu/start` is swallowed, and
  `GET menu` keeps reporting `active: true`. Plain CW Keyer fails the same way, so it is not M32KIP-specific.
- Only a reboot clears it (`esptool --after hard_reset`, no reflash). Afterwards the very next start links first try.

A user who leaves and re-enters Remote Keyer repeatedly could hit this, so it deserves a look before release. The
suspect is the socket/WiFi teardown in `MorseKipKeyer::end()` → `begin()` (`audp.close()` then `audp.listen()`), but
that is a hypothesis, not a diagnosis — the failing device could not be questioned over the network, only rebooted.
The bench driver now reboots the Pocket before every run so a measurement is never spent on a Keyer that cannot
transmit; that is a workaround for the bench, not a fix.

**It got worse through 2026-09-16, and the reboot workaround no longer holds it off.** Later the same morning the
state returned within a couple of stop/start cycles of a reboot the driver had just performed, and it cost three
more runs. Two details are worth more than the rest:

- **`PUT menu/start` returns *silence*, not an error** — no `ok`, no `menu`, no `activate`, nothing for 30 s —
  while `menu/stop`, `wifi/trxpeer`, `wifi/select` and `GET wifi` all answer instantly on the same port in the same
  second. So the protocol and the serial path are alive; only mode entry is dead.
- **The clearest evidence yet of what fails first:** in the 11:07 run the *Rig* reported a live session (its 10 s
  reports print only inside the session branch) while the *Keyer* reported `No answer` and left the mode. The
  Pocket's HELLO therefore arrived at the classic and the classic's `HELLO_ACK` never got back. A Keyer that can
  transmit but not receive explains every symptom of the day, including why this Mac could always reach the Rig.

**The receive-path theory was then tested and refuted.** With the Pocket freshly rebooted and pointed at a reference
Rig on this Mac (`reference_peer.py rig`), it linked **immediately** — `session … opened by 192.168.1.23, source 0` —
and went on sending keepalives (120+ KEY packets). A handshake only completes if the `HELLO_ACK` comes back, so the
Pocket's UDP receive path is healthy. Two better explanations remain:

1. **This bench's own probe can block the pair.** `probe_session.py` takes a session on the Rig and releases it with
   a BYE; if that BYE is lost, the Rig keeps a session with *the Mac*, and spec §8 allows only one — so the Pocket's
   HELLO is refused with `NACK(busy)`, which the Keyer reports as "No answer". That is exactly the run where the Rig
   showed a live session while the Keyer said No answer. **Use the probe sparingly, and never while a run is
   starting.** A probe that cannot be released cleanly is not a read-only instrument.
2. **Commands swallowed on the Pocket.** In the 11:02 run the Keyer linked, but `PUT cw/repeat` drew *no reply at
   all* and no keying followed — the same silence as `menu/start` in the stuck state. The Pocket answers queries
   (`GET wifi`, `GET menu`) throughout, so this is mode/command handling, not the serial path and not the network.

So the open question is narrower than it looked: **not networking — the Pocket's command and mode handling after
repeated mode entry.** Everything else here still stands, including that only a reboot clears it.

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

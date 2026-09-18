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

### §13.2 Impairment profiles — via the relay

The Keyer's TRX Peer points at this Mac, `impair_relay.py` forwards both directions with the profile's delay, jitter
and loss, and the Rig is measured as before. Three minutes of continuous 25 WPM keying per profile.

| profile | marks | within 0.2 ms | worst mark | shortened | late | underruns | playout while keying |
|---|---|---|---|---|---|---|---|
| **(a) 50 ms, 10 ms jitter** | 963 | **963 (100 %)** | −82 … +90 µs | **0** | 0 | 0 | steady 150 ms |
| **(b) 120 ms, 40 ms jitter** | 809 | **809 (100 %)** | −92 … +100 µs | **0** | 0 | 0 | steady 150 ms |
| **(c) (b) + 2 % loss** | 833 | **833 (100 %)** | −82 … +103 µs | **0** | 0 | 0 | steady 150 ms |

| **(d) (b) + 5 % loss, reordered** | 808 | **808 (100 %)** | −76 … +94 µs | **0** | 0 | 0 | steady 150 ms |
| **(e) bursts of 3 lost, every 5 s** | 844 | 843 (99.9 %) | **−137982** … +74 µs | **1** | 2 | 2 | 150 → 224 ms |

**(e) is the one profile that fails its acceptance: one mark came out 138 ms short.** §13.2 asks for *no* shortened
mark under burst loss, and 843 of 844 marks were inside 0.2 ms — but one was not, with the gap after it 212 ms long.
The Rig then grew its playout delay 150 → 224 ms and nothing further was shortened in the remaining ~30 bursts, so
the behaviour is self-correcting; it is still a breach of design principle 5, which is the promise the whole design
rests on.

*Mechanism — inference, not measurement:* a burst appears to have swallowed every quick copy of one key-down, so it
arrived only on a later repeat, past its playout time, and was emitted at once; the key-up that followed was
unaffected and went out on schedule, shortening the mark by the key-down's lateness. That fits `late 2`, `und 2` and
the delay growth, and it explains why the *following* gap is long. Confirming it needs the arrival time of that one
edge, which the present instrument does not capture — worth adding before this is treated as settled.

*Mitigation already shipped:* on a lossy link, raising **Rig Delay** above the burst recovery time prevents this
outright — exactly what D4's fixed-delay setting exists for. The default (Adaptive, starting at 150 ms) is what was
measured here; a rig on a burst-prone path should be given a floor.

**(c) is the first profile that actually cost packets, and the redundancy swallowed all of it.** The relay dropped
**135 of 7091** KEY packets on the way to the Rig (plus 3 STATS on the way back) and not one edge was lost: every
mark and space still landed within 0.2 ms, with no late edge and no underrun. That is D13's repeat schedule — three
copies 20 ms apart, then the keepalive — earning its ~3× packet cost.

**(d) lost 358 of 7070 packets (5 %) with reordering on, and still reproduced every mark and space within 0.2 ms.**
Its two outlier intervals are both **idle gaps** — −982 µs and −991 µs — and the run took exactly **two clock-offset
steps**. A step is capped at 1 ms and may only be taken in an idle gap (§7.2), so two steps give two gaps about a
millisecond short: a one-to-one correspondence, and the same behaviour the host tests assert. No mark or in-character
space was touched. Out-of-order arrival, one of the discarded suspects for the 25 WPM outliers, causes no trouble at
all when it is actually applied.

Relay: 6894 KEY packets forwarded for (a) and 7030 for (b), none dropped — neither profile adds loss, and none
occurred. For (b) the delay held at 150 ms across *all three* reports, checked in the series rather than sampled at
the end.

**Reading the playout delay correctly matters here.** The relay's STATS log shows the delay falling 150 → 122 → 96 →
86 → 52 → 50 ms, which looks like the collapse §13.2 warns about — but every one of those rows is timestamped *after*
the last measurement report, i.e. after keying stopped. Throughout the keyed portion the delay sat at exactly 150 ms.
What the tail shows is the Rig shrinking D during idle, which is what §7.3 asks of it when there is nothing to
protect. Acceptance for (a) — no shortened mark, underruns ≤ 1/min after the first 30 s, D converging without
oscillation — is met.

*Not understood, and therefore not claimed:* the Rig's own `jitter` field reads 32–220 ms under a profile that adds
only ±10 ms. It may be measuring the relay's scheduling granularity on top of real WiFi, or the field may mean
something narrower than assumed here. It bears on none of the acceptance criteria — marks, spaces, lateness and
underruns are all clean — but it should be pinned down before anyone quotes it.

### §13.3 Drift — **not satisfied as the spec intends; here is what was actually done**

Ten minutes of continuous 25 WPM keying on the direct link: **3247 marks, 3032 spaces and 215 idle gaps, every one
within 0.2 ms**, no shortened mark, no late edge, no underrun, playout delay steady at 150 ms — and **zero clock
offset corrections** (`off 0`).

**That last figure is the point, and it is why this test is not finished.** §13.3 asks for a *two-hour* session so
that the clocks drift far enough apart for the corrections to be observed and checked: ≤ 1 ms per step, never inside a
character. In ten minutes the clocks barely diverge, so there is nothing to observe. Running twelve ten-minute legs
would **not** substitute: each leg restarts both ends and re-establishes the offset, so it samples the first ten
minutes of drift twelve times rather than watching two hours of it. The ten-minute ceiling on an unattended command
here is what forces the legs, so this cannot be completed hands-free.

Earlier runs did take corrections — one or two per ten-minute run, and profile (d) showed exactly two steps producing
exactly two idle gaps ~1 ms short, which is the permitted behaviour and matches what the host tests assert. That is
supporting evidence, not the test.

**To finish it:** leave the pair keying for two uninterrupted hours and read the Rig's report at the end. The
instrument already reports `off` steps cumulatively, so nothing further needs building — only the time.

*A second attempt at a leg aborted* when the Keyer fell into the state described below (which turned out to be
the protocol's mode-entry handling, not the Keyer — see the correction in that section); the drift legs were
stopped there rather than repeatedly restarting a Pocket that trips the fault every few mode entries.

### D17 Remote configuration — verified on the wire, 9/9 plus the NVS round trip

`Software/tests/kip/config_tests.py`, run from this Mac against the classic as Rig (Pocket deliberately not linked,
so the test holds the single session itself):

| Property | Result |
|---|---|
| Capability advertised | `HELLO_ACK flags 0x02` — a Keyer can tell a capable Rig from an older one |
| Fetch | `CFG_REQ` answered with all six values, flagged `CFG_STORED` |
| Set | the changed value took, **nothing else moved**, and the reply reported what was *stored* rather than echoing |
| Clamp | asked for **250**, the Rig stored **30** — the parameter's own maximum |
| NVS round trip | after a genuine reboot the Rig still reported the stored value |

The clamp test is the one that matters most for safety: a stale or hostile Keyer must not be able to set a remote
transmitter's key-down limit to anything it likes, and the Rig — not the Keyer — enforces the range.

*Cleanup:* the test left `Rig Limit Kyr` at 30 (the clamp result); it has been set back to its default of 3, and no
other value was touched.

*One oddity, chased down and benign:* the Rig initially reported `Rig Hang Unit = 1` (Dits) and `Rig Hang = 10`,
where the source defaults are 0 (Milliseconds) and 5. Since they survived a reboot they were genuinely in NVS, and
the worry was the positional `prefPos` / `prefName[]` / `pliste[]` triplet — whose `static_assert`s check array
*lengths* but **not alignment**, so three entries inserted at different relative positions in two arrays would
compile cleanly and then read and write each other's keys (CLAUDE.md rule 10).

Checked against the source rather than reasoned away: the two arrays ran in identical order
(`kipPlayout, kipMaxKeyer, kipMaxManual, kipFirstExt, kipHangUnit, kipHang, kipGlitch` against `Rig Delay, Rig Limit
Kyr, Rig Limit SK, Rig 1st Ext, Rig Hang Unit, Rig Hang, Glitch Filter`) and the defaults are 0 and 5 as expected.
Setting both explicitly and rebooting returns them as 0 and 5, persistently. **No misalignment**: the 1 and 10 were
historical state in that device's flash from earlier bench work, not a live fault. The Rig is now on its documented
defaults across all six settings.

*Superseded on 2026-09-17:* `Rig Hang Unit` has since been removed (D16 amendment) — the break-in hang is
milliseconds only — so both lists above are one entry shorter in today's firmware.

**Re-run 2026-09-18 after the hang unit was removed: 11/11, plus the NVS round trip.** The test gained two checks
for the byte that used to carry the unit, now reserved: the rig sends it as **zero**, and a Keyer that puts junk
there (`0xAA`, sent together with the hostile 250) has it **ignored** — so the byte can carry dits again one day
without an older rig misreading it. Everything else passed as before: capability advertised, fetch flagged as
stored, a set reporting what was stored, 250 clamped to 30, and `0,30,10,0,0,5` read back after a genuine reboot.

The first run used the **Pocket as the Rig** (192.168.1.23), because the classic had dropped off the USB bus and
could not be flashed. Cleanup: `Rig Limit Kyr` set back to 3 over the link, and the Pocket left in the menu.

**The classic, once replugged and flashed, passed the same 11/11 and its own NVS round trip** (`0,30,5,0,0,3` after
the reboot that opening its serial port causes). It did *not* start on the defaults: it held Rig Limit Kyr 2, Rig
Limit SK 5 and Rig Hang 3 (150 ms), where the 2026-09-16 cleanup had left all of them at default. The serial
protocol's `GET config` reports the same values as the link, and `prefName[]`/`pliste[]` are verified aligned, so
these are genuinely what NVS holds under those keys. **Willi confirmed he set them from the Keyer's preferences**
while reviewing the `Rig:` items — which makes them the first evidence of the whole D17 path end to end through the
real UI: values chosen on the Pocket's screen, carried over the link, stored by the classic, and still there after
a reflash and two reboots. Cleanup restored the pre-test values rather than the defaults: Rig Limit Kyr back to 2.
The classic was left in Remote Rig, listening.

### §13.4 Safety — forged MAC, key-down limit, watchdog

All three behaviours verified against the classic running as Rig, driven from this Mac
(`Software/tests/kip/safety_tests.py`). The decisive evidence is a trace of the Rig's `rig_state` through one
sequence — key-down, valid keepalives, then a stream of key-ups with a corrupted MAC:

| t | rig_state | what it means |
|---|---|---|
| 0.9 s | `0x01` | the key is down; forged key-ups start going out |
| 2.0 s | `0x01` | **ignored** — the mark is still on the air |
| 2.9 s | `0x01` | still ignored |
| 4.0 s | `0x08` | the **key-down limit** fired, 3.0 s after the key went down, and cut the mark |

- **Forged MAC ignored** (spec §9, D12c: a bad MAC is dropped in silence). The mark outlived more than two seconds
  of corrupted key-ups and ended only when the limit cut it — the forged packets changed nothing.
- **Key-down limit** cut the mark at its configured 3000 ms and set bit 3, with `late 0` and no watchdog involved.
- **Watchdog**: with the Keyer silent mid-mark the key dropped after **1.0–1.1 s**, inside the 1 s keepalive timeout
  plus one STATS period.

**The harness failed three times before the firmware passed once — the fifth measuring tool in this project to pose
as a firmware fault.** In order: it asserted on the first STATS after a key-down, which routinely still reads `0x00`
because the edge must clear the playout delay and be caught by a 1 Hz report; then it *starved its own stream* while
waiting, so the Rig's watchdog lifted the key (`0x04`) and the test blamed the firmware for a mark the test itself
had released; then it sent keepalives once a second against a one-second timeout and lost the race. The lesson is
general: **a harness that waits must keep sending**, and properties about intervals ("the mark survived", "it was cut
at the limit") cannot be read from one sampled value — the tests now track the series of reports.

**The corrected harness then reproduced the trace exactly: 7/7.** Mark survived **1987 ms** of forged key-ups; the
limit cut it **3.0 s** after key-down; the watchdog dropped the key after **0.9–1.1 s** of silence. The test now
sends on a 200 ms timer rather than at the mercy of its blocking reads, prints the survival and cut timings whatever
the verdict, and reports its own worst send interval so a future failure is diagnosable from the output alone.

*Caveat, recorded rather than smoothed over:* that self-measurement came back as **1001 ms** — right at the 1 s
watchdog threshold. The run passed and its numbers match the independent trace, but the output does not say whether
the gap fell inside or after the measurement window, so the cadence deserves tightening before this test is relied on
unattended.


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
| 25 *(before the display fix)* | 3249 | 3219 (99.1 %) | −90 … **+3587 µs** | 3033 | 3006 | **−3600** … +84 µs | **0** | 0 | 0 |
| **25 (after the fix)** | **3125** | **3125 (100 %)** | **−84 … +93 µs** | **2920** | **2920 (100 %)** | **−93 … +85 µs** | **0** | 0 | 0 |
| **40** | **4962** | **4962 (100 %)** | **−98 … +88 µs** | 4961 | 4960 | −98 … **+20099 µs** | **0** | 1 | 1 |

**40 WPM: every mark within 0.2 ms; the one long space is the network, answered exactly as specified.** In the third
minute a single packet arrived after its playout time (`late 1`): the Rig grew the delay 150 → 170 ms to cover it and
shifted the timeline, and because an increase is deferred to a key-up it landed in a **gap**, which came out 20.1 ms
long. Nothing was shortened and no mark was disturbed — spec §7.3 working as written. For the remaining seven minutes
there was no further late edge or underrun and the delay held at 170 ms. (`idle -`: at 40 WPM no gap in the test text
reaches the idle threshold — 300 ms against a 168 ms word gap — so this run has no idle-gap statistics.)

After the fix, **not one interval in ten minutes was off by more than 0.5 ms** — marks, spaces and all 204 idle gaps
alike (the playout delay held at 150 ms and took no decrease that run, so no legitimate gap adjustments occurred
either). 15 WPM was measured *before* the fix and passed anyway: at that speed edges are sparse enough that a 250 ms
redraw rarely lands beside one, and its only outliers were in idle gaps, where the Rig is permitted to adjust.

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
down, never within 50 ms of the next edge.

### The display was the cause — fixed

Three minutes at 25 WPM, identical instrument settings, with and without the gate:

| | marks | within 0.2 ms | worst mark | intervals off by > 0.5 ms |
|---|---|---|---|---|
| before | 833 | 824 | +4.62 ms | 8 |
| **after** | **834** | **834** | **+90 µs** | **0** |

Every mark, every space and every idle gap inside 0.2 ms, and not one outlier row in the whole run.

**The magnitude argument was wrong, and testing it anyway is what found the bug.** A 37 ms redraw does not produce a
37 ms slip: it stalls the loop that *arms* the emitter, and only the tail of the stall overlaps an edge's due moment,
so the edge comes out late by the remainder — a few milliseconds, which is what the traces showed. The reasoning that
dismissed the display looked sound and was not.

This is a **product** fix, not a bench fix: the shipping firmware redrew every 250 ms with no regard for the key
line. The Rig now obeys what Phase 0 required of the Keyer. Two other changes made while hunting are kept because
each is right on its own terms — STATS is no longer sent next to an edge, and the alarm is re-armed whenever the
queue head moves — though neither shifted the measurements.

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

### A Remote Keyer restarted often enough stops linking — and what it actually was

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

**Seen again at 13:11, and this is the clearest instance yet.** The driver had rebooted the Pocket at the start of
that very run, and the Pocket had been through only **two mode starts since** — one successful ten-minute leg, then
this one. The classic reported Remote Rig at 13:10:50; the Keyer called it 35 s later (ample settle) and got
`No answer`, then went silent on two further attempts. At that moment the Rig accepted a HELLO from this Mac
**immediately** and held no session, so it was listening, free and reachable. Afterwards the Pocket showed the
familiar signature: `GET menu` reporting `active: true`, `PUT menu/start` returning silence, everything else
answering normally. **So the pre-run reboot does not hold the fault off** — it recurs within a couple of mode entries.

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

#### Corrected 2026-09-17: this was not a Remote Keyer defect {-}

Everything above is what was observed, and the observations stand. The **conclusions drawn from them do not**, and
two pieces of evidence arrived after they were written.

**Willi's bench evidence.** Started and left by hand, Remote Keyer has never once hung — "tested that several
times". Every instance recorded above was a run driven over the serial protocol. That alone moves the suspicion off
the mode and onto the way the bench drives it, and it retires the socket-teardown hypothesis in
`MorseKipKeyer::end()` → `begin()`: nothing in that path cares whether the mode was entered by hand or by command.

**The source explains the silence exactly.** `PUT menu/start` (`m32_v6.ino`, the `type == "menu"` branch) sets
`goToMenu = false`, and then sets `executeMenu` **and replies** only inside `if (m32state == menu_loop)`. A start
that arrives while the device is anywhere else — running a mode, or unwinding out of one — falls straight through
the handler: nothing is started, and **nothing is answered**. That is precisely the signature recorded above:
`menu/stop` answers OK, `GET menu` answers, `GET wifi` answers, and `menu/start` alone returns silence for 30 s.
With `value` given, the fall-through is worse than a no-op — `newMenuPtr` has already been changed.

So the defect is in the **serial protocol's mode-entry handling**, not in M32KIP: any mode started this way, from
any client, can be swallowed the same way, which is exactly why plain CW Keyer failed identically. **No firmware
defect in Remote Keyer is evidenced.** The fix belongs to the protocol: answer with an error instead of silence,
and — optionally — latch the request so that a start arriving during an unwind is honoured when the menu is
reached. Willi: **not a blocker at this stage.**

**What is still unexplained**, and should not be written up as solved: why `PUT menu/stop` sometimes answered OK
without returning the Pocket to the menu. `goToMenu` is consumed in the global loop (`m32_v6.ino:1340`), which is
where Remote Keyer runs, so it should have taken effect. That thread was not pulled.

**The bench driver still reboots the Pocket before each run**, but no longer as a workaround for a suspected Keyer
fault — the record above shows it never held that fault off anyway. It is there for the ordinary reason a timing
bench wants it: a known, identical starting state for every measurement.

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

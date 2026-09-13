# M32KIP Phase 1 — the protocol core, and what testing it found

Status: **Phase 1 complete, 2026-09-12.** 597 host checks pass, the Python cross-check agrees on every
wire vector, and both firmware variants build with the core in the tree.

What exists now: `Software/src/Version 6 and newer/M32Kip.{h,cpp}` — the whole protocol as
platform-free C++ (crypto, packet codecs, replay window, speed estimate, edge queue, playout and
offset tracking, and the Rig and Keyer state machines) — plus `Software/tests/kip/` with the C++ test
suite, an independent Python implementation of the wire format, and a working Python reference peer
for both roles. None of it is wired into the firmware yet; that is Phase 2.

**One decision is open — F3 below.** The rest are recorded as amendments for Draft 0.3.

---

## The runs

Every profile of spec §13.2, at 15, 25 and 35 WPM, plus a clock-drift run of a thousand marks.
"Late" counts edges that arrived after the moment they were due; "stretched" counts marks that came
out more than 2 ms longer than they were sent.

| Profile | Late edges | Worst lateness | Marks stretched | Playout delay |
|---|---|---|---|---|
| a: 50 ms, 10 ms jitter | 0 | — | 0 | 150 → 140 ms |
| b: 120 ms, 40 ms jitter | 0 | — | 0 | 150 → 140 ms |
| c: b + 2 % loss | 0–1 | 18 ms | 0 | 150 → 135–167 ms |
| d: b + 5 % loss | 0–2 | 45 ms | 0 | 150 → 140–239 ms |
| e: bursts of 3 lost packets | 2–3 | **313 ms** | 1 per ~10 bursts | 150 → 240–474 ms |
| e, with 30 ms keepalives | **0** | — | **0** | 150 → 140 ms |
| 30 ppm drift, 999 marks | 0 | — | 0 | 150 → 140 ms |

**No mark was shortened in any run, at any speed, on any profile.** That is the promise the whole
design rests on (principle 5), and it is the assertion the suite checks hardest.

---

## F1 — a playout increase applied mid-mark stretches that mark *(fixed)*

§7.3 says to absorb a late edge by increasing *D*, and that because this shifts the whole timeline the
relative timing of everything after it is preserved. That holds only while the shift lands in a gap.
Applied between a key-down and its key-up, it moves the queued key-up later and leaves the key-down
where it already went out: the mark grows by the full increase. At 35 WPM the simulation produced a
dah 50 ms too long, which is not a delayed element but a different one.

**Fixed** by holding an increase back until the key comes up, when the following gap absorbs it. Marks
stopped stretching on every profile except the burst case, where the cause is F3.

*Draft 0.3:* §7.3 should say a *D* increase is applied at a key-up boundary, never inside a mark.

## F2 — the clock-offset correction had no rate limit *(fixed)*

§7.2 caps a drift correction at 1 ms per adjustment but does not say how often an adjustment may
happen, so the implementation did one per poll. In one word gap it took nineteen steps and moved that
gap 19 ms earlier. Worse, the amount depended on how often the caller polled, which would have made
the behaviour differ between the two variants and between debug and release builds.

**Fixed:** at most one adjustment per idle gap. That is still far more correction than drift needs —
tens of ppm is a couple of milliseconds a minute, and a minute of sending has several word gaps.

*Draft 0.3:* §7.2 should state the rate, not only the step size.

## F3 — redundancy is counted in packets, but what protects an edge is time *(decided: D13)*

§6.4 makes every packet repeat the last *R*−1 edges, so that "up to *R*−1 consecutive lost packets lose
no information". True as stated, but *when* the information arrives depends on when the next packet is
sent. While sending, packets follow each edge and a loss is repaired within milliseconds. Between
words there are no edges, so the next packets are keepalives 250 ms apart, and three lost packets can
withhold an edge for the better part of a second.

That is what the burst profile shows: a worst lateness of **313 ms**, the playout delay driven from
150 ms to 474 ms trying to compensate, and roughly one disturbed element per ten bursts. Re-running
the identical losses with 30 ms keepalives gave **zero late edges and zero stretched marks at every
speed**, with the delay never leaving 140 ms. So it is the keepalive interval, not the loss, that
costs the timing.

Three ways to close it:

1. **Repeat briefly after activity, then idle slowly (recommended).** After an edge, if no further edge
   follows within ~20 ms, send the redundant repeat again, up to three times, then fall back to 250 ms
   keepalives. During continuous sending this adds *nothing* — each new edge's packet already carries
   the repeats — and it costs about three extra packets per inter-word gap. Recovery time drops to
   ~60 ms. This is what §6.4's own remark about "cheap extra redundancy after the end of a word" is
   reaching for.
2. **Raise the keepalive rate outright**, to 30–50 ms. Simple, and measured to work, but it roughly
   doubles idle traffic and spends it when nothing is happening.
3. **Leave it.** One disturbed element per ten bursts, and only on a link that loses three packets in a
   row. The max-key-down limit still bounds the worst case.

**Ratified 2026-09-13: option 1.** Three repeats 20 ms apart after each edge, then the 250 ms idle
keepalive, implemented in the core as `KeyerSession::sendDue()` / `noteSent()` and exercised by the
simulation. Every profile at every speed now runs with zero late edges, zero disturbed marks and a
playout delay that never leaves 140 ms; the Draft 0.2 cadence is kept as a control run so a regression
in either is visible.

One correction to what is written above: this does **not** cost nothing during continuous sending. A
20 ms repeat interval is shorter than an element at every practical speed, so a repeat fires between
most edges and the same message costs about three times the packets, putting the link near 3 kB/s
rather than §5's 1–2 kB/s estimate. Measured figures and the tuning knob are in `DECISIONS.md` D13.
§11 gains a repeat count and interval; §5's bandwidth figure needs updating.

## Smaller things

- **§6.4 contradicts itself on keepalives.** One sentence says a keepalive is "a `KEY` packet with
  `n = 0`", the next says keepalives "also carry the last *R*−1 edges once more". Both cannot hold.
  The implementation takes the second reading — a keepalive repeats the last *R*−1 edges, and `n` is 0
  only when there is no history yet — because the first would throw away exactly the redundancy the
  paragraph says is wanted. Worth fixing in the text.
- **§7.4.2's slack is written as part of the lateness test**, which reads as though an edge arriving
  half a millisecond *before* it is due were late by a negative amount. Implemented that way it
  underflowed an unsigned subtraction, reported a lateness of four billion ticks, and pinned the
  playout delay to its 600 ms ceiling — caught by the simulation, and the reason the burst profile
  first showed a 474 ms delay. The slack only ever meant "do not arm a timer for something this
  close"; the lateness test needs no slack at all.
- **`NACK(auth)` is gone**, per D12c: a packet that fails its MAC is dropped in silence.

## What Phase 2 inherits

Requirements on `MorseKipRig` and `MorseKipKeyer` that are now settled by measurement rather than by
argument:

- The Keyer **must not send from the loop** — Phase 0 measured 2–8 ms of stall per packet — and **must
  not redraw the display while an edge can arrive** (37 ms on the OLED, 46 ms on the TFT).
- The Rig emitter is a hardware-timer ISR on core 1: 45–96 µs worst case under load, both variants.
- `WiFi.setSleep(false)` for the duration of a session.
- The core's `poll()` is the reference semantics for the emitter. On the device the ISR pops the queue
  directly; the queue holds sender time and computes the emission moment on demand, so a change to
  *D* or the offset shifts the whole timeline at once, which is what keeps F1's fix correct.

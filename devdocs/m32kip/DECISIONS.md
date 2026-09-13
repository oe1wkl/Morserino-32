# M32KIP — ratified decisions

Willi's sign-off of 2026-09-12 on the decision list in `IMPLEMENTATION_PLAN.md` §3.
All recommendations were accepted except D2. These are binding for the implementation;
amend the spec (`SPEC.md`, Draft 0.2) accordingly when it is next revised.

| # | Decision |
|---|---|
| D1 | Rig side is built for **both variants** under `CONFIG_M32KIP`; the Phase 0 timer measurement decides whether the classic ships it. |
| D2 | **Changed from the recommendation.** Straight-key / bug edges are captured on the **existing loop-polled decoder path** (`Decoder::checkInput()` noise blanker → `keyOut()`), not by a GPIO interrupt. Consequence: the timing targets of spec §4.3 are **relaxed to 1 ms** (capture accuracy, emission accuracy, and the reproduced-element error — the 0.2 ms / 0.5 ms figures are withdrawn). Willi's judgement: 1 ms is immaterial even at very high speed. **Phase 0 result (classic):** the polled loop is good to 0.24 ms provided the Keyer mode (a) never sends from the loop — `writeTo()` stalls it 2–7 ms — and (b) never redraws the display while an edge can arrive (37 ms per OLED redraw). Both are now requirements on `MorseKipKeyer` (see `PHASE0_RESULTS.md`). |
| D3 | Rig host = the selected WiFi entry's **TRX Peer**. PSK entered via the AP web form and the serial protocol only, write-only like the WiFi password. Port fixed at **7374**, protocol-only override. |
| D4 | On-device preferences: Rig — Playout Delay (10 ms units, 0 = adaptive), Max Key-Down keyer (s), Max Key-Down manual (s), PTT Lead (ms, 0 = off), PTT Hang (100 ms units). Keyer — Glitch Filter (1–5 ms). All other §11 parameters are compile-time constants. |
| D5 | Two leaves appended to the menu enum, wired into the Transceiver ring next to WiFi Trx: `Remote Keyer` / `Remote Rig` (labels can still be tuned). The working name M32KIP does not appear on the device. |
| D6 | PTT protocol fields and Rig logic implemented behind a `PIN_PTT` build flag; unset in both shipped envs, so the Rig reports PTT unavailable. |
| D7 | No "external keyer" keyer-mode option in v1; source 1 covers it. |
| D8 | Rig mode: deep-sleep timeout off, battery warning on entry, always return to listening after a session ends; mode-exit strategy follows whatever `reboot-on-wifi-exit` settles on. |
| D9 | Serial protocol 1.5 with `GET/PUT kip/...` in v1; Config Tool and iOS app updates are follow-ups. |
| D10 | Spec committed as `devdocs/m32kip/SPEC.md`. |
| D11 | **No logic analyser is available.** Timing is self-measured in firmware (hardware timer vs. `esp_timer`), and the Python reference peer injects network impairments in software. The §13 acceptance tests are read against these instruments. |
| D12 | Spec amendments to fold into Draft 0.3: (a) §7.6 emitter = hardware-timer ISR via the Arduino `hw_timer` API (no `esp_timer` ISR dispatch in the prebuilt core); (b) §4.3/§13.1 acceptance is relative to the Keyer unit's own key line, which has millisecond granularity; (c) drop `NACK(auth)` — bad MACs are silently dropped; (d) §8: the Keyer unit forces noTx regardless of the `Key Ext Tx` preference. |

## Decided since Phase 1

**D13 — the keepalive and repeat schedule. RATIFIED 2026-09-13: the repeat schedule.** After each edge
the Keyer sends up to three more copies of the same redundant packet, 20 ms apart, and then falls back
to the 250 ms idle keepalive. Implemented in the core as `KeyerSession::sendDue()` / `noteSent()`, so
the cadence is host-tested rather than buried in the device layer.

Measured across every impairment profile at 15, 25 and 35 WPM:

| | Late edges | Worst lateness | Marks disturbed | Playout delay |
|---|---|---|---|---|
| Repeat schedule (ratified) | **0** everywhere | none | **0** everywhere | never leaves 140 ms |
| Draft 0.2's 250 ms keepalives | 2–3 per run | 233 ms | up to 2 per run | driven to 561 ms |

**What it costs, and a correction.** Phase 1 claimed this would add nothing during continuous sending,
because each new edge's packet already carries the repeats. That was wrong, and the measurement says
so: a 20 ms repeat interval is shorter than an element at every practical speed, so a repeat does fire
between most edges. The same message costs roughly **three times the packets** — 1209 against 325 at
15 WPM, 1000 against 304 at 25 WPM, 898 against 309 at 35 WPM — which puts the link at about 3 kB/s
where §5 estimated 1–2 kB/s. That is still small, and it buys a link that never once ran late in any
profile, but the figure in §5 needs updating for Draft 0.3.

If 3 kB/s ever turns out to matter, the knob is in `KeyerSession::begin()`: two repeats at 30 ms holds
the same ~60 ms recovery bound for fewer packets. Nothing else depends on the numbers.

## Phase 0 instruments

- `M32KipSpike.cpp` (`-D KIP_SPIKE=1` on the command line, never in `platformio.ini`) boots the
  device into a measurement loop: one-shot hardware-timer alarms 5–60 ms apart on core 1, toggling
  the real key line; the ISR records its own lateness. The loop meanwhile does what an interactive
  mode does (touch reads, 4 Hz display, 50 pps UDP send) and records the gap between passes.
- `Software/tests/kip/spike_load.py` floods the device with echo packets and reports LAN RTT/loss.
- Results and the D1 verdict go into `PHASE0_RESULTS.md`.

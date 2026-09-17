# M32KIP — Morserino Keying over IP

**Real-time CW keying between two Morserino-32 units over the Internet**

Document status: Draft 0.2 (proposal for implementation; 0.2 adds straight-key / bug handling, §4.4)
Working name: *M32KIP* 
Target firmware: Morserino-32 v9, classic and Pocket

---

## 1. Scope and goals

### 1.1 Problem

Remote transceivers are operated over the Internet through PC software. CW is poorly served by this: the operator's keying has to survive network latency *and* jitter, and any jitter directly distorts element lengths. MOPP (Morse Over Packet Protocol, UDP 7373) already links two M32 units, but it transmits decoded characters word by word, which adds a delay of a full word plus the inter-word gap and is not usable for real QSO keying.

### 1.2 Goal

Transmit the *timing* of the keyer output of one M32 (the **Keyer unit**) to a second M32 (the **Rig unit**) whose transceiver output keys the remote radio, such that:

- element and gap lengths are reproduced exactly (target: ≤ 0.5 ms error, 99th percentile);
- the constant end-to-end delay is as small as the network allows (typically 100–250 ms);
- the link is robust against packet loss, reordering and jitter;
- the Rig unit can never leave the transmitter keyed if something goes wrong;
- nobody but the licensed operator can key the transmitter.

### 1.3 Non-goals (v1)

- Transporting receive audio or sidetone. Audio comes back through the existing remote-station software; sidetone is generated locally on the Keyer unit as today.
- Rig control (frequency, mode). Out of scope; use the existing CAT path.
- Bidirectional keying. One direction per session. Two sessions can run in opposite directions if ever needed.
- Encryption. Keying timing is not secret. Authentication *is* required (see §9).

### 1.4 Terminology

| Term | Meaning |
|---|---|
| Keyer unit (client) | M32 at the operator. Runs the iambic keyer (or straight key input) and sidetone, initiates the session, sends edges. |
| Rig unit (server) | M32 at the transceiver. Listens on a UDP port, reconstructs the key line, drives TX-out and optionally PTT. |
| Edge | A transition of the key line: key-down (mark begins) or key-up (space begins). |
| Tick | Wire time unit, 100 µs. |
| Playout delay *D* | Constant delay added on the Rig unit between sender timestamp and reproduced edge. |
| PSK | Pre-shared key (passphrase) configured on both units. |

---

## 2. Design principles

1. **Edges, not samples.** The Keyer unit sends the *time* of each edge, not the current key state. The Rig unit rebuilds the waveform from timestamps.
2. **Constant delay beats low delay.** Every edge is reproduced at `t_sender + D`. Jitter is absorbed by *D*; latency is not fought, only made constant.
3. **UDP only.** No TCP anywhere in the real-time path.
4. **Redundancy instead of retransmission.** Every packet repeats the last few edges. Loss of a single packet costs nothing; loss of several consecutive packets costs at most a lengthened gap, never a clipped element.
5. **Never shorten a mark.** If reconstruction is in doubt, extend a space. The receiver may delay, it may never distort.
6. **Fail safe = key up.** Any anomaly (link loss, watchdog, bad authentication) results in key-up and PTT-off.
7. **Time-critical output is hardware-timed.** Edges are emitted from a hardware timer, not from the main loop.

---

## 3. System overview

```
 Operator                                            Remote site
 ┌──────────────────────────┐   UDP/IP    ┌────────────────────────────┐
 │ M32 Keyer unit           │────────────▶│ M32 Rig unit               │
 │  paddles → iambic keyer  │  KEY pkts   │  jitter buffer (D)         │
 │  local sidetone          │◀────────────│  hw-timer scheduler        │
 │  display: link stats     │  STATS pkts │  TX-out (key), PTT-out     │
 └──────────────────────────┘             └──────────────┬─────────────┘
                                                         │ key / PTT
                                                   ┌─────▼──────┐
                                                   │ transceiver│
                                                   └────────────┘
```

Session flow:

1. Rig unit boots into *Rig mode*, connects to WiFi, listens on the M32KIP port.
2. Keyer unit enters *Remote keying mode*, sends `HELLO` to the configured host.
3. Rig unit verifies the HMAC, answers `HELLO_ACK`.
4. Keyer unit streams `KEY` packets (edges + keepalives). Rig unit streams `STATS` back once per second.
5. Either side sends `BYE` to end; loss of keepalives ends the session implicitly.

---

## 4. Timing model

### 4.1 Sender clock

The Keyer unit timestamps each edge with its own free-running monotonic clock (`esp_timer_get_time()`, µs), converted to ticks of 100 µs, truncated to 32 bits. The wrap period is ≈ 119 hours; all timestamp comparisons use modular arithmetic (`(int32_t)(a - b)`).

The clock does **not** need to be synchronised with the Rig unit. Only the *difference* between consecutive sender timestamps matters; the Rig unit maps sender time to its own clock through an estimated offset (§7.2).

### 4.2 What is timestamped

The output of the keyer state machine, i.e. the fully formed iambic element stream (dits, dahs, and the inter-element gaps the keyer inserts), or the raw contact closure when a straight key / external keyer is attached. Paddle contacts themselves are **never** sent — the keying logic stays on the Keyer unit so that sidetone and key feel are local and unaffected by the network.

### 4.3 Resolution and accuracy targets

| Quantity | Target |
|---|---|
| Timestamp resolution on wire | 100 µs |
| Timestamp capture accuracy (Keyer) | ≤ 100 µs relative to actual key-line edge |
| Edge emission accuracy (Rig) | ≤ 200 µs relative to scheduled time |
| Reproduced element length error | ≤ 0.5 ms (99th percentile), ≤ 1 ms (max) |

For reference: a dit at 40 WPM is 30 ms; 0.5 ms is 1.7 %.

### 4.4 Key sources

The Keyer unit reports one of three **sources** in `HELLO` and in every `KEY` packet:

| `source` | Meaning | Timing origin | `wpm` field |
|---|---|---|---|
| 0 | Internal keyer (iambic / Ultimatic etc.) | Keyer state machine; elements are machine-exact | Valid |
| 1 | Straight key or bug (semi-automatic) | Raw contact closure, debounced; timing is human/mechanical | `0` = unknown |
| 2 | External keyer or keying line fed into the paddle jack | Raw contact closure, debounced | `0` = unknown |

The protocol is identical for all three; what changes is what the Rig unit may assume:

- **Nothing on the Rig side depends on `wpm`.** It is informational only (display, logging). All gap classification uses a speed estimate derived on the Rig unit from the edge stream itself (§7.5), so a missing or wrong `wpm` cannot affect reconstruction.
- **Marks can be arbitrarily long** with sources 1 and 2 — operators hold a straight key down to tune. The maximum key-down limit (§8) is therefore a Rig-side configuration item with separate defaults per source, and the Keyer unit is told the active limit in `HELLO_ACK` so it can warn on its display.
- **Contact bounce is the Keyer unit's problem**, not the protocol's. Edges are timestamped *after* the glitch filter (§10.1); the filter's fixed delay is absorbed into *D* like any other latency. No edge shorter than the filter width ever appears on the wire, so the Rig unit's state-alternation check (§6.4) remains valid.
- **Very short marks** occur with a bug (dits of 20–25 ms at speed) and with fast straight-key operators. The glitch filter must stay well below that (≤ 5 ms), and the reconstruction accuracy targets in §4.3 apply unchanged.

---

## 5. Transport

- **Protocol:** UDP, IPv4 (IPv6 permitted if the stack supports it).
- **Default port:** 7374 (MOPP uses 7373; adjacent, distinct).
- **Direction / NAT:** The Keyer unit sends first. The Rig unit always replies to the source address and port of the most recent authenticated packet. Only the Rig side needs a port forward (or a static/DDNS address). No relay in v1.
- **Packet size:** all packets fit in one frame well under 200 bytes; no fragmentation.
- **Rate:** one `KEY` packet per edge, plus keepalives every 250 ms when idle. Worst case around 25 packets/s at very high speed. Roughly 1–2 kB/s.
- **Loss / reorder:** handled by redundancy and per-edge deduplication (§6.4, §7.4). No ACKs, no retransmission in the real-time path.

---

## 6. Packet formats

All multi-byte integers are little-endian (native ESP32 order, no conversion cost). Packet layout is fixed; no variable-length encodings except the edge list.

### 6.1 Common header (12 bytes)

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 1 | `magic` | `0x4B` ('K') |
| 1 | 1 | `version` | Protocol version, `0x01` |
| 2 | 1 | `type` | Packet type (§6.2) |
| 3 | 1 | `flags` | Type-specific flags, 0 if unused |
| 4 | 4 | `session` | Session ID assigned by the Rig unit in `HELLO_ACK`; `0` in `HELLO` |
| 8 | 2 | `seq` | Per-sender sequence number, increments by 1 per packet, wraps |
| 10 | 2 | `reserved` | 0 |

The header is followed by the type-specific payload, followed by the 8-byte **MAC** (§9). Packets with a bad magic, unsupported version, wrong session or invalid MAC are silently discarded.

### 6.2 Packet types

| Type | Value | Direction | Purpose |
|---|---|---|---|
| `HELLO` | 1 | Keyer → Rig | Open session |
| `HELLO_ACK` | 2 | Rig → Keyer | Accept session, hand out session ID and rig parameters |
| `KEY` | 3 | Keyer → Rig | Edge stream and keepalive |
| `STATS` | 4 | Rig → Keyer | Link and buffer statistics |
| `BYE` | 5 | either | Close session |
| `NACK` | 6 | Rig → Keyer | Session refused (busy, version) |

### 6.3 `HELLO` / `HELLO_ACK` / `NACK`

`HELLO` payload (16 bytes):

| Size | Field | Description |
|---|---|---|
| 8 | `nonce_c` | Random, generated by the Keyer unit |
| 4 | `t_now` | Keyer clock, ticks, at send time |
| 1 | `wpm` | Current keyer speed, informational only; `0` = unknown (straight key, bug, external keyer) |
| 1 | `redundancy` | Requested edge redundancy *R* (default 4) |
| 1 | `source` | Key source (§4.4): 0 keyer, 1 straight key/bug, 2 external |
| 1 | `caps` | Capability bits, 0 in v1 |

`HELLO_ACK` payload (24 bytes):

| Size | Field | Description |
|---|---|---|
| 8 | `nonce_c` | Echoed |
| 8 | `nonce_s` | Random, generated by the Rig unit |
| 4 | `session` | Newly assigned session ID (non-zero) |
| 2 | `max_keydown_ms` | Rig-enforced maximum mark length for this session (chosen by the Rig unit from the `source` in `HELLO`, §8) |
| 1 | `ptt_lead_ms` | Configured PTT lead time (0 = PTT disabled) |
| 1 | `flags` | bit0: PTT available; others 0 |

`NACK` payload (2 bytes): `reason` (1 = busy, 2 = version, 3 = auth) + reserved.

The `HELLO` is retried every 500 ms up to 10 times. A `HELLO` with an unknown `nonce_c` in the `ACK` is ignored (replay protection). After `HELLO_ACK`, both nonces are mixed into the per-session MAC key (§9), so packets from an old session cannot be replayed into a new one.

### 6.4 `KEY`

Payload:

| Size | Field | Description |
|---|---|---|
| 4 | `t_now` | Keyer clock, ticks, at send time (drives offset/drift tracking and keepalive) |
| 1 | `wpm` | Current keyer speed, informational only; `0` = unknown |
| 1 | `n` | Number of edges in this packet, 0–8 |
| 1 | `source` | Key source (§4.4); may change mid-session, e.g. when the operator switches keyer mode. The Rig unit re-selects `max_keydown_ms` on the next idle gap and reports the new value in `STATS` |
| 1 | `reserved` | 0 |
| 5 × n | `edges[]` | Newest edge **last** |

Each edge (5 bytes):

| Size | Field | Description |
|---|---|---|
| 4 | `t` | Edge time, Keyer clock, ticks |
| 1 | `state` | `1` = key-down (mark begins), `0` = key-up |

**Redundancy rule.** On every new edge the Keyer unit sends a `KEY` packet immediately containing the new edge and the *R−1* preceding edges (*R* from `HELLO`, default 4). Thus up to *R−1* consecutive lost packets lose no information. A `KEY` packet with `n = 0` is a keepalive; keepalives are sent every 250 ms while no edge has occurred, and also carry the last *R−1* edges once more (cheap extra redundancy after the end of a word).

The first edge in a session must be a key-down; edges strictly alternate in state. A Rig unit that sees two consecutive edges of the same state after deduplication treats the earlier one as authoritative and logs a protocol error (§7.4).

### 6.5 `STATS`

Sent by the Rig unit once per second while a session is active. Payload:

| Size | Field | Description |
|---|---|---|
| 2 | `playout_ms` | Current *D* in ms |
| 2 | `jitter_100us` | Current jitter estimate (≈ 3σ of arrival deviation) |
| 1 | `loss_pct` | Packet loss over last 10 s, percent |
| 1 | `late_edges` | Edges that arrived after their playout time, last 10 s |
| 1 | `underruns` | Times *D* had to be increased, last 10 s |
| 1 | `rig_state` | bit0 key down, bit1 PTT active, bit2 watchdog tripped since last STATS, bit3 max-keydown limit hit since last STATS |
| 2 | `max_keydown_ms` | Currently enforced mark limit (may change with `source`) |
| 2 | `dit_est_100us` | Rig-side speed estimate (§7.5), 0 if none yet; lets the Keyer display the speed the Rig "sees" for straight-key operators |
| 4 | `t_echo` | `t_now` of the most recently received `KEY` |
| 4 | `t_rig` | Rig clock at send time, ticks (informational) |

(Payload is 20 bytes.)

The Keyer unit shows `playout_ms`, `loss_pct` and a quality indicator on its display. From `t_echo` and its own clock it can estimate RTT.

### 6.6 `BYE`

No payload. Sent three times, 100 ms apart. Receipt causes immediate key-up / PTT-off (after hang time) and session teardown.

---

## 7. Rig unit: reconstruction

### 7.1 Data structures

- **Offset** `off`: estimate of `rig_clock − keyer_clock` in ticks (int32, modular).
- **Playout delay** `D` (ticks).
- **Edge queue**: sorted by edge time, deduplicated by exact `t`. Small (a few dozen entries suffices).
- **Last emitted edge**: time and state, used for state consistency and watchdog.

### 7.2 Offset and drift estimation

On each incoming `KEY`, compute `d = rig_now − (t_now + off)`.

- On the first packet of a session, set `off = rig_now − t_now`, so `d = 0`.
- Track `d_min` over a sliding window (e.g. last 5 s of packets): this approximates the fastest path through the network, i.e. the least-jittered arrival.
- Track a jitter estimate `J` (e.g. exponentially weighted absolute deviation of `d` from `d_min`, scaled to ≈ 3σ).
- **Drift**: ESP32 crystals differ by tens of ppm, i.e. tens of ms per ten minutes. `d_min` slowly walks with drift. Correct `off` toward `d_min` **only while the key line has been idle for at least the gap threshold (§7.5)** and in steps ≤ 1 ms per adjustment. Never adjust `off` while an edge is pending within the next *D*.

### 7.3 Playout delay `D`

- Initial `D` = configured default (150 ms), or the user-fixed value if the user disabled adaptation.
- Adaptive target: `D_target = d_min_margin + J + safety`, where `safety` defaults to 20 ms. Clamp to `[D_min, D_max]` (defaults 40 ms … 600 ms).
- **Increase** `D` immediately when a *late edge* occurs (§7.4) — but implement the increase as a shift of the whole timeline (see below), never as a per-edge change.
- **Decrease** `D` toward `D_target` only during idle gaps, in steps ≤ 5 ms, at most once per 2 s, and only after 10 s without any late edge. The decay is deliberately slow; a good link gets a short `D` after a minute, a bad link never oscillates.

Scheduled emission time for an edge: `t_emit = t + off + D`.

### 7.4 Receiving edges

For each edge in a `KEY` packet:

1. If `t` already in the queue or older than the last emitted edge and already emitted → duplicate, ignore. (Redundancy makes duplicates the normal case.)
2. Compute `t_emit`. If `t_emit > rig_now + slack` (slack ≈ 1 ms): insert into queue.
3. Otherwise the edge is **late** by `L = rig_now − t_emit`:
   - if `L ≤ 2 ms` and the edge is a **key-up**: emit now (the mark is stretched by ≤ 2 ms — accepted rather than shifting the timeline);
   - otherwise: set `D += L + 10 ms` (which also shifts every queued edge), then insert. Because the shift applies to the whole timeline, the *relative* timing of all subsequent edges — including the partner edge of the late one — is preserved. The net effect is one lengthened space, never a shortened mark. Count as `underrun`.
4. State consistency: if the edge would not alternate with its queue neighbours, keep the earlier edge, drop the later one, count a protocol error.

### 7.5 Speed estimate and gap threshold

The Rig unit needs to know when it is safe to adjust `off` and `D`, i.e. when the operator is between words rather than between elements. It must not rely on the `wpm` field, which is meaningless for a straight key or bug. Instead it keeps its own **dit estimate** `dit_est` from the edge stream — the same idea the M32 decoder already uses:

- Keep the last 16 mark lengths. `dit_est` = median of those marks that are shorter than 0.6 × the longest mark in the set (i.e. the "short" cluster). If the set contains only one cluster (e.g. only dahs so far, or a tuning carrier), fall back to the previous `dit_est`; if none exists, use 60 ms (20 WPM).
- Ignore marks longer than 1 s (tuning) for estimation purposes.
- Reset the estimate on session start and whenever `source` changes.

An **idle gap** is then a key-up period of at least `max(300 ms, 6 × dit_est)`, and adjustments are additionally suppressed while any edge is queued.

Rationale for the constants: a machine-keyed word gap is 7 dits, so 6 dits catches word gaps with margin against the estimate being slightly high; human straight-key spacing is irregular and often *shorter* than standard between characters but *longer* between words, so the 300 ms floor keeps a fast, tightly-spaced straight-key operator from triggering adjustments inside a word. At 40 WPM (`dit_est` 30 ms) the threshold is 300 ms; at 15 WPM (80 ms) it is 480 ms.

For source 0 (internal keyer) the estimate will simply confirm the known speed; using it uniformly means one code path and no special cases.

### 7.6 Emission

- A hardware one-shot timer (`esp_timer` in ISR-dispatch mode, or a GPTimer alarm) is armed for the head of the queue. When it fires, the GPIO is written directly (`gpio_set_level` / register write), the state is recorded, and the timer is re-armed for the next queued edge.
- The queue-manipulating task runs on the core that does **not** run the WiFi stack (core 1 on classic ESP32), at high priority. WiFi and display code introduce multi-ms stalls; they must not sit between the timer and the GPIO.
- Display updates and STATS generation on the Rig unit are strictly lower priority and rate-limited (STATS once per second, display at most 4 Hz).

### 7.7 PTT

If PTT is enabled (`ptt_lead_ms > 0`): because every key-down is known *D* in advance, PTT is asserted `ptt_lead_ms` before the first key-down of a transmission and released `ptt_hang_ms` after the last key-up. Constraint: `ptt_lead_ms ≤ D − 10 ms`; if `D` is reduced below that, the lead is reduced accordingly. Hang time is a Rig-side setting (default 500 ms). PTT is dropped unconditionally on any watchdog event.

---

## 8. Safety

All of the following are enforced by the Rig unit and cannot be overridden by the Keyer unit:

| Condition | Action |
|---|---|
| Mark longer than `max_keydown_ms` (source 0: default 3000; sources 1/2: default 10000) | Key up immediately; ignore the rest of that mark; set bit3 in STATS `rig_state`. Keying resumes with the next key-down. |
| No authenticated packet for `keepalive_timeout` (default 1000 ms) | Key up, start PTT hang, drop session after 5 s |
| `BYE` received | Key up, PTT off after hang, session closed |
| Queue overflow, protocol error storm (> 10/s) | Key up, session closed, `NACK` on next `HELLO` for 5 s |
| Any packet with bad MAC / session / version | Silently dropped; does not reset the keepalive timer |
| Second `HELLO` from another address while a session is active | `NACK(busy)`. One session per Rig unit. |
| Local override | A physical key/paddle on the Rig unit, or the Rig unit's encoder button, terminates the session and forces key up |

The Keyer unit for its part keys **nothing locally** into a transmitter while in Remote keying mode; only sidetone.

**Tuning.** Straight-key operators key down to tune; with the default 10 s limit for sources 1/2 this works as expected, and the Keyer unit shows a countdown-style warning once a mark exceeds half the limit (it knows the limit from `HELLO_ACK`). A dedicated "tune" mode is deliberately not part of the protocol: it would be a second way to key the transmitter, with its own safety logic, for no gain over simply holding the key. Operators with an internal keyer can tune by switching the Keyer unit to straight-key mode (which changes `source` to 1 mid-session, §6.4).

---

## 9. Authentication

- Both units hold the same PSK, a passphrase of at least 12 characters, entered via the existing configuration path (serial config utility / web config) and stored in NVS. It is never sent over the air.
- `K_base = SHA-256(PSK)`.
- `HELLO` and `NACK` are authenticated with `HMAC-SHA256(K_base, header ‖ payload)`, truncated to the first 8 bytes.
- After the handshake, `K_sess = HMAC-SHA256(K_base, nonce_c ‖ nonce_s)`; all subsequent packets use `HMAC-SHA256(K_sess, header ‖ payload)` truncated to 8 bytes.
- Replay protection: the Rig unit accepts a `KEY` packet only if `seq` is within a window of 64 ahead of / 64 behind the highest seen (reordering allowed), and never accepts a `seq` twice. `HELLO` replay is defeated by the nonces.
- mbedTLS on ESP-IDF provides SHA-256/HMAC; cost is well under 100 µs per packet and runs in the network task, off the timing-critical path.

An 8-byte truncated MAC gives 2⁶⁴ work per forgery attempt against a UDP service that rate-limits failures — more than adequate for the threat (someone keying your transmitter).

---

## 10. Keyer unit behaviour

- Enters *Remote keying (M32KIP client)* mode from the menu; parameters: host (IP or hostname), port, PSK (pre-configured), redundancy *R*.
- Sends `KEY` packets from a dedicated network task fed through a queue; the keying path itself never blocks on the socket.
- Sidetone unchanged and local.

### 10.1 Edge capture per source

**Source 0 (internal keyer):** edges are timestamped in the keyer's element state machine at the exact point where the local TX-out would be toggled. Capture accuracy is limited only by the timer resolution of the state machine.

**Sources 1 and 2 (straight key, bug, external keyer):** the contact input is read through a glitch filter and timestamped at the filter's output:

- Filter: an edge is accepted only if the new level has been stable for `glitch_filter_ms` (default 3 ms, range 1–5 ms). A level change that reverts within the window is discarded entirely, so neither edge of a bounce reaches the wire.
- The timestamp of an accepted edge is the time the level *first* changed, not the time the filter confirmed it (i.e. `t_confirm − glitch_filter_ms`). This keeps the reported timing identical to the physical key movement; the filter only adds a constant reporting delay, which *D* absorbs.
- Implementation: sample the input from a periodic timer at 1 kHz or, better, use a GPIO interrupt to capture the raw edge time and arm a one-shot timer of `glitch_filter_ms` to confirm it. The interrupt approach gives 100 µs-class capture accuracy even with the main loop busy, which matters because — unlike source 0 — a human key edge can occur at any instant.
- Bugs produce dits down to ~20 ms and their dit contact can chatter; 3 ms handles typical chatter while leaving a 6× margin to the shortest legitimate mark. If a specific key needs more, the user raises the filter; the protocol does not care.
- The local sidetone for sources 1/2 follows the *raw* debounced contact as it does today; it is not derived from the filtered edge stream.

The `source` field follows the keyer mode setting of the Keyer unit (iambic/Ultimatic → 0, straight key → 1). An "external keyer" setting (→ 2) is worth adding to the keyer-mode menu for this use case; functionally it is identical to source 1 with the filter possibly set shorter.
- Display shows: connection state, `D`, loss, jitter/quality bar, the Rig unit's key/PTT state (from STATS), and the decoded text as today.
- Keepalives every 250 ms; `BYE` on leaving the mode.
- The Keyer unit tolerates missing STATS silently (they are informational) but shows "no link" if none arrive for 5 s.

---

## 11. Configuration parameters and defaults

| Parameter | Side | Default | Range |
|---|---|---|---|
| `port` | both | 7374 | 1024–65535 |
| `psk` | both | — | ≥ 12 chars |
| `redundancy R` | Keyer | 4 | 1–8 |
| `keepalive_interval_ms` | Keyer | 250 | 100–1000 |
| `D_default_ms` | Rig | 150 | 40–600 |
| `D_adaptive` | Rig | on | on / off (fixed = `D_default`) |
| `D_min_ms` / `D_max_ms` | Rig | 40 / 600 | — |
| `safety_ms` | Rig | 20 | 5–100 |
| `max_keydown_keyer_ms` (source 0) | Rig | 3000 | 500–30000 |
| `max_keydown_manual_ms` (sources 1/2) | Rig | 10000 | 500–30000 |
| `glitch_filter_ms` (sources 1/2) | Keyer | 3 | 1–5 |
| `keepalive_timeout_ms` | Rig | 1000 | 500–5000 |
| `ptt_lead_ms` | Rig | 0 (off) | 0–100 |
| `ptt_hang_ms` | Rig | 500 | 0–2000 |

---

## 12. Hardware considerations

### 12.1 Classic M32 and M32 Pocket

Both run ESP32-family SoCs with WiFi and the ESP-IDF timer and GPIO primitives this design relies on, so the protocol itself is platform-neutral. Points to verify per platform during implementation:

- **Timer path:** confirm the chosen one-shot timer fires with ≤ 200 µs latency under WiFi load on each SoC; measure with a logic analyser (§13).
- **Core assignment:** on dual-core parts, pin the scheduler task to the non-WiFi core; on any single-core variant, use an ISR-level timer callback with the GPIO write inside the ISR.
- **Display stalls:** TFT updates on the Pocket and I²C OLED updates on the classic can both block; rate-limit and keep them off the scheduler task.
- **TX-out:** the existing optocoupler output is used unchanged for the key line. PTT requires a second isolated output; on the classic this may mean repurposing an existing pin, on the Pocket it depends on spare GPIOs.

Where the classic hardware cannot meet the timing target under WiFi load, ship the Rig side for the Pocket only and keep the Keyer side (which is timing-tolerant — it only timestamps) on both.

### 12.2 A reduced "Rig-side" board

A dedicated Rig unit needs only: ESP32 module with WiFi (Ethernet would be a welcome option — wired links have far less jitter than WiFi), two isolated outputs (key, PTT), one status LED or tiny display, one button for local override/reset, and USB for configuration. No audio chain, no encoder, no paddle inputs. The protocol's Rig-side state machine is small enough that such a board would run the same firmware module with the UI stripped.

---

## 12a. Remote configuration of the Rig unit (D17, for Draft 0.3)

A rig that cannot be adjusted from the operating position is half a feature: the playout delay, the key-down limits
and the break-in compensation all have to be tuned against the transmitter, which is at the other end of the link.
Three packet types carry that, **sealed with the session key** like `KEY`, so only the current session holder can
read or change anything.

| Type | Direction | Payload | Meaning |
|---|---|---|---|
| 7 `CFG_REQ` | Keyer → Rig | none | send me your settings |
| 8 `CFG_VAL` | Rig → Keyer | 8 bytes | these are my settings (also the acknowledgement of a `CFG_SET`) |
| 9 `CFG_SET` | Keyer → Rig | 8 bytes | use these, and store them |

The 8-byte payload is shared by `CFG_VAL` and `CFG_SET`, each value exactly as the preferences hold it, so nothing is
scaled twice and a reply can be compared with a request field by field:

| Offset | Field | Meaning |
|---|---|---|
| 0 | `playout` | Rig Delay: 0 = adaptive, else an index into the delay table |
| 1 | `limit_keyer` | Max key-down for source 0, seconds |
| 2 | `limit_manual` | Max key-down for sources 1/2, seconds |
| 3 | `first_ext` | First-element extension, ms, 0 = off |
| 4 | reserved | must be zero (was `hang_unit`; see D16 amendment) |
| 5 | `hang` | Break-in hang, 50 ms steps |
| 6 | `flags` | bit0 `CFG_STORED`: these values are in the Rig's NVS |
| 7 | reserved | must be zero |

Rules, all of which follow from the Rig being a transmitter rather than a settings server:

- **Capability is advertised**, in `HELLO_ACK` `flags` bit 1. A Rig that predates this never sets it, so a Keyer
  hides the remote settings instead of waiting for an answer that cannot come. No version break.
- **Values are clamped by the Rig** to each parameter's own range. A stale or hostile Keyer cannot push a
  transmitter's key-down limit out of range.
- **The reply says what was stored**, not what was asked for, so the operator sees the Rig's own truth after any
  clamping.
- **Neither the reply nor the NVS write may happen next to an edge.** `writeTo()` blocks and an NVS write erases
  flash; both wait for the key to be up with no edge due, exactly as `STATS` and the display redraw do. The one
  window where this could still collide with keying is the operator's return from the preferences menu, and the
  Keyer holds off keying briefly to cover it.
- **The pre-shared key is neither readable nor settable** over the link, as it is not over the serial protocol.

## 13. Test plan

1. **Bench loopback:** Keyer and Rig unit on the same LAN. Logic analyser on the Keyer's local key line and the Rig's TX-out. Acceptance: every mark and space within 0.5 ms of the original at 15, 25 and 40 WPM, over 10 minutes of continuous keying.
2. **Impairment tests:** a Linux box with `tc netem` between the units. Profiles: (a) 50 ms delay, 10 ms jitter; (b) 120 ms delay, 40 ms jitter; (c) profile (b) plus 2 % loss; (d) profile (b) plus 5 % loss with reordering; (e) burst loss of 3 consecutive packets. Acceptance for (a)–(d): no shortened mark, ever; underruns ≤ 1 per minute after the first 30 s; `D` converges within 60 s and does not oscillate. Acceptance for (e): no shortened mark; at most one lengthened gap per burst.
3. **Drift test:** 2-hour session; `off` corrections must stay ≤ 1 ms per step and never occur inside a character.
4. **Safety tests:** pull the Keyer's WiFi mid-mark → TX-out must drop within `keepalive_timeout`. Send a forged packet with a bad MAC → ignored, watchdog not affected. Hold key-down > `max_keydown_ms` → key up at the limit.
5. **Real Internet:** Keyer on a mobile hotspot, Rig unit behind a home router with port forward. Log STATS for an hour; compare with the netem results.
6. **Straight key / bug:** repeat tests 1 and 2(b) with `source` = 1, keyed by (a) a real straight key with deliberately sloppy, tightly-spaced sending at ~15 WPM, (b) a bug at ~25 WPM, and (c) a signal generator producing 20 ms marks with 2 ms contact chatter on every edge. Acceptance: no chatter edge appears on the wire; every accepted mark reproduced within the §4.3 targets relative to the *raw* contact edge; `dit_est` converges within one word; no `off`/`D` adjustment ever occurs inside a character (check against the logic-analyser trace of the raw key).
7. **Tune:** hold a straight key down for 12 s → TX-out drops at 10 s, Keyer display warned at 5 s, next key-down keys normally. Switch keyer mode iambic → straight key mid-session → `STATS` reports the new `max_keydown_ms` within 2 s.

---

## 14. Relationship to MOPP

M32KIP and MOPP are independent: different ports, different packet formats, different purposes. A future extension could send the Keyer unit's decoded characters as MOPP-style text in a side channel for display on the Rig unit or as a fallback; this is explicitly out of scope for v1. Both can coexist on a unit, but not simultaneously in the same mode.

---

## 15. Open questions

1. **Straight-key operators and "speed":** with `wpm` = 0 the Keyer display can show the Rig-side `dit_est` as an approximate speed, but should it? It may be a useful self-check for hand-keying operators or an annoyance. Make it a display option.
2. **Rig-side sidetone / monitoring:** should the Rig unit offer an optional local sidetone for a second operator at the remote site? Cheap to add on the Pocket, irrelevant on a reduced board.
3. **Hostname vs. IP:** DNS resolution on the Keyer side is straightforward but must not block the keyer task; resolve before entering the mode.
4. **Relay/rendezvous server:** would spare users the port forward. Adds a hop (more delay, more jitter) and an operator. Defer.
5. **Second keyer "sitting in":** a second Keyer unit joining as observer (receives STATS and edge stream, keys nothing) for training/monitoring — possible with the same packet format, but needs a multicast/fan-out design on the Rig side. Defer.
6. **IPv6:** free with lwIP if enabled; test.

---

## Appendix A — Worked example of the timeline shift

Keyer sends a dit at 25 WPM (48 ms) followed by a 48 ms gap and a dah (144 ms). `D` = 120 ms. The key-down of the dah arrives 30 ms late because of a jitter burst.

- Queue before the late packet: key-up of dit scheduled correctly; nothing else.
- Dah key-down arrives with `L = 30 ms` → `D := 120 + 30 + 10 = 160 ms`. Both the dah key-down and (when it arrives) the dah key-up are scheduled with the new `D`.
- Result on the air: dit 48 ms ✓, gap 88 ms (lengthened by 40 ms), dah 144 ms ✓. Subsequent elements are all exact, just 40 ms later than before. Nothing was clipped, and `D` will decay back toward its target over the following minutes if the link stays quiet.

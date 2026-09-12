# M32KIP Phase 0 — timing measurements

Status: **classic measured 2026-09-12 (three runs), verdicts below; Pocket runs pending.**

## Procedure

1. Flash the spike image (built with `PLATFORMIO_BUILD_FLAGS="-D KIP_SPIKE=1"`; firmware only,
   never `uploadfs`). **Disconnect any transmitter from the key output** — the key line toggles
   continuously. Battery or USB power, either is fine.
2. The device boots into the spike, connects to the stored WiFi network, and shows its IP on the
   status line (`KIP 192.168.x.y`). No WiFi credentials stored → it shows "Not connected" and reboots.
3. **Idle reading:** wait 60 s, note the four lines.
4. **Loaded reading:** from the Mac, on the same network:
   ```
   python3 Software/tests/kip/spike_load.py 192.168.x.y --pps 200 --seconds 120
   ```
   Single-click the encoder to reset the counters as soon as the load starts, wait for the script
   to finish, note the lines again, and keep the script's RTT/loss summary.
5. **Touch reading (optional):** during the loaded run, tap the touch paddles and the buttons — every
   interactive mode does these reads, and the spike does them too, but real fingers change the
   touch-read timing.
6. Long-press the encoder to reboot. Repeat for the other variant.

## Reading the display

All times in µs; fields saturate at 5 digits. Fitted to the OLED's 14 columns.

| Line | Meaning |
|---|---|
| `ISR <max>/<avg>` | Lateness of the one-shot hardware-timer ISR: maximum and average since reset. This is the Rig emitter's timing error (spec §4.3, target ≤ 1 ms after D2). A trailing `!` means an alarm fired *early*, which would be a bug in the spike's compare. |
| `>1 <n> L>2 <n>` | ISR firings more than 1 ms late; `loop()` gaps longer than 2 ms. |
| `LP<max> D<max>` | Longest gap between `loop()` passes **excluding** passes that redrew the display, and the longest redraw itself. The first is the straight-key capture jitter on the polled path when the mode avoids redrawing while the key is down (D2); the second is what a redraw costs if it collides with an edge. |
| `rx N tx N cN` | Echo packets received / sent, and the core the ISR ran on (`c1` expected). Pocket only (the OLED has three lines). |

## Results

### M32 Pocket (`pocketwroom`)

| Condition | ISR max µs | ISR avg µs | >200 µs | >1 ms | LOOP max µs | LOOP >2 ms | rx / tx | core |
|---|---|---|---|---|---|---|---|---|
| idle 60 s | | | | | | | | |
| 200 pps 120 s | | | | | | | | |
| + touching paddles | | | | | | | | |

spike_load.py summary: *(paste)*

### Classic M32 (`heltec_wifi_lora_32_V2`)

Measured 2026-09-12, device at 192.168.1.237 on the shack WiFi, Mac on the same /24.

**Run 1** (first spike layout, WiFi modem sleep still ON, readings cumulative from boot through
the 120 s load, ~4 min; the OLED truncated the wider first layout at 14 columns):

| ISR max | ISR avg | ISR > 200 µs | loop() max gap |
|---|---|---|---|
| **84 µs** | (truncated) | **0** | **37.5 ms** |

The ISR figure settles D1 for the classic: 84 µs worst case over four minutes of 200 pps
load is an order of magnitude inside the 1 ms target. The 37 ms loop gap is the I²C OLED
redraw (4 Hz, three lines) — run 2 separates it from the rest of the loop.

**Run 2** (WiFi modem sleep OFF, redraw timed separately):

| Condition | ISR max | ISR avg | >1 ms | loop >2 ms | LP (no-draw max) | D (redraw max) |
|---|---|---|---|---|---|---|
| boot → end of load, ~5 min | **64 µs** | **17 µs** | **0** | 6135 | **7.7 ms** | **37.2 ms** |

Reading: the Rig emitter is fine (D1 for the classic: **yes**). The polled Keyer path is
not at the 1 ms target on the classic: 7.7 ms worst case between loop passes even with no
redraw in the pass, and a redraw costs 37 ms on the I²C OLED. 6135 passes over 2 ms in five
minutes is far more than the ~1200 redraws, so most of them come from the touch reads and/or
the UDP send. Run 3 cycles those off to apportion it.

**Run 3** (load states cycling every 60 s: all on / touch off / touch + UDP send off; longest
no-redraw loop gap per state):

| ISR max/avg | D (redraw max) | loop >2 ms | LP all on | LP touch off | LP touch + send off |
|---|---|---|---|---|---|
| **96 / 20 µs** | 36.9 ms | 5140 | 7.58 ms | 6.99 ms | **0.24 ms** |

(200 pps load for 190 s; the 60 s states each ran at least once, cumulative since reflash.)

Apportioning: touch reads cost well under 1 ms (7.58 → 6.99 ms when they stop). The
**`AsyncUDP::writeTo()` call from the loop is the stall**: with it gone the loop never
paused longer than 0.24 ms outside a redraw. 5140 passes over 2 ms in ~120 s of send-on
states at 50 sends/s means nearly every send blocked the loop for 2–7 ms — `writeTo()`
hands the packet to the lwIP thread and waits, and under 200 pps of inbound echo traffic
that wait is long. (MOPP sends from the loop the same way today; at a few packets per
word it never mattered.)

spike_load.py, 200 pps × 120 s, 60-byte packets:

```
sent 24000, echoed 23974, lost 26 (0.11 %)
RTT ms: min 5.37  median 11.46  p90 65.14  p99 83.53  max 509.92
jitter (p99 - min): 78.16 ms
```

Run 2, same load, with `WiFi.setSleep(false)` in the spike (the Arduino core leaves modem
power save ON by default and the firmware never turns it off):

```
sent 24000, echoed 23981, lost 19 (0.08 %)
RTT ms: min 5.16  median 10.68  p90 62.40  p99 81.75  max 507.44
```

Identical profile, so the tail is **not** the device's power save. Separating the hops
(all from the Mac, which turned out to be on **WiFi** itself, `en0`):

| Probe | min | avg / median | p90 | max |
|---|---|---|---|---|
| ping Mac → gateway (Mac's own WiFi hop only), 200 × 0.1 s | 3.1 ms | 8.3 ms | — | 86 ms |
| ping Mac → device (lwIP answers, no AsyncUDP), 200 × 0.1 s | 4.6 ms | 13.6 ms | — | 85 ms |
| UDP echo Mac → device at a realistic 25 pps, 40 s | 5.4 ms | 8.8 ms | 36 ms | 83 ms |

The ~80 ms tail is already present on the Mac's own hop to the access point; the device's
hop adds ~5 ms at the median and nothing to the tail, and the AsyncUDP echo path costs no
more than lwIP's ping reply. The 500 ms outliers appear only at 200 pps and are most likely
the Mac's WiFi under that packet rate. **This LAN, probed from a WiFi-connected Mac, says
little about the device; a wired probe host is needed for a real jitter baseline.** Keep
`WiFi.setSleep(false)` in the KIP modes regardless — it is the documented cause of exactly
this kind of tail on the ESP32 side, and costs nothing here.

## Verdicts (classic; Pocket runs still to do)

- **D1 — Rig side on the classic: YES.** Hardware-timer ISR on core 1, under 200 pps of WiFi
  traffic for minutes: 64–96 µs worst case, ~20 µs average, never over 1 ms, never early. Ten
  times inside the relaxed target and inside the original 200 µs one as well.
- **D2 — polled straight-key capture: viable, with two conditions on the Keyer mode.**
  The polled loop itself is good to 0.24 ms once nothing blocks it. What blocks it:
  1. **Sending from the loop.** `writeTo()` stalls the loop 2–7 ms per packet under load. The
     Keyer must send from a dedicated task fed by a queue (spec §10 already says so); the
     loop only timestamps and enqueues. Non-negotiable after this measurement.
  2. **Display redraws.** 37 ms per redraw on the I²C OLED. While the key is down (and for
     the polled path, at any time an edge may arrive — i.e. always, for a straight key) the
     Keyer mode must not redraw synchronously. Options for Phase 3: hold all redraws while
     the key line is closed and for one dit after it opens (the classic modes print a decoded
     character only after the inter-character gap anyway, which is exactly the safe window);
     or move display work to a low-priority task. The Pocket's SPI TFT will be faster per
     redraw but scrolling a line is not free — measure there before choosing.
  Touch reads may stay on (< 1 ms).
- **Network probe:** inconclusive about the LAN, conclusive about the probe host — see run 2.
  `WiFi.setSleep(false)` stays in the KIP modes on principle.
- **Spec follow-ups (fold into Draft 0.3):** §10 "the keying path itself never blocks on the
  socket" is now backed by a number; add the redraw rule to §10; §12.1's "ship the Rig side for
  the Pocket only if the classic cannot meet the target" is moot — the classic meets it.

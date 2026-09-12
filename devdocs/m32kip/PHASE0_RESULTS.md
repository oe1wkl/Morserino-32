# M32KIP Phase 0 — timing measurements

Status: **instruments built, measurements pending.** Fill in the tables below from the
device display; the D1 verdict (Rig side on the classic?) follows from them.

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

| Line | Meaning |
|---|---|
| `ISR mx NNNNN av NNNNN` | Lateness of the one-shot hardware-timer ISR, µs: maximum and average since reset. This is the Rig emitter's timing error (spec §4.3, target ≤ 1 ms after D2). |
| `>.2ms N >1 N` | Count of ISR firings more than 200 µs and more than 1 ms late. A trailing `!` means an alarm fired *early*, which would be a bug in the spike's compare. |
| `LOOP mx NNNNN >2 N` | Gap between successive `loop()` passes, µs: maximum, and count of gaps over 2 ms. This is the straight-key capture jitter on the polled path (D2, target ≤ 1 ms typical; the 4 Hz display redraw will show as periodic outliers — what matters is how large they are). |
| `rx N tx N cN` | Echo packets received / sent, and the core the ISR ran on (`c1` expected; `c0` would mean the timer landed on the WiFi core). Pocket only (the OLED has three lines). |

## Results

### M32 Pocket (`pocketwroom`)

| Condition | ISR max µs | ISR avg µs | >200 µs | >1 ms | LOOP max µs | LOOP >2 ms | rx / tx | core |
|---|---|---|---|---|---|---|---|---|
| idle 60 s | | | | | | | | |
| 200 pps 120 s | | | | | | | | |
| + touching paddles | | | | | | | | |

spike_load.py summary: *(paste)*

### Classic M32 (`heltec_wifi_lora_32_V2`)

| Condition | ISR max µs | ISR avg µs | >200 µs | >1 ms | LOOP max µs | LOOP >2 ms |
|---|---|---|---|---|---|---|
| idle 60 s | | | | | | |
| 200 pps 120 s | | | | | | |
| + touching paddles | | | | | | |

spike_load.py summary: *(paste)*

## Verdicts

- **D1 (Rig side on the classic):** *pending* — yes if ISR `>1 ms` stays at 0 under load.
- **D2 (polled straight-key capture):** *pending* — the LOOP maximum is the worst-case capture
  error; if the display redraw dominates it, the Keyer mode can rate-limit or skip redraws while
  the key is down.

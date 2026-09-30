# BLE Serial — developer notes and bench scripts

Design and history: [`PLAN.md`](PLAN.md) (implementation plan, test matrix §8),
[`DESIGN.md`](DESIGN.md) (as-built notes and bench results),
[`ACCESS_CONTROL.md`](ACCESS_CONTROL.md) (on-device consent for BLE clients).

## Bench scripts

All need Python with `bleak` and `pyserial`. **Run them from macOS Terminal.**
A process started from a sandboxed tool shell whose host app has no Bluetooth
permission is killed by macOS on first CoreBluetooth use — exit code 134, no
output at all. Terminal asks for the permission once.

Since access control, a new BLE client must be allowed **on the device**
(FN = allow, encoder click = deny, 20 s = deny), only at the top menu. The
scripts say out loud when to press FN. A client returning within 60 s of a
grant is admitted without asking (decision D3), so not every cue needs a press.

| Script | What it does | Hands |
|---|---|---|
| `round3_bench.py` | Full regression: fail-closed T0, transport isolation, BLE basics (multi-chunk menus, `GET capabilities`, paginated `GET configs/details`, keyer start, echo, batched write, DEVICE BUSY), Bluetooth Use switching and snapshot recall, congestion, WiFi suspend ×5 | FN ~10×, none during T0 |
| `consent_trials.py` | Unanswered handshakes must be declined — N trials, all but the last from a true hard reset | none (hands off) |
| `wifi_cycles.py` | Repeated WiFi suspend/resume with notice timing; the leak/stall reproducer | FN when outside the grace window |
| `ble_connect_probe.py` | Connect + notify to every NUS device by address; tells a stuck device from a stuck Mac | none |
| `usb_m32.py`, `ble_bench_common.py` | Shared clients and helpers | — |
| `ble_m32_test.py`, `round2_regression.py` | **Stale** (pre–access control), kept for history | — |

Gotchas that each cost time on 2026-09-30:

- **Opening the classic's USB port does not reliably reset it.** Use
  `hard_reset()` (esptool, read-only `chip_id`) when a test needs a fresh boot.
- **`PUT menu/start` is silently ignored while the device is not yet back at
  the menu** (TODO B3). Settle before each start or results look like lost
  notices.
- Both variants advertise as `Morserino-32`; tell them apart by address.

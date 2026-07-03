# BLE Serial — design notes

*Feature: the M32 serial protocol over BLE (Nordic UART Service), branch
`ble-serial-protocol`, 2026-07. The reviewed implementation plan (with the
full decision table D1–D19 and rationale) is [PLAN.md](PLAN.md); this file
records what an implementer or debugger needs beyond it: the contracts,
deliberate exemptions, measured results, and the one latent bug we found.*

## Architecture in five sentences

`MorseBleSerial` runs a Bluedroid NUS GATT server (`6E400001/2/3-B5A3-…`,
name `Morserino-32` in the scan response, no bonding, one central).
Emission and delivery are split: code that generates protocol output gates on
`protocolActive()` (= `m32protocol || bleProtocol`), and the `m32out` tee
(`M32ProtocolOut.h/.cpp`) delivers per transport — USB iff `m32protocol`,
BLE iff `bleProtocol && linkUp()`. Inbound bytes go GATT `onWrite` →
SPSC ring → `bleSerialEvent()` (called from `serialEvent()`, so BLE is
serviced at every USB polling site), which dispatches **one completed line
per poll** through the same `serialDecode()` engine as USB. Each transport
has its own handshake: `PUT device/protocol/on|off` is intercepted per
transport (on BLE as a case-insensitive whole-line compare). WiFi bring-up
(all funnels: `setupWifi`, `setupESPNow`, the `_wifi_*` dispatch) suspends
BLE Serial; the top-menu backstop in `menu_()` restarts it.

## Contracts that must not rot

- **Single-writer ring discipline (PLAN D18).** `BleByteRing` indices have
  exactly one writing task each. The session reset never writes a
  producer-owned index: `onConnect` (Bluedroid task, serialized with
  `onWrite`) latches `connectMark = head`; `pump()` (loop task) sets
  `tail = mark`. `hardReset()` is legal only with the server down.
  Host tests: `test_blebytering.cpp` (14 cases —
  `clang++ -std=c++17 -Wall -Wextra -Werror -I"../../Software/src/Version 6 and newer" test_blebytering.cpp && ./a.out`).
- **Flow control (PLAN D17).** Two monotonic single-writer counters
  (`notifyCount` loop task / `confCount` BT task via the custom GATTS hook,
  filtered by `gatts_if`+`conn_id`); in-flight < 4, ≤ 2 chunks per pump pass;
  2 s watchdog resync; wrap clamp. `ESP_GATTS_CONF_EVT` fires on hand-off to
  the controller for notifications (ATT confirmation exists only for
  indications) — credits pace the queue, they do not confirm delivery.
  Residual loss risk: `BLECharacteristic::notify()` returns void and hides
  per-PDU errors; a torn multi-KB response is documented protocol behavior
  (client re-issues the GET).
- **Post-stop no-op contract (PLAN D19).** `stop()` sets `isRunning = false`
  first, then `BLEDevice::deinit(false)`, then synchronously clears
  `isConnected`, `bleProtocol`, pending flags and flow state. It never trusts
  `onDisconnect` to be delivered through deinit. After `stop()`, `readByte()`
  returns false and every other API is a defined no-op — that is what makes
  remote self-disable (`PUT config/<BLE Serial>/0` arriving over BLE, so
  `stop()` runs inside the dispatch chain) safe. Callers that want the TX
  ring delivered flush **before** stopping (`txFlush(300/500)`); `stop()`
  itself does not flush.
- **Echo never stalls keying.** `txEnqueueEcho` (the `SerialOutMorse` copy)
  drops immediately when the ring is full — no self-drain; a dit at 40 WpM
  is ~30 ms. Protocol replies (`txEnqueue`) self-drain for at most 20 ms,
  then drop and count `txDropped`.

## Deliberate raw-`Serial` exemptions

Do **not** "fix" these into the `m32out` tee:

- `DEBUG()` (`m32_v6.ino`) — unframed text; interleaving it into
  notifications would corrupt a client's JSON framing.
- The RadioLib init prints in `setup()` (`m32_v6.ino`, "[SX12xx]
  Initializing…") — fire pre-handshake, would be dropped anyway.
- `SerialOutMorse` echo **to USB** is unconditional `Serial.print` (inside
  `m32out.echo()`) exactly as before the feature — the "Serial Output"
  preference, not the protocol handshake, governs it.

## deinit(false) — and the latent HID re-init hang (note for Willi)

Verified in arduino-esp32 core 2.0.17 (`BLEDevice.cpp`): `deinit(true)`
releases BTDM controller memory irreversibly **and leaves the static
`initialized` flag true**; any later `BLEDevice::init()` is then a silent
no-op and the next `createServer()` blocks forever on a `portMAX_DELAY`
semaphore in `registerApp`.

Consequences:

1. `MorseBluetooth::stopBluetooth()` now calls `deinit(false)` when
   `CONFIG_BLE_SERIAL` is compiled in (flag-off builds keep `deinit(true)`),
   so BLE Serial can start after the keyboard stops. Trade-off: once BLE has
   run, the BT controller BSS (~30 KB) stays reserved for the rest of the
   boot — the same heap situation as today's boot→WiFi-upload path, but less
   free heap than the old keyer-with-BT→menu→WiFi path where `deinit(true)`
   donated its memory. Verify WiFi upload/OTA on the classic V2 **after** a
   BLE session (hardware checklist item 13).
2. **Latent pre-existing bug (unrelated to this feature, un-masked by it):**
   with the flag off, every keyer→menu→keyer cycle with the BT keyboard
   re-runs `initializeBluetooth()` after a `deinit(true)`. The fresh
   `bluetoothTask` then hangs forever in `registerApp` (contained only
   because it is a dedicated task) and leaks its 10 KB stack per cycle. The
   `//ESP.restart(); // not needed anymore` comment in `menu_()` masks this
   history. The `deinit(false)` change likely fixes it on flag-on builds —
   verify on hardware and record the result here. On the S3 (`pocketwroom`),
   also record what `esp_bt_controller_mem_release` actually returned in the
   old `deinit(true)` path (BLE-only silicon may have failed the release all
   along, i.e. poisoned-but-not-released).

## Memory / builds (measured 2026-07-02, espressif32@6.11.0)

| Build | RAM | Flash |
|---|---|---|
| V2 baseline (master) | 29.6 % (96,856 B) | 60.5 % (2,022,669 B) |
| V2 with BLE Serial | 31.3 % (102,680 B) | 60.7 % (2,029,037 B) |
| pocketwroom baseline | 37.4 % (122,648 B) | 67.5 % (2,256,993 B) |
| pocketwroom with BLE Serial | 39.2 % (128,464 B) | 67.7 % (2,263,501 B) |
| pocketwroom flag-off regression | 37.4 % (+8 B: tee object) | 67.5 % (+236 B) |

Static cost ≈ 5.7 KB (1 KB RX ring + 4 KB TX ring + 512 B staging + strings).
Bluedroid heap (~45–60 KB when running) is the same load the HID keyboard
already imposes. Known library wart: the `BLEServer`/service/characteristic
objects are leaked by the library across `deinit(false)`/`init()` cycles (our
callback objects are static); measure the per-cycle free-heap delta in
hardware checklist item 10 — the suspend/restart cycle runs many times per
battery charge.

## Hardware test checklist

The full 15-step on-hardware matrix is PLAN.md §8. Run
[`ble_m32_test.py`](ble_m32_test.py) (`pip install bleak`) from a Mac for
steps 1–8 (scripted) and `--repl` for the rest. No hardware was attached
during implementation, so **all on-hardware steps are still open**; the
host-testable logic (ring, reset, flow policy) and both-variant + flag-off
builds are done.

## TODO handed to Willi

- Regenerate `Documentation/Protocol Description/M32 Protocol.pdf` from the
  updated `.md` (no build script exists for it).
- Run the hardware checklist (above) on classic V2 + Pocket; record the
  heap-delta, boot-latency and `mem_release` observations in this file.

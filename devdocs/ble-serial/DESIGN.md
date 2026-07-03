# BLE Serial — design notes

*Feature: the M32 serial protocol over BLE (Nordic UART Service), branch
`ble-serial-protocol`, 2026-07. The reviewed implementation plan (with the
full decision table D1–D19 and rationale) is [PLAN.md](PLAN.md); this file
records what an implementer or debugger needs beyond it: the contracts,
deliberate exemptions, measured results, and the one latent bug we found.*

## Selector rework (PR #194 feedback, 2026-07-03)

Willi asked for **one** Bluetooth selector instead of two competing
preferences (keyboard output vs. BLE Serial). Consequences:

- `posBleSerial` was removed entirely: the rule-9 triple (enum entry,
  `pliste[]`, `prefName[]`), the `BLESER` macro in the option arrays, the
  snapshot exemption (read, write, and JSON sides), and the
  keyboard-exclusion predicate with its one-time notice splash
  ("BT Kbd off"/"BLE Serial on").
- BLE serial is now **option 5 of the "Bluetooth Use" selector**
  (`posBluetoothOut == 5`, `BLT_USE_SERIAL_PROT` in `morsedefs.h`). Options
  0-4 keep their historic order and meanings, so stored settings stay
  compatible; option 5 exists only in `CONFIG_BLE_SERIAL` builds.
- The keyboard-vs-serial mutual exclusion is now **structural**: one selector
  value owns the BLE stack, so no exclusion predicate or notice is needed.
- The setting is stored in snapshots like any normal preference (the old
  snapshot exemption is obsolete).
- Display-width caveat: the mapped value `"BLT Serial Prot."` is 16 chars vs
  the OLED's 14-char line budget — flagged to Willi in the PR.

References to `posBleSerial`, the OFF/ON toggle, the snapshot exemption, or
the "BT Kbd off" splash in PLAN.md and in the sections below are historical;
this section supersedes them.

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

## Adversarial implementation review (2026-07-02) — outcome

A 5-dimension multi-agent review (concurrency/BLE-stack, protocol semantics,
CLAUDE.md conformance, plan fidelity, regression risk) with two independent
verification votes per finding confirmed 12 findings; all were fixed in the
same session (commit "review fixes"):

- **critical:** disconnect replayed the whole session's RX bytes as executed
  commands (mark latched only at connect) → mark is now re-latched in
  `onDisconnect`; regression host test `testDisconnectRelatchNoReplay`.
- mtuPayload now resets to 20 per connection (stale large MTU = silent ATT
  truncation); `init()` verifies controller+Bluedroid status after
  `BLEDevice::init` (whose failures are swallowed by the library) before
  `createServer`, with a sticky-failure latch so polling sites don't retry
  every pass; RX overflow re-designed (ring poisons itself until drained —
  no spliced line can ever dispatch, intact pre-drop lines still execute);
  `txEnqueue` got a back-off latch (a congestion episode costs the loop task
  at most one 20 ms wait total, not 20 ms per byte); `bleProtocol` also
  cleared in the pump session reset (handshake/disconnect race); overlong
  BLE lines now report `BLE LINE TOO LONG`; the top-menu restart backstop
  also runs in the menu wait loop (a `_wifi_*` function returns there, not
  through `menu_()`'s top — without this BLE never resumed, and a pref
  toggled ON from the top menu never started); `confirmDelete()`'s two
  `jsonConfigShort` calls gated on `protocolActive()` (see below); heap
  monitor `#ifdef` widened to `CONFIG_BLE_SERIAL`.

**Known residual risks (accepted, documented):**

- `BLECharacteristic::notify()` runs on the loop task while the Bluedroid
  task mutates the server's connection map on disconnect; the library has no
  locking. Window is minimized (fresh `linkUp()` check each drain iteration)
  but cannot be closed from outside the library. Panel severity: minor.
- The library leaks the `BLEServer`/`BLEService`/`BLECharacteristic`/
  `BLE2902` objects on every `deinit(false)` → `init()` cycle (our own
  callback objects are static). Roughly a few hundred bytes to ~1 KB per
  suspend/restart cycle; measure in hardware checklist item 10.
- Flag-on behavior delta vs master: `confirmDelete()` (delete-snapshot
  dialog) used to emit `{"config":…}` JSON to a **never-handshaken** USB
  terminal (the only two ungated emission sites in the codebase). These are
  now gated on `protocolActive()` like every sibling site — handshaken
  clients see no difference. Judged a pre-existing inconsistency, aligned
  rather than preserved.
- `MorseBluetooth::stopBluetooth()`'s `deinit(false)` change is compile-flag
  gated, so flag-on builds change the BT-keyboard stop path even for users
  who never enable BLE Serial (likely fixing the latent hang below, but
  less free heap for a subsequent WiFi session than the old `deinit(true)`
  path). **Hardware verification of checklist items 10–13 is a hard
  pre-merge gate.**

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

## First hardware contact (2026-07-03, M32 Pocket) — four fixes + findings

Steps 1–8 of the PLAN §8 matrix **pass on a real M32 Pocket** (scripted run:
handshake + early-write, chunked `GET menus` reassembly, remote keyer start,
`cw/play` audible, batched write, mixed-case `Put device/protocol/off`
isolation + un-handshaken silence, re-handshake, 2.9 KB flow-control soak,
disconnect → re-advertise → reconnect cycles). Four defects were found and
fixed on the way — none reachable without radio hardware:

1. **Library scan-response overflow (no advertising at all).**
   `BLEAdvertising::start()` with `setScanResponse(true)` memcpy's the whole
   ADV payload (128-bit NUS UUID included) into the scan response beside the
   name → 35 B > 31 B → `esp_ble_gap_config_adv_data` INVALID_ARG (visible
   only at CORE_DEBUG_LEVEL ≥1, which QuickDebug precludes) → `start()`
   returns **before** `esp_ble_gap_start_advertising`. Fix: direct GAP calls
   with explicit ADV payload (flags + UUID) and raw name-only scan response.
2. **Keyboard-bond connection theft (invisible while "working").** Hosts
   bonded to the BT keyboard ("Morserino32 Keyboard", e.g. for VBand)
   auto-reconnect to the chip's **public address** the instant anything
   advertises from it, and hold the link — `AUTH_CMPL` fires, advertising
   stops, no scanner ever sees the device. Fix: BLE Serial advertises from a
   **static random address** (own identity, derived from the BT MAC with the
   static-random bits set and one byte flipped); keyboard bonds keep working
   for the keyboard, and never capture BLE Serial. The advertising *restart*
   in `pump()` must use the same path — the library default would silently
   revert to the public address on the first reconnect cycle.
3. **HID-owned stack latched a permanent init failure.** Enabling BLE Serial
   while the keyboard runs (keyer mode) made `init()`'s defensive pre-check
   latch `initFailedSticky` — and nothing could clear it while not running.
   Fixes: HID-owned stack is a transient refusal (no sticky, no splash;
   backstop retries after `menu_()` stops the keyboard), and
   `stopWithNotice()` now calls `stop()` even when not running so a pref
   off→on cycle always resets module state.
4. Test-script bugs of its own: bare `GET control` is an INVALID ARGUMENT by
   protocol (use `control/speed`); the soak now uses `GET configs`.

Observed, for the app side: the character echo arrives **lowercase** (the
"Output Case" preference governs it) — the iOS EchoTracker must compare
case-insensitively; and the app must **cancel its pending connect** when the
picker closes, or iOS completes it silently later (zombie connection the app
doesn't show).

## Hardware test checklist — remaining

The rest of the PLAN §8 matrix is still open: WiFi suspend/resume ×5 with
per-cycle heap deltas (both boards), mid-game multiplayer drop, suspended-
session auto-sleep + remote self-disable, BT-keyboard interplay on the
classic V2, ≥35 WpM stalled-client CW-timing soak, WiFi-upload-after-BLE on
V2, snapshots, and the classic-V2 run of the scripted steps. Run
[`ble_m32_test.py`](ble_m32_test.py) (`pip install bleak`) scripted or
`--repl`.

## TODO handed to Willi

- Regenerate `Documentation/Protocol Description/M32 Protocol.pdf` from the
  updated `.md` (no build script exists for it).
- Run the hardware checklist (above) on classic V2 + Pocket; record the
  heap-delta, boot-latency and `mem_release` observations in this file.

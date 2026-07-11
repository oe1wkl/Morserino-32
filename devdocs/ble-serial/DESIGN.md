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
- Display-width caveat (resolved 2026-07-10): the mapped value was
  `"BLT Serial Prot."`, 16 chars vs the OLED's 14-char line budget; per
  Willi's review it is now **"BLE Serial"**, and `displayValueLine`'s
  padding arithmetic is guarded against unsigned wrap for any future
  overlong value.

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
- **Nothing on the TX side ever waits (revised 2026-07-10, review round 2).**
  `txEnqueueEcho` (the `SerialOutMorse` copy) drops immediately when the ring
  is full; a dit at 40 WpM is ~30 ms. Protocol replies (`txEnqueue`) used to
  self-drain for up to 20 ms, but that deadline sat inside the CW keying path
  (speed/volume `jsonControl` from keyer mode) — twice the ~10 ms delays that
  have caused audible, field-reported CW-timing bugs before. `txEnqueue` now
  also never drains: it checks the `txBackoff` latch FIRST (so fragments of
  later objects cannot splice into a torn one), enqueues what fits, and on
  overflow enters a backoff episode that drops everything until `pump()` has
  fully drained the ring. `pump()` — called from every polling site — is the
  only drainer. JSON additionally reaches the tee in ~256-byte chunks
  (`MorseJSON::jsonSend`), not one `write()` per byte.
- **Session-scoped messages are single-transport (added 2026-07-10).**
  Handshake replies, `end m32protocol`/"Goodbye!", the BLE
  `LINE TOO LONG`/`RX OVERFLOW` errors and the suspend/stop notices concern
  exactly one transport; their emission is wrapped in an `M32TargetScope`
  (`M32ProtocolOut.h`) narrowing the tee to `UsbOnly`/`BleOnly`, so a
  handshaken client never sees the *other* session end or errors it never
  provoked. A `BleOnly` message is delivered whenever the link is up (even
  pre-handshake — it answers that client's own input). Both `protocol/on`
  and `protocol/off` are intercepted per transport in `bleSerialEvent`, so
  `m32Put`'s `device/protocol` branch is USB-only and answers `UsbOnly`.

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
- `MorseBluetooth::stopBluetooth()` uses `deinit(false)` **unconditionally
  since master 06a3502/c61ac37** (2026-07-09, verified on hardware by Willi:
  the keyboard was one-shot per boot under `deinit(true)`, exactly the latent
  hang described below) — the flag-gated hunk this branch carried was merged
  away. The BT controller BSS stays reserved after any BT session; Willi
  treats the WiFi-upload/OTA-after-BT check on the classic V2 (checklist
  item 13) as a hard merge gate. **Hardware verification of checklist items
  10–13 is a hard pre-merge gate.**

## Review round 2 (Willi's PR #194 review, fixed 2026-07-10)

Willi's in-depth review confirmed seven issues; all are fixed on the branch:

1. **Stale HID keying after a keyboard→BLE-Serial selector switch.** Master
   now clears `isBleConnected` in `stopBluetooth()`; additionally every HID
   bitmask gate goes through the new `MorseBluetooth::keyboardMode()`, which
   returns 0 for selector value 5 — 5 = 0b101 would otherwise pass both the
   `& 0x1` (key-as-CTRL) and `& 0x6` (decoded output) tests. New HID gates
   must use `keyboardMode()`, never the raw `pliste` value.
2. **`PUT snapshot/recall` bypassed the selector change-switch** (it recalls
   without `writePreferences`): the recall path in `m32Put` now runs the same
   stop-BLE-Serial side effect when the recalled selector is not 5.
3. + 4. **`txEnqueue` backoff order + synchronous drain** — see the revised
   TX-side contract above (never waits, backoff checked first).
5. **Session-control leakage across transports** — see the new
   single-transport contract above (`M32TargetScope`).
6. **`sessionResetPending` lost-update**: the pump's reset block now clears
   the flag FIRST (like `takeLineReset`), so an edge signalled by the
   Bluedroid task mid-block triggers another reset instead of being lost.
7. **Downgrade hazard**: `readPreferences` maps an out-of-range
   `posBluetoothOut` (e.g. stored 5 read by a keyboard-only build) to 0 (Off)
   instead of letting the generic constrain clamp it to 4 = Generic Kbd,
   which would type decoded CW at any bonded host.

Plus the requested non-blocking items: selector value renamed to
**"BLE Serial"** + `displayValueLine` padding wrap guard; on-device splash
("BLE Ser. susp.") when WiFi suspends BLE Serial per UX_CONVENTIONS §10.1;
USB `PUT device/protocol/<value>` compares case-insensitively (parity with
BLE); scan response derived from `BLE_NAME` with a `static_assert`; chunked
JSON serialization (`MorseJSON::jsonSend`). The suggested **static `BLE2902`
was tried and REVERTED after hardware testing**: `BLEDescriptor::
executeCreate()` refuses a descriptor whose handle is already set (private
`setHandle`, no reset path), so from the second `init()` on the TX
characteristic silently had no CCCD and every enable-notify failed with ATT
"attribute not found" — the per-init `new BLE2902()` (small documented leak,
like the library's other per-cycle objects) is load-bearing. Deliberately NOT done: heap-allocating the
rings/staging (~5.6 KB) in `init()` — static allocation is immune to the
fragmented-heap failure mode that `init()` already has to defend against
(the "NO MEM" path), and the RAM headroom on both variants absorbs it;
revisit only if a variant gets tight.

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
2. **Latent pre-existing bug — CONFIRMED AND FIXED ON MASTER (2026-07-09):**
   every keyer→menu→keyer cycle with the BT keyboard re-ran
   `initializeBluetooth()` after a `deinit(true)`; the fresh `bluetoothTask`
   hung forever in `registerApp` (contained only because it is a dedicated
   task) and leaked its 10 KB stack per cycle — the keyboard was one-shot per
   boot since the `ESP.restart()` after "Stop BT Kbd" was removed in March.
   Willi verified the flow on hardware and fixed it on master (06a3502 +
   c61ac37): `stopBluetooth()` uses `deinit(false)` unconditionally, clears
   `isBleConnected`, and a `btStopping` teardown flag skips `onDisconnect`'s
   5 s reconnect grace so mode exit stays fast. Still open for the S3
   (`pocketwroom`): what `esp_bt_controller_mem_release` actually returned in
   the old `deinit(true)` path (BLE-only silicon may have failed the release
   all along, i.e. poisoned-but-not-released).

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

## Second hardware run (2026-07-10, M32 Pocket, round-2 + V9.0-merged build)

Scripted PLAN §8 suite (steps 1–8) **passes**; a 27-check dual-transport
regression (USB `pyserial` + BLE `bleak` attached simultaneously) verified
the round-2 fixes on-device: session-control isolation in both directions
(fix 5, incl. re-ack and bogus-value intercepts), `PUT snapshot/recall`
change-switch (fix 2, store/recall/clear round-trip with selector
transitions), uppercase `protocol/OFF` on USB (fix d), and the congestion
policy (fixes 3+4): during an 8× `GET configs` flood into a full TX ring the
**USB round-trip stayed at ~40 ms** (no loop stall), the backoff DEBUG
arrived cleanly on raw serial, and the BLE session was usable again
immediately after the ring drained.

**WiFi suspend/resume ×5 (Pocket): PASS** — remotely executed
`Disp MAC Addr` with a handshaken USB session; all 5 cycles suspended,
re-advertised and reconnected + re-handshaked. The BLE-side suspend notice
arrived in 4/5 cycles (best-effort by design — it races the link teardown).

**Measured per-cycle heap loss: ~3.8 KB** (init-to-init free-heap deltas
over 5 suspend/resume cycles: 3896/3824/3848/3824/3816 B; free heap at
BLE-init fell 77.6 KB → 58.4 KB across the run). This is the library's
leaked server/service/characteristic/descriptor objects per
`deinit(false)`→`init()` cycle — noticeably more than the "few hundred
bytes to ~1 KB" estimated earlier in this file. ~15–20 WiFi round-trips in
one power-on could exhaust enough heap that `init()` starts refusing
(visibly, via the "NO MEM" defensive path — no crash). Acceptable for
typical sessions; a future mitigation would need library surgery (reusing
the server object across cycles) — out of scope here.

Two additional defects found and fixed during this run:

1. **Static `BLE2902` broke every re-init** (see the round-2 section above
   — reverted to per-init `new`).
2. **`GET menus` silently truncated at ~34 entries on TFT builds**
   (`jsonMenuList`'s fixed 3072-byte pool vs `menuN = 43` with the games;
   ArduinoJson drops adds on a full pool). The whole WiFi Functions block
   was missing from the protocol menu list — pre-existing on master for
   every game-enabled build, found because the suspend/resume test could
   not find `Disp MAC Addr` remotely. Pool is now sized from `menuN`.

Client-side observation (for the protocol doc / app authors): a response
tail dropped under backoff leaves an **unbalanced `{` prefix** in the BLE
stream — a client framing purely by brace-counting cannot resync from the
stream alone; it needs a timeout-based resync or a reconnect (the session
reset clears everything). `ble_m32_test.py`'s REPL and the regression
harness now do timeout-resync.

## Hardware test checklist — remaining

Still open from the PLAN §8 matrix: the **classic V2** run of everything
above (incl. WiFi-upload-after-BT with free-heap deltas — Willi's bench),
mid-game multiplayer drop, suspended-session auto-sleep + remote
self-disable (auto-sleep with an *un*-handshaken BLE link was incidentally
confirmed — the bench device slept between test sessions), and the
≥35 WpM stalled-client CW-timing soak (needs ears + paddle: D1's 40 ms loop
latency is the proxy, not the proof). Run
[`ble_m32_test.py`](ble_m32_test.py) (`pip install bleak`) scripted or
`--repl`; the dual-transport regression above is
[`round2_regression.py`](round2_regression.py) (`pip install bleak pyserial`,
USB port via `M32_PORT`, defaults to `/dev/cu.usbmodem101`) — it needs the
device at the top menu with the selector on *BLE Serial*, and it
stores/clears a snapshot in the first free slot during section C.

## TODO handed to Willi

- Regenerate `Documentation/Protocol Description/M32 Protocol.pdf` from the
  updated `.md` (no build script exists for it).
- Run the hardware checklist (above) on classic V2 + Pocket; record the
  heap-delta, boot-latency and `mem_release` observations in this file.

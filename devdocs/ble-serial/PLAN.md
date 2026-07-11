# Implementation Plan — M32 Serial Protocol over BLE ("BLE Serial") — rev. 4 (final)

**Branch:** `ble-serial-protocol` off `master` (major feature ⇒ dedicated branch per CLAUDE.md §1).

> **Review status.** Rev. 3 went through three adversarial review rounds (maintainer / hardware / architect).
> Hardware: approved. Maintainer + architect each raised exactly one remaining blocker — the **same defect**:
> the §3 `bleSerialEvent` pseudocode gated the `protocol/off` interception on a case-sensitive
> `startsWith("PUT ")||startsWith("put ")` prefix, so a mixed-case verb (`Put device/protocol/off`) fell
> through to `serialDecode` → `m32Put`, killing the **USB** session and leaving `bleProtocol` stuck true
> (zombie sleep-suppression). Both reviewers verified everything else in the plan as accurate.
> **Adjudicated fix (rev. 4):** replace both handshake probes with allocation-free
> `String::equalsIgnoreCase` full-line compares — the interception is now genuinely case-insensitive for
> both `protocol/on` and `protocol/off` (D7's documented contract now holds), no String copies are built on
> the command path at all, and §8 step 8 gains the mixed-case `Put device/protocol/off` regression case.
> The architect's deeper alternative (origin-token through `m32Put`) was considered and declined: it touches
> the verified USB handler for no additional safety once the compare is case-insensitive, and the tee (D5)
> already deliberately avoids origin-routing.

## Decisions

| # | Decision | Rationale (one line) |
|---|----------|----------------------|
| D1 | New compile flag `CONFIG_BLE_SERIAL=1`, set per-env in `platformio.ini` for both canonical variants; comment noting that adding it to `common.M32_Pocket_build_flags` propagates to every pocket-derived env (intended, same as `CONFIG_BLUETOOTH_KEYBOARD`) | CLAUDE.md §1 requires a named config flag; feature fits both boards (Bluedroid BLE already compiles everywhere) |
| D2 | Transport = Nordic UART Service (NUS) UUIDs, write RX + notify TX; device name in the **scan response** (`setScanResponse(true)`) since flags (3 B) + 128-bit UUID (18 B) leave no room for "Morserino-32" in the 31-byte ADV PDU | De-facto BLE-UART standard; works out of the box with CoreBluetooth, bleak, nRF Connect — and the name stays visible in scanners |
| D3 | New module `MorseBleSerial.cpp/.h`, own BLE server, **not** co-hosted with the HID keyboard; mutual exclusion, BLE Serial wins — HID skip gated on `pliste[posBleSerial].value && MorseBleSerial::isRunning`, so a failed BLE init degrades to today's keyboard behavior | HID server has incompatible lifecycle (keyer-only, torn down in `menu_()`), bonded-keyboard pairing UX, `delay(5000)` in its disconnect callback (MorseBluetooth.cpp:129-133) |
| D4 | No bonding/pairing (Just Works, no `BLESecurity`) | Same trust model as plugging in USB; bonding is what makes the HID pairing UX painful on iOS |
| D5 | Output = tee (`M32Tee m32out : Print`) with **per-transport delivery gates**: Serial side delivers iff `m32protocol`, BLE side iff `bleProtocol && linkUp()`; echo channel (`m32out.echo()`: USB always, per today's `posSerialOut` semantics; BLE copy iff handshaken) uses **drop-immediately** enqueue (no self-drain — echo sits in the keying path), while protocol replies keep the bounded 20 ms self-drain | Emission and delivery split (D15); a never-handshaken USB terminal stays byte-silent; echo can never stretch CW element timing even at 35+ WpM |
| D6 | RX handoff = SPSC byte ring (1 KB), GATT `onWrite` copy-and-return; **at most one completed line dispatched per poll**, mirroring USB `serialEvent()` | Blessed pattern in this codebase (MorsePileup.cpp:344-360 `ftpRxRing`: producer writes only `ftpRxHead`, consumer only `ftpRxTail`); USB `serialEvent()` breaks at the first `\n` (m32_v6.ino:3769-3771) so `loop()` consumes dispatch flags between lines — BLE must match or batched commands behave differently per transport |
| D7 | Per-transport protocol state: `m32protocol` (USB) + new `volatile bool bleProtocol` (BLE); each transport sends its own `PUT device/protocol/on`; `PUT device/protocol/off` clears only the transport it arrives on (BLE-side interception, USB handler at m32_v6.ino:3977-3978 unchanged). Case asymmetry accepted and documented: BLE interception is case-insensitive, USB's `m32Put` thirdArg is case-sensitive, so `protocol/OFF` succeeds on BLE but errors on USB | Prevents flooding an un-handshaken client; a USB configurator sending `protocol/off` on exit cannot kill an active iOS session |
| D8 | BLE Serial runs at boot (if pref on) and across menus/modes, **suspended whenever any code path brings up the WiFi radio** — the `_wifi_*` menu functions (kept uniform, incl. display-only `_wifi_mac`), WiFi Trx, and mid-session ESPNow/WiFi starts (multiplayer entry, CW-output = WiFi). Auto-restart only on return to the top menu | WiFi+BLE were never co-resident (MorseMenu.cpp:243-271); all bring-up funnels through `setupWifi()` (:790) / `setupESPNow()` (:830) — incl. MorsePileup.cpp:953, MorseMorsel.cpp:1448, MorseGridNet.cpp:553, MorseMenu.cpp:517/519, 647/655 — so two hooks + the `_wifi_*` dispatch cover everything. LoRa (separate radio) coexists |
| D9 | User control = preferences toggle `posBleSerial` ("BLE Serial", OFF/ON), pliste path, snapshot-exempt like `posSerialOut` **on read, write, and JSON sides** | Read skip (MorsePreferences.cpp:1454-1456), write-loop skip at :1603 (only `posTimeOut` is skipped today), `jsonGetSnapshot` skip (MorseJSON.cpp:391-394); change-switch action gets an `if (morserino)` guard per the `posGoertzelBandwidth` precedent (:1614-1617) |
| D10 | MTU: request 517 via `BLEDevice::setMTU()`; track negotiated value in `onMtuChanged` in a `volatile` var read once per pump pass; TX chunk = `min(mtu − 3, 512)` from a static staging buffer (20 before negotiation) | APIs verified (BLEDevice.h:47, BLEServer.h:147); MTU-derived chunks give large-MTU clients 3× fewer notifications |
| D11 | No extra framing on BLE: identical byte stream to USB (LF-in, bare concatenated JSON out); client reassembles by brace matching | "Same protocol" is literal; adding BLE-only framing would fork the spec |
| D12 | `DEBUG()` output stays USB-only; likewise the boot-time RadioLib init prints (m32_v6.ino:774-776) stay raw `Serial` — listed in DESIGN.md beside the DEBUG exemption so a later cleanup doesn't "fix" them into the tee | Unframed debug lines interleaved into notifications would corrupt naive client JSON parsing; the RadioLib prints fire pre-handshake and would be dropped anyway |
| D13 | Protocol version stays **1.3** | Zero new commands/properties; new pref in `GET configs` is additive-legal per M32 Protocol.md:99-103 and :322 |
| D14 | Connection indication: none on screen in v1; protocol `message` objects to clients; UX_CONVENTIONS addition drafted (§4) | Matches BT-keyboard precedent (`isBleConnected` is invisible); no improvised UI |
| D15 | Gating architecture: emission gate = `protocolActive()` ≡ `m32protocol \|\| bleProtocol`, swept across all 40 `if (m32protocol)` emission/behavior sites (inventory §3); `serialDecode()`'s guard at m32_v6.ino:3806 becomes `if (!protocolActive()) return;`; per-transport *delivery* is the tee's job (D5) | `serialDecode()` hard-returns without `m32protocol` (:3806-3807) and every unsolicited object is gated on the global flag — without the sweep a BLE-only session gets no commands executed and no events |
| D16 | BLE stop = `BLEDevice::deinit(false)` by design, never `deinit(true)`; `MorseBluetooth::stopBluetooth()`'s `deinit(true)` (MorseBluetooth.cpp:214) becomes `deinit(false)` when `CONFIG_BLE_SERIAL` is compiled in; `MorseBleSerial::init()` is defensive and refuses (visible error, no `createServer`) if the stack is in an unusable state | Verified in core 2.0.17: `deinit(true)` releases BTDM memory irreversibly **and leaves the static `initialized` flag true** (BLEDevice.cpp:649-663); a later `init()` is a silent no-op (:334) and `createServer()` blocks forever in `registerApp` on a `portMAX_DELAY` semaphore. `deinit(false)` clears `initialized` and makes stop/start cycles real |
| D17 | Notify flow control via custom GATTS hook (`BLEDevice::setCustomGattsHandler`, BLEDevice.h:63, dispatched alongside normal handling at BLEDevice.cpp:126-128), **filtered by `gatts_if` + `conn_id`**, tracking `ESP_GATTS_CONGEST_EVT` and `ESP_GATTS_CONF_EVT`. Credits are **two monotonic single-writer counters** — `volatile uint16_t notifyCount` (written only by the loop task) and `confCount` (written only by the BT-task hook); in-flight = `(uint16_t)(notifyCount − confCount)`, drain while in-flight < 4 and `!congested`. **No shared read-modify-write** (a `volatile int8_t` credit decremented on one core and incremented on the other loses updates on dual-core ESP32 and silently quarters throughput). Watchdog: if in-flight > 0 and `confCount` unchanged for 2 s, the loop task resyncs by writing `notifyCount = confCount` snapshot (single-writer preserved); wrapped/negative in-flight (> 8) treated as 0. Drain-gate policy lives in the host-testable module (§3). Note in code: `ESP_GATTS_CONF_EVT` fires on send-to-controller for **notifications** (ATT confirmation only for indications), so credits pace the queue, they don't confirm delivery | `BLECharacteristic::notify()` reports `SUCCESS_NOTIFY` unconditionally when the enqueue returns `ESP_OK` — congestion is never surfaced by the wrapper; without back-pressure the sub-ms `loop()` cadence overruns Bluedroid's buffer pool on any multi-KB `GET snapshot` |
| **D18** | **RX-ring session reset is mark-based and consumer-side only.** The loop task never writes a producer-owned index. `onConnect` — which runs on the same Bluedroid host task as `onWrite`, so it is serialized with the producer — latches `rxConnectMark = head`. `pump()`'s session reset then performs only consumer-side writes: `tail = rxConnectMark` (legal: old-session `head` never exceeded the mark, so `tail ≤ mark` always holds) plus a `takeLineReset()` signal that makes `bleSerialEvent` clear its loop-task-owned `bleInputString`/discard flag. This discards exactly the old-session bytes (all written before the connect event on the same task) while **preserving a first command the new central wrote between connect and the first pump pass** — nothing is eaten during a splash `delay()` or busy stretch. Contract stated in `BleByteRing.h`; host test covers stale-byte discard, early-first-write preservation, and reset with a torn old-session line pending | Rev-2's "reset both rings in pump()" was race-free only for the TX ring (producer = loop task); resetting the RX producer index from the consumer task races a concurrent `onWrite` RMW and can replay a stale prior-session command line. The ftpRxRing precedent (MorsePileup.cpp:344-360) never violates single-writer — neither do we |
| **D19** | **`stop()` performs synchronous session teardown and never trusts `onDisconnect` to fire.** Order: `isRunning = false` first (all subsequent callbacks and module APIs become defined no-ops), best-effort `txFlush`, `BLEDevice::deinit(false)`, then synchronously clear `isConnected`, **`bleProtocol`**, `sessionResetPending`, `advertisePending`, congestion state, and reset the flow counters. Module contract: after `stop()`, `readByte()` returns false and `txEnqueue()`/`pump()`/`txFlush()` are safe no-ops — **including when `stop()` is invoked from within the `bleSerialEvent` → `serialDecode` call chain** (remote self-disable, §3). A late `onDisconnect` from Bluedroid teardown only re-clears flags | `esp_bluedroid_disable/deinit` gives no guarantee the application `onDisconnect` is delivered; if `bleProtocol` survived a WiFi-suspend or pref-OFF `stop()` with a live handshaken central, `protocolActive()` would stay true forever — `checkShutDown` (m32_v6.ino:2886) would never time out and a battery-powered device would never auto-sleep, invisibly (D14). Verified: `setParameter` loops `i <= posSerialOut` (m32_v6.ino:4361) so `posBleSerial` **is** remotely settable over BLE itself, reaching the change-switch `stop()` inside the BLE dispatch chain |

---

## 1. Goals / non-goals

**Goals**

- An iOS app (CoreBluetooth, no MFi) connects over BLE GATT and speaks the existing line-based M32 protocol verbatim: `PUT device/protocol/on\n`, `PUT menu/start now/<n>\n`, `PUT cw/play/...\n`, all GETs — and receives the JSON responses, unsolicited events, and the raw character echo (`SerialOutMorse`, gated by the existing "Serial Output" pref).
- USB serial protocol continues to work **simultaneously and observably unchanged**: a USB client that has handshaken sees today's bytes plus — *only if a BLE session is also handshaken* — the BLE session's responses (documented; 1.3 clients must tolerate unsolicited objects, M32 Protocol.md:99-103). A USB port that has **not** handshaken remains byte-silent (echo and `DEBUG()` excepted, exactly as today).
- Cross-transport semantic equivalence: commands are executed one line per poll in arrival order with device-state transitions applied between lines, on both transports (D6).
- Works on both canonical variants: `heltec_wifi_lora_32_V2` (classic, Bluedroid ESP32) and `pocketwroom` (ESP32-S3, BLE-only silicon).
- The protocol engine (`serialDecode`/`m32Get`/`m32Put`) is reused, not duplicated. No new protocol commands.

**Non-goals**

- No BLE + WiFi coexistence (BLE Serial suspends whenever the WiFi radio comes up — D8, including mid-game multiplayer and generator WiFi transmit).
- No simultaneous BLE Serial + BLE HID keyboard (mutual exclusion, D3).
- No NimBLE migration (Bluedroid stays; already linked into every env).
- No BLE-side extra framing, encryption, or multi-central support (one central at a time).
- No changes to LoRa/MOPP, and no new on-screen BLE status icon in v1.

## 2. Transport design

### GATT service (Nordic UART Service)

| Item | Value |
|---|---|
| Service UUID | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX characteristic (central → M32) | `6E400002-…`, properties **Write + Write Without Response** |
| TX characteristic (M32 → central) | `6E400003-…`, property **Notify**, with `BLE2902` CCCD |

Why NUS: de-facto standard for UART-over-BLE; every BLE tool (nRF Connect, LightBlue), bleak sample code, and countless iOS reference implementations speak it.

### MTU, chunking & flow control

- At init: `BLEDevice::setMTU(517)`. Negotiated value tracked in `onMtuChanged` (Bluedroid task) in a `volatile uint16_t`, read once per pump pass; payload assumed 20 until the event fires.
- **TX (notify):** bytes are staged from the TX ring into a 512-byte static buffer in chunks of `min(mtu − 3, 512)`; `notify()` per chunk. Chunk boundaries are arbitrary — may split a JSON object or a UTF-8 char; the client reassembles the byte stream and frames exactly like a USB client (balanced-brace framing, raw echo bytes outside objects).
- **Flow control (D17):** custom GATTS hook, filtered by `gatts_if` + `conn_id` (cheap defense: the hook sees every server event, and if the mutual-exclusion invariant is ever weakened, HID confirmations must not credit the NUS pump). It writes `volatile bool congested` (from `ESP_GATTS_CONGEST_EVT`, `param->congest.congested`) and the BT-task-owned `confCount`. `pump()` (sole writer of `notifyCount`) drains only while `linkUp() && !congested && inFlight < 4`, at most 2 chunks per pass; ring bytes are retired only when their chunk's `notify()` has been issued under available in-flight budget. Watchdog resync per D17. This paces the drain to the connection-event cadence (~5-10 KB/s on iOS at 15-30 ms intervals); residual loss risk (the wrapper hides per-PDU esp_err) is documented in DESIGN.md, as is the CONF-vs-indication semantics note.
- **RX (write):** each GATT write's payload (`getData()`/`getLength()` — **not** `getValue()`, which heap-allocates a `std::string` on the BT host task) is `memcpy`'d into the RX ring; producer writes only `head` (ftpRxRing discipline). Lines end with `\n` (`\r` tolerated); a line may span writes; multiple lines may arrive in one write — but only **one completed line is dispatched per poll** (D6).

### Advertising & name

- Device name **`Morserino-32`** (`BLEDevice::init("Morserino-32")`) — distinct from the HID's "Morserino32 Keyboard". The advertisement PDU carries flags + the 128-bit NUS UUID (21 B — the 12-char name cannot also fit in 31 B); the **name goes in the scan response** (`setScanResponse(true)`), verified visible in scanners in §8 step 1.
- No appearance field, no HID UUIDs.
- Advertise whenever running and no central connected. On disconnect, callbacks only set flags (`advertisePending`, `sessionResetPending`); advertising restarts from `pump()` on the loop task — never block a Bluedroid callback (the HID `delay(5000)` in `onDisconnect` is the anti-pattern).
- One central at a time.

### Stop/start cycles (D16 + D19 — verified against core 2.0.17)

- `MorseBleSerial::stop()` = `isRunning = false` → bounded `txFlush` → `BLEDevice::deinit(false)` → **synchronous clear of `isConnected`, `bleProtocol`, pending-session flags, and flow-control state** (D19). This clears the library's `initialized` flag (BLEDevice.cpp:660) so a later `init()` actually re-initializes; controller memory (~30 KB BSS) stays reserved for the rest of the boot — accepted (§6). Safe if not running; safe when called from inside the `bleSerialEvent` dispatch chain (§3).
- `MorseBluetooth::stopBluetooth()` currently does `deinit(true)` (MorseBluetooth.cpp:214), which releases BTDM memory irreversibly **and leaves `initialized` true** — after it runs, any `BLEDevice::init()` is a no-op and `createServer()` deadlocks in `registerApp`. Under `#ifdef CONFIG_BLE_SERIAL` this becomes `deinit(false)` (flag-off builds unchanged). Without this, the sequence *HID keyer session → menu_() stopBluetooth → user enables BLE Serial → backstop `init()` on the loop task* would freeze the device.
- `MorseBleSerial::init()` is defensive: if `BLEDevice::getInitialized()` is already true (HID currently up, or a poisoned post-`deinit(true)` state) or `esp_bt_controller_get_status()` is inconsistent, it does **not** call `createServer`; it shows a splash `"BLE init fail"` + `jsonError("BLE INIT FAILED")` and returns `false` with `isRunning == false`. The loop task must never enter a call that can block forever.
- **Latent pre-existing bug to flag to Willi (devdocs + note in PR):** today every keyer→menu→keyer cycle with the BT keyboard re-runs `initializeBluetooth()` after a `deinit(true)`; the fresh `bluetoothTask` hangs forever in `registerApp` (contained only because it is a dedicated task) and leaks its 10 KB stack per cycle — the `//ESP.restart(); // not needed anymore` comment (MorseMenu.cpp:269) masks this. The `deinit(false)` change likely fixes it; verify on hardware and record the result. On the S3 (`pocketwroom`), also record what `esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)` actually returned in today's `deinit(true)` path (it may fail with `INVALID_ARG` on BLE-only silicon — poisoned-but-not-released all along), which changes the wording of the latent-bug note, not the fix.

### Coexistence with the BLE HID keyboard: mutual exclusion, BLE Serial wins

- If `pliste[posBleSerial].value != 0 && MorseBleSerial::isRunning`, both HID start sites — `menuExec` `_keyer` case (MorseMenu.cpp:459-465) and the loop backstop (m32_v6.ino:1086-1093) — skip `MorseBluetooth::initializeBluetooth()`. Gating on `isRunning` too means a failed/refused BLE init (D16) degrades to today's keyboard behavior instead of leaving the user with neither. On keyer entry, one-time splash line 1 `"BT Kbd off"` (10 chars), line 2 `"BLE Serial on"` (13 chars — both ≤ 14, the OLED `NoOfCharsPerLine`, MorseOutput.cpp:23), plus `jsonCreate("message", "BT Kbd off: BLE Serial active", "")`.
- The `menu_()` "Stop BT Kbd" block is untouched (guarded by `MorseBluetooth::isBLErunning`, false when the keyboard never started); it never references `MorseBleSerial`.
- Rationale for "serial wins": `PUT cw/play` requires the keyer active (m32_v6.ino:4294), so BLE Serial must work *in* keyer mode.

### WiFi-radio transitions and LoRa

All WiFi bring-up funnels through `MorseMenu::setupWifi()` (:790) and `MorseMenu::setupESPNow()` (:830) — called from the generator start path (MorseMenu.cpp:517/519, 647/655, when CW output = WiFi), and **mid-session** from multiplayer entry (MorsePileup.cpp:953, MorseMorsel.cpp:1448, MorseGridNet.cpp:553) — plus the `_wifi_mac/_wifi_config/_wifi_check/_wifi_upload/_wifi_update/_wifi_select` menu cases, which dispatch to `MorseWiFi::menuExec`/`menuNetSelect` directly (MorseMenu.cpp:772-781).

- Helper `suspendBleSerialForWifi()`: emit `jsonCreate("message", "BLE serial suspended: wireless mode", "")` (teed to the BLE client), `MorseBleSerial::txFlush(500)`, then `MorseBleSerial::stop()` — which synchronously clears `bleProtocol` (D19), so `protocolActive()` reverts to USB-only and auto-sleep keeps working. **Three call sites**: top of `setupWifi()`, top of `setupESPNow()`, and once before the `_wifi_*` dispatch (kept uniform even for display-only `_wifi_mac` — one rule, no special case).
- **Restart only in `menu_()`:** after the existing `WiFi.mode(WIFI_OFF)` + BT-Kbd block (:255-270): `if (pliste[posBleSerial].value && !MorseBleSerial::isRunning) MorseBleSerial::init();`. Consequence, stated plainly and documented: **starting a multiplayer round or a WiFi-transmit generator session drops the BLE link, and it stays down for the remainder of that session**, resuming at the top menu. Additionally documented: `_wifi_mac` and `_wifi_check` are remotely executable (`isRemotelyExecutable` excludes only config/upload/update/select, MorseMenu.cpp:861-869) — a BLE client that issues them **cuts off its own link** (Transports section, §9).
- **LoRa** (`loraTrx`, classic V2): BLE Serial stays up — SX127x/SX126x on SPI, not the shared 2.4 GHz radio. Heap in §6.

## 3. Firmware architecture

### Gating: emission vs. delivery (D15 — the load-bearing change)

Verified reality: `serialDecode()` begins `if (!m32protocol) return;` (m32_v6.ino:3806-3807), and all protocol output is *generated* only under `if (m32protocol)` at 40 call sites. The fix has two layers:

**Layer 1 — emission**: new predicate in `M32ProtocolOut.h`:

```cpp
extern boolean m32protocol;                    // existing (extern'd already in MorseMenu.h:24, MorsePreferences.h:30)
#ifdef CONFIG_BLE_SERIAL
extern volatile bool bleProtocol;
inline bool protocolActive() { return m32protocol || bleProtocol; }
#else
inline bool protocolActive() { return m32protocol; }
#endif
```

Complete verified inventory of `if (m32protocol)` sites and their disposition:

| File | Sites → `protocolActive()` | Sites left as-is (reason) |
|---|---|---|
| `m32_v6.ino` | :1042, :1155, :1162, :1173, :1181, :1278, :2657, :2676, :2886 (checkShutDown timeout suppression), :2892 (Power-off message), :2903 (shutMeDown jsonError), :3480, :3497, :3504 — **14**; plus the `serialDecode` guard :3806 → `if (!protocolActive()) return;` | :3777 (USB dispatch — per-transport by design), :3782 (USB handshake set), :3977-3978 (`protocol/off` handler clears `m32protocol` only — BLE off never reaches it, intercepted in `bleSerialEvent`) |
| `MorseMenu.cpp` | :262, :309, :414 (`jsonMenu` on knob turn), :446 (`jsonActivate`), :652, :675, :757, :855, :930, :953, :957, :964 — **12** | — |
| `MorsePreferences.cpp` | :753, :769, :784, :787, :873, :1662, :1692, :1736 — **8** | — |
| `MorseWiFi.cpp` | :277, :339, :363, :375, :405, :580 — **6**, swept for uniformity even though BLE is always suspended (hence `bleProtocol == false`) in WiFi modes — one rule, no special case to drift | — |

`shutMeDown()` additionally gets `MorseBleSerial::txFlush(300)` after its `jsonError` (m32_v6.ino:2903), mirroring the `PUT device/reset` treatment, so a BLE client receives the power-off notice best-effort.

**Layer 2 — delivery** is the tee's job (below): Serial gets protocol bytes iff `m32protocol`, BLE iff `bleProtocol && linkUp()`. Net observable behavior: un-handshaken USB stays silent (today's behavior preserved even while BLE is active); each handshaken transport sees everything (documented).

### New module: `MorseBleSerial.cpp` / `.h` (+ pure ring/chunker/flow-policy in `BleByteRing.h`)

Entire implementation wrapped in `#ifdef CONFIG_BLE_SERIAL`. Namespace `MorseBleSerial`:

```cpp
namespace MorseBleSerial {
  extern volatile bool isRunning;      // GATT server up; set false FIRST in stop()
  extern volatile bool isConnected;    // set/cleared in callbacks (BT task) AND cleared synchronously by stop()
  bool linkUp();                       // isRunning && isConnected && !sessionResetPending — the only enqueue/drain gate
  bool init();                         // defensive (D16): false + visible error if stack unusable; never blocks
  void stop();                         // D19: isRunning=false → txFlush best-effort → deinit(false) → clear ALL session state
  void pump();                         // loop task: mark-based session reset (D18) → advertising restart → flow-gated TX drain
  bool readByte(uint8_t &b);           // consume one byte from RX ring (loop task); false after stop()
  bool takeLineReset();                // read-and-clear: bleSerialEvent must discard its partial line (set by session reset)
  bool takeRxOverflow();               // read-and-clear overflow flag (line granularity)
  size_t txEnqueue(const uint8_t *buf, size_t len);        // protocol replies: bounded self-drain ≤20 ms, then drop + txDropped++
  size_t txEnqueueEcho(const uint8_t *buf, size_t len);    // echo channel: drop-immediately, NO self-drain (keying path, D5)
  void txFlush(uint32_t timeoutMs);    // pump until TX ring empty or timeout (congestion-aware); no-op after stop()
}
```

- **No dedicated FreeRTOS task.** `init()` runs synchronously on the loop task (boot latency ~200-500 ms, measured and recorded in DESIGN.md); GATT callbacks run on the Bluedroid host task regardless.
- **Ring/chunker/flow-policy as a pure module (`BleByteRing.h`):** head/tail logic, the **mark-based reset contract (D18)**, overflow policy, MTU−3 chunk staging, **and the drain-gate policy (in-flight from monotonic counters, congestion, watchdog resync, wrap clamp — D17)** are plain-buffer/plain-integer code with no BLE types; the Bluedroid callbacks are a thin adapter. The trickiest logic (flow control, session reset) is exactly what gets host-side tests before any hardware (§8).
- **Buffers (static):** RX ring 1 KB (producer = `onWrite` on BT task, consumer = loop task); TX ring 3 KB (producer *and* consumer = loop task — pump-side reset is trivially safe for this ring); 512 B TX staging buffer; overflow flags `volatile`.
- **Callbacks set flags/marks only — no consumer-owned state, no loop-task-owned state** (single-writer discipline throughout): `onConnect` → `rxConnectMark = head` (serialized with `onWrite` on the same Bluedroid host task, so the mark cleanly separates old-session from new-session bytes), `isConnected = true`, `sessionResetPending = true`. `onDisconnect` → `isConnected = false`, `bleProtocol = false`, `sessionResetPending = true`, `advertisePending = true`, return immediately. `onMtuChanged` → store `volatile` payload size. `onWrite` → `getData()/getLength()` copy, advance `head` only. GATTS hook → `congested`, `confCount` (BT-task-owned counter, D17), filtered by `gatts_if`+`conn_id`.
- **`pump()`** (loop task, in order): (1) if `sessionResetPending` → **RX ring: `tail = rxConnectMark` (consumer-owned index only — never `head`; old-session bytes all precede the mark, a new central's early first write survives)**; TX ring + staging reset (loop-task-owned); `notifyCount = confCount` snapshot; set the `takeLineReset()` latch so `bleSerialEvent` clears `bleInputString` and its discard flag; clear `rxOverflow`; clear `sessionResetPending`; (2) if `advertisePending` → `startAdvertising()`; (3) drain TX per D17 (≤2 chunks/pass, in-flight < 4, `!congested`).
- **`txEnqueue` full-ring policy (protocol channel):** self-drain (same gate as pump) bounded to **20 ms**, then drop the remainder, `txDropped++`. **`txEnqueueEcho` (echo channel): drop-immediately, no self-drain** — `SerialOutMorse` sits in the keying path and a dit at 40 WpM is ~30 ms, so even the bounded drain could be audible at speed. Torn-JSON consequence documented (client re-issues the GET).
- **After `stop()` (D19):** `readByte` returns false, `takeRxOverflow`/`takeLineReset` return false, `txEnqueue*`/`pump`/`txFlush` are no-ops, `linkUp()` is false. This makes the remote-self-disable chain safe: `PUT config/<BLE Serial>/0` over BLE → `setParameter` (loops `i <= posSerialOut`, m32_v6.ino:4361, so `posBleSerial` is reachable) → `writePreferences` → change-switch `stop()` — control then unwinds into `serialDecode`, whose `jsonOK` hits the tee with `bleProtocol`/`linkUp()` now false and is dropped; `bleSerialEvent`'s loop exits via `readByte() == false`. Client-visible outcome documented (§7, §9): the change-switch emits `jsonCreate("message", "BLE serial off", "")` + `txFlush(300)` *before* `stop()`, best-effort; **the `jsonOK` itself is lost and the link drops** — stated in the protocol doc.

### Output sink: tee (`M32Tee`), new files `M32ProtocolOut.h/.cpp`

```cpp
class M32Tee : public Print {
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t len) override;
  // protocol channel: Serial.write iff m32protocol; BLE txEnqueue iff bleProtocol && MorseBleSerial::linkUp()
  void echo(const String &s);
  // echo channel: Serial.print ALWAYS (today's posSerialOut behavior, protocol handshake irrelevant);
  //               BLE copy via txEnqueueEcho (drop-immediately) iff bleProtocol && linkUp()
};
extern M32Tee m32out;
```

- Without `CONFIG_BLE_SERIAL`: `write()` is a plain unconditional `Serial.write` forward and `echo()` a plain `Serial.print` — byte-identical, because every `write()` call site is only reachable under an `m32protocol` gate in flag-off builds.
- **Header hygiene:** `M32ProtocolOut.h` references only the extern flags and declares the class; the `MorseBleSerial` dependency is confined to `M32ProtocolOut.cpp` — no `MorseJSON.cpp → M32ProtocolOut.h → MorseBleSerial.h` header cycle.
- **Why tee, not route-to-origin:** unsolicited objects (`jsonMenu`, `jsonActivate`, `jsonConfigShort`, `jsonControl`, ~20 `message` sites) have no requesting transport; per-transport *handshake* gating in the tee gives each client exactly what it asked for without threading an origin token through the engine. (A future third transport would be a third branch in `write()`/`echo()` plus a disjunct in `protocolActive()` — acceptable enumeration for now.)
- Substitution sites (complete verified output surface — the **gates** are D15, these are the **writes**):
  - `MorseJSON.cpp`: 24 × `serializeJson(doc, Serial)` (lines 29, 48, 60, 96, 114, 137, 146, 155, 165, 178, 193, 208, 222, 272, 290, 298, 325, 341, 357, 411, 425, 433, 450, 470) plus raw prints in `jsonFileFirstLine` (227, 236, 238) and `jsonFileText` (244, 249-258, 262) → `m32out`.
  - `m32_v6.ino:3929` — `GET file/parts` outlier `serializeJson(doc, Serial)` → `m32out`.
  - `SerialOutMorse()` m32_v6.ino:2518 — `Serial.print(s)` → `m32out.echo(s)`.
  - `m32_v6.ino:3997` — before `ESP.restart()`: after `Serial.flush()`, add `MorseBleSerial::txFlush(500)`.
  - **Not** substituted: `DEBUG()` (m32_v6.ino:451-454) and the RadioLib boot prints (:774-776) stay raw `Serial` (D12).

### RX path: feeding `serialDecode` without duplicating the engine

New globals in `m32_v6.ino` beside `m32protocol` (:181): `volatile bool bleProtocol = false;` (cleared by `onDisconnect` on the BT task **and synchronously by `stop()`** — D19), `String bleInputString;` (`reserve(400)` in `setup()`, mirroring `inputString` :182/485), and `bool bleDiscardLine = false;`.

```cpp
#ifdef CONFIG_BLE_SERIAL
void bleSerialEvent() {                       // m32_v6.ino, directly below serialEvent() — deliberate mirror of its
  uint8_t b;                                  // line assembly (cross-referenced in comments both ways)
  MorseBleSerial::pump();                     // session resets (D18) + advertising + flow-gated TX drain
  if (MorseBleSerial::takeLineReset()) { bleInputString = ""; bleDiscardLine = false; }
  while (MorseBleSerial::readByte(b)) {       // returns false immediately after stop() (D19)
    if (bleDiscardLine) { if (b == '\n') bleDiscardLine = false; continue; }   // runaway/overflow tail: never executed
    bleInputString += (char)b;
    if (bleInputString.length() > 400) { bleInputString = ""; bleDiscardLine = true; continue; }
    if (b != '\n') continue;
    bleInputString.trim();
    if (MorseBleSerial::takeRxOverflow())     // torn line: discard, report once
      MorseJSON::jsonError("BLE RX OVERFLOW");
    else if (!bleProtocol) {
      if (bleInputString.equalsIgnoreCase("put device/protocol/on")) {   // allocation-free, fully case-insensitive
        bleProtocol = true; MorseJSON::jsonDevice(brd, vsn);
      }
    } else if (bleInputString.equalsIgnoreCase("put device/protocol/off")) {  // rev-4 fix: whole-line compare —
      MorseJSON::jsonCreate("end m32protocol", "Goodbye!", "");               // no casing hole, no String copy
      bleProtocol = false;                                                    // D7: USB session untouched
    } else
      serialDecode(bleInputString);                                // engine reused; guard is protocolActive() (D15)
    bleInputString = "";
    break;                                                         // D6: EXACTLY ONE completed line per poll —
  }                                                                // loop() consumes goToMenu/executeMenu/executeNow
}                                                                  // between polls, matching USB's break-at-\n (:3769-3771)
#endif
```

- Called from **inside `serialEvent()`** (one added line at its end, under `#ifdef`): `serialEvent()` is polled in every device state (`loop()` m32_v6.ino:1098, menu wait-loop MorseMenu.cpp:288, ~45 busy-wait sites), so BLE is serviced everywhere USB is, with zero new call sites. One-line-per-poll makes batched writes behave identically to USB. One poll can dispatch one USB line **and** one BLE line; per-transport ordering is guaranteed, cross-transport interleaving within a poll is not — one sentence in the Transports section (§9).
- `checkShutDown` (m32_v6.ino:2886): `if (protocolActive() && !enforce) return;` — `bleProtocol` clears on disconnect **and synchronously on every `stop()`** (D19), so neither an abandoned device nor a WiFi-suspended one gets zombie sleep-suppression; a live BLE session receives the power-off `message` (:2892) and shutdown `jsonError` (:2903) via the D15 sweep.

## 4. Lifecycle & UX

- **Control:** preferences toggle **"BLE Serial"** (OFF/ON) via double-click → preferences (UX_CONVENTIONS §10). No menu entry — BT-keyboard precedent. ON takes effect on next return to the top menu (`menu_()` backstop); OFF stops BLE on preferences exit via the `writePreferences` change-switch — `case posBleSerial:` **guarded `if (morserino)`** (posGoertzelBandwidth precedent, MorsePreferences.cpp:1614-1617) so snapshot stores never fire it; on OFF it emits the best-effort `"BLE serial off"` message + `txFlush(300)` before `stop()` (D19).
- **Boot:** in `setup()`, after `MorseMenu::wifiWarmup()` (m32_v6.ino:628) and the spurious-serial drain (:828-831): `if (pliste[posBleSerial].value) MorseBleSerial::init();`.
- **Across modes:** up through menu navigation, keyer, generator (unless CW output = WiFi), echo, Koch, decoder, LoRa Trx, single-player games, QSO Bot. **Down whenever the WiFi radio comes up** — including mid-game multiplayer entry and generator WiFi transmit — and stays down until the next return to the top menu (D8, documented).
- **Connection indication (v1):** none on-screen (D14); suspension and mutual-exclusion events get a one-time splash + protocol `message`. Pinned strings (OLED budget 14 chars/line, MorseOutput.cpp:23): splash `"BT Kbd off"` / `"BLE Serial on"` / `"BLE init fail"`; messages `"BT Kbd off: BLE Serial active"`, `"BLE serial suspended: wireless mode"`, `"BLE serial off"`.
- **Factory reset:** `resetDefaults()` (MorsePreferences.cpp:1641) persists `bleSerial = 0` like any other pref — noted in DESIGN.md and exercised in §8.
- **UX_CONVENTIONS.md addition** (draft, added verbatim):

  > **Background connectivity services.** Services that extend the device's I/O channels without their own screen (currently: BLE Serial; pattern also covers the Bluetooth keyboard) are controlled by a preferences toggle, never a menu entry. When enabled they start at boot and remain active across menu navigation and interactive modes, except that any activity which needs the WiFi radio — a WiFi menu function, WiFi transceiver, or a mode that starts wireless multiplayer or WiFi transmit mid-session — suspends them for the remainder of that session; they resume automatically on return to the top menu. Connection and disconnection are silent on the device screen; state changes the user must know about (suspension, mutual exclusion with another service) are shown once as a short splash and mirrored as an M32-protocol `message` object.

## 5. File-by-file change list

| File | Change |
|---|---|
| `Software/src/platformio.ini` | Add `-D CONFIG_BLE_SERIAL=1` to `[env:heltec_wifi_lora_32_V2]` `build_flags` and to `common.M32_Pocket_build_flags`, with a comment that the latter propagates to every pocket-derived env (intended; only the two canonical envs are build-gated) |
| **NEW** `Version 6 and newer/MorseBleSerial.h/.cpp` | NUS GATT server; callbacks flag/mark-only (D18); custom GATTS hook (conn_id/gatts_if-filtered congestion + `confCount`); defensive `init()`; **`stop()` with synchronous session teardown + post-stop no-op contract (D19)**; pump per §3; whole file `#ifdef CONFIG_BLE_SERIAL` |
| **NEW** `Version 6 and newer/BleByteRing.h` | Pure header-only ring + chunk staging + **mark-based consumer-side reset contract (D18)** + **drain-gate flow policy (monotonic counters, watchdog resync, wrap clamp — D17)** over plain buffers (no BLE types) — host-testable |
| **NEW** `Version 6 and newer/M32ProtocolOut.h/.cpp` | `M32Tee` (protocol channel + drop-immediately echo channel), global `m32out`, `protocolActive()` predicate; `MorseBleSerial` include confined to the `.cpp` (no header cycle) |
| `MorseJSON.cpp` | `Serial` → `m32out` at the 24 `serializeJson` sites + `jsonFileFirstLine`/`jsonFileText`; `#include "M32ProtocolOut.h"`; `jsonGetSnapshot` loop (:391-394): add `posBleSerial` skip beside `posTimeOut` (`#ifdef`) |
| `m32_v6.ino` | (a) globals `volatile bool bleProtocol`, `bleInputString` (+`reserve(400)`), `bleDiscardLine`; (b) `bleSerialEvent()` (one line/poll, discard-until-`\n` runaway tail, prefix-checked off-interception) below `serialEvent()`, called from its end; (c) boot init after `wifiWarmup()`; (d) **D15 sweep — 14 gate sites** (:1042, 1155, 1162, 1173, 1181, 1278, 2657, 2676, 2886, 2892, 2903, 3480, 3497, 3504) → `protocolActive()`; (e) `serialDecode` guard :3806 → `protocolActive()`; (f) `SerialOutMorse` :2518 → `m32out.echo`; (g) `GET file/parts` :3929 → `m32out`; (h) `PUT device/reset` :3997 + `shutMeDown` :2903: add `txFlush`; (i) HID loop backstop :1086-1093: skip iff `pliste[posBleSerial].value && MorseBleSerial::isRunning` |
| `morsedefs.h` | **Rule 9 triple, part 1:** `#ifdef CONFIG_BLE_SERIAL posBleSerial, #endif` in `prefPos` immediately before `posSerialOut` |
| `MorsePreferences.cpp` | **Rule 9 parts 2+3, identical position, same `#ifdef`:** `prefName[]` `"bleSerial"` before `"serialOut"`; `pliste[]` `{0, 0, 1, 1, "BLE Serial", "BLE remote ctrl", true, {"OFF","ON"}}` before serialOut. Plus: (a) `BLESER` macro beside `BLUE` (:575-578); insert before `posSerialOut` in all 13 option arrays containing it (keyerOptions :587 … allOptions :671); (b) **read-side** snapshot skip: `|| i == posBleSerial` in the `!morserino` skip (:1454-1456); (c) **write-side** snapshot skip: `if (i == posBleSerial && !morserino) continue;` beside the `posTimeOut` skip in the `writePreferences` loop (:1602-1604); (d) change-switch: `case posBleSerial: if (morserino && pliste[posBleSerial].value == 0) { MorseJSON::jsonCreate("message", "BLE serial off", ""); MorseBleSerial::txFlush(300); MorseBleSerial::stop(); } break;`; (e) **D15 sweep — 8 gate sites** (:753, 769, 784, 787, 873, 1662, 1692, 1736) |
| `MorseMenu.cpp` | (a) `menu_()` restart backstop after :255-270; (b) `_keyer` case (:459-465): skip HID when BLE Serial on **and running** + one-time splash/message; (c) `suspendBleSerialForWifi()` at top of `setupWifi()` (:790), `setupESPNow()` (:830), and before the `_wifi_*` dispatch (:772-781); (d) **D15 sweep — 12 gate sites** (:262, 309, 414, 446, 652, 675, 757, 855, 930, 953, 957, 964) |
| `MorseWiFi.cpp` | **D15 sweep — 6 gate sites** (:277, 339, 363, 375, 405, 580), uniformity (no-op for BLE, suspended in WiFi modes) |
| `MorseBluetooth.cpp` | `stopBluetooth()` :214: `#ifdef CONFIG_BLE_SERIAL` → `BLEDevice::deinit(false)` `#else` `deinit(true)` (D16); comment referencing the latent re-init hang |
| `CLAUDE.md` | New §3 hard-rule line: protocol emission gates on `protocolActive()`, protocol output via `m32out`, echo via `m32out.echo()` — never raw `Serial` or bare `m32protocol` for new protocol code (the §8 grep is one-time; the invariant must survive it) |
| `Documentation/…`, `devdocs/…` | See §9 |

**No new NVS namespace** — `bleSerial` is a plain `uint8_t` key in `morserino` (§4 conventions satisfied).

## 6. Memory / flash budget

- **Flash:** Bluedroid GATT-server code already linked via `CONFIG_BLUETOOTH_KEYBOARD`; incremental cost ≈ our module (~8-15 KB). Classic V2 at ~61% of its OTA slot — ample.
- **Static RAM:** RX 1 KB + TX 3 KB + staging 0.5 KB + reserved `String`s ≈ **6 KB bss** vs. V2's 45.9 KB current bss / 320 KB budget.
- **Heap:** Bluedroid ≈ 45-60 KB — the load the HID keyboard already imposes concurrently with the keyer on V2. New combination **BLE + LoRa Trx** (V2) verified on hardware (heap monitor m32_v6.ino:1315-1324, `#ifdef` widened to `|| defined(CONFIG_BLE_SERIAL)`).
- **`deinit(false)` consequence (D16), stated explicitly:** on classic V2, WiFi modes now run with the BT controller BSS still reserved once BLE Serial (or the keyboard) has ever started — the same heap situation as today's boot→WiFi-upload path (which works), but *less* free heap than today's keyer-with-BT→menu→WiFi path where `mem_release` donated ~60-70 KB. Dedicated §8 test: WiFi upload/OTA on V2 **after** a BLE Serial session. ESP32-S3 nuance (BLE-only controller, no classic-BT release mode; actual `mem_release` return value recorded per §2) in DESIGN.md; the stop/start hardware gate runs on both boards.
- **Stop/start heap hygiene:** arduino-esp32 2.x has field reports of per-cycle heap leaks across `deinit(false)`/`init()` cycles; §8 step 10 records the **free-heap delta per cycle** on both boards (the WiFi-suspend/menu-restart cycle runs many times per battery charge — a few KB per cycle would matter).
- **String churn:** none in hot paths. Callbacks: `memcpy` only (`getData/getLength`). Chunks from static staging. `bleInputString` pre-reserved; the handshake probes use `equalsIgnoreCase` — no String copies on the command path at all (rev. 4).

## 7. Failure modes & edge cases

| Case | Behavior |
|---|---|
| Client disconnects mid-line / reconnects | Callbacks set flags + `rxConnectMark`; **pump() resets consumer-side only (`tail = mark`, D18)** — no loop-task write to a producer index, no replay of stale prior-session bytes as commands; `takeLineReset()` clears the partial `bleInputString` |
| Central connects and writes its first command before pump() runs (splash `delay()`, busy stretch) | Preserved, not eaten: the mark separates sessions at the connect event (BT task, serialized with `onWrite`); reset discards only bytes below the mark |
| Client disconnects while TX ring non-empty | `linkUp()` gate stops enqueue and drain; TX ring reset by pump on next session (loop-task-owned, trivially safe) |
| Connected but not subscribed | CCCD respected by Bluedroid; protocol replies only flow after handshake anyway |
| Bluedroid TX congestion (multi-KB `GET snapshot`) | D17: `ESP_GATTS_CONGEST_EVT` halts the drain; in-flight < 4 via single-writer monotonic counters (no cross-core RMW loss, no silent throughput collapse); bytes retired only when sent under budget |
| RX ring overflow | Producer drops + flags; consumer discards through next `\n` (never executes a torn command), `jsonError("BLE RX OVERFLOW")` once |
| Command line > 400 chars | Line dropped **and `bleDiscardLine` set until the next `\n`** — the tail of the runaway line is never dispatched as a garbage command (same policy as the overflow path) |
| TX ring full (stalled client) | Protocol channel: `txEnqueue` self-drain ≤ 20 ms then drop + `txDropped`; **echo channel: drop-immediately, zero drain** (keying path, 40 WpM dit ≈ 30 ms); torn JSON documented, client re-issues |
| Batched commands in one GATT write | One line per poll (D6): identical semantics to USB, state transitions between lines |
| Both transports handshaken | Each sees all responses/events (documented; 1.3 clients tolerate unsolicited objects). Un-handshaken USB sees nothing even while BLE is active. Cross-transport interleaving within one poll possible (documented) |
| `PUT device/protocol/off` | Per-transport (D7): BLE-side interception clears `bleProtocol` only; USB path (:3977-3978) unchanged. The goodbye object is teed to the *other* handshaken transport too — documented with a discriminator rule: **a client treats `"end m32protocol"` as its own session end only if it sent `protocol/off`**. Case asymmetry (BLE lenient, USB thirdArg case-sensitive) accepted + documented |
| Remote self-disable: `PUT config/<BLE Serial>/0` **over BLE** | Reachable (setParameter loops `i <= posSerialOut`, m32_v6.ino:4361) → change-switch: best-effort `"BLE serial off"` message + `txFlush(300)`, then `stop()` inside the dispatch chain — safe by the D19 no-op contract (`readByte` false, tee BLE gate false); **the `jsonOK` is lost and the link drops — documented in the protocol doc** |
| WiFi suspend / pref-OFF while a central is connected and handshaken | `stop()` **synchronously** clears `bleProtocol`/`isConnected` (D19) — never relies on `onDisconnect` being delivered through `deinit`; `protocolActive()` reverts immediately, **auto-sleep timeout keeps working** (§8 test), no zombie emission behind the tee |
| Sleep / power-off | `checkShutDown` suppressed only while genuinely `protocolActive()`; power-off `message` + shutdown `jsonError` reach a live BLE client, `txFlush(300)` best-effort |
| `PUT device/reset` | `Serial.flush()` + `txFlush(500)`; link drop documented |
| WiFi radio needed (menu function — incl. remotely-executable `_wifi_mac`/`_wifi_check` — multiplayer entry, generator WiFi TX) | `message` → flush → `stop()`; re-advertises only after return to top menu (stays down for the session remainder — documented, incl. the client-issued self-cutoff) |
| Stop/start cycling | `deinit(false)` design (D16): `initialized` cleared, re-init real, no `registerApp` deadlock; `init()` refuses visibly (splash + `jsonError`, no `createServer`) if `BLEDevice::getInitialized()` is unexpectedly true or controller status inconsistent; heap delta per cycle recorded (§6) |
| HID ran and was stopped, then BLE Serial enabled | Safe: `stopBluetooth()` now `deinit(false)` under the flag → backstop `init()` succeeds (was a guaranteed loop-task freeze, BLEDevice.cpp:649-663) |
| BLE Serial ON but `init()` failed, `posBluetoothOut != 0` | HID skip requires `isRunning` — user gets today's keyboard behavior, not neither |
| Snapshot store/recall | `posBleSerial` excluded on read (:1456), write (:1603), and `jsonGetSnapshot`; change-switch guarded `if (morserino)` — no dead keys in `snap0..7`, no BLE stop during snapshot store; recall leaves the live value untouched |

## 8. Test plan

**Build gates (after every risky step, and final):**
```
cd Software/src
pio run -e heltec_wifi_lora_32_V2
pio run -e pocketwroom
```
Plus one `pocketwroom` build **without `CONFIG_BLE_SERIAL`** (tee degenerates to plain `Serial`, `MorseBluetooth` keeps `deinit(true)`).

**Without hardware:** both builds clean; rule-9 triple-array audit under the same `#ifdef`; grep that no `serializeJson(doc, Serial)` / protocol `Serial.print` remains outside `DEBUG()` and the RadioLib boot prints; grep that no `if (m32protocol` emission gate remains outside the documented as-is list (§3 inventory); **host-side scratch test of `BleByteRing.h`**: wrap-around, overflow-drop, split-UTF-8 chunk boundary, MTU−3 math, **mark-based session reset (stale bytes below mark discarded; bytes written after the mark preserved; reset with torn old line pending; producer index never written by consumer-side reset — D18)**, and **flow policy (in-flight from monotonic counters incl. uint16 wrap, congestion gate, watchdog resync, wrap clamp — D17)**; size-report deltas.

**On-hardware end-to-end (Mac, Python `bleak`), script `scratchpad/ble_m32_test.py`:**
1. Scan: device appears as **`Morserino-32`** (name via scan response — verify visible in nRF Connect scanner) with the NUS UUID; connect; subscribe to TX; handler reassembles by balanced braces, prints raw echo outside objects.
2. `PUT device/protocol/on\n` → `{"device":…}` with `"protocol version":"1.3"`. **Variant:** issue the handshake *immediately* after connect (before the device leaves a splash/busy stretch) — must not be swallowed (D18 early-write preservation).
3. `GET menus\n` → multi-chunk reassembly correct (chunking gate).
4. `PUT menu/start now/<keyer>\n` → `{"activate":…}` (validates the D15 sweep — arrives only if `jsonActivate`'s gate is `protocolActive()`).
5. `PUT cw/play/CQ CQ CQ DE W2ASM\n` → ok + sidetone + (Serial Output = 4/5) echo over BLE.
6. `PUT cw/stop\n`; `GET configs\n` includes `"BLE Serial"`.
7. **Batched write:** single GATT write `b"PUT menu/start now/<keyer>\nPUT cw/play/HI\n"` → both succeed (one-line-per-poll gate; on USB the same batch must behave identically).
8. **Transport isolation:** with BLE handshaken, un-handshaken USB terminal stays byte-silent; handshake USB too → both receive both sessions' traffic; `PUT device/protocol/off` on USB → BLE unaffected, and vice versa; goodbye-object discriminator behavior confirmed. Also `PUT device/protocol/OFF` (uppercase) **and `Put device/protocol/off` (mixed-case verb — the rev-4 regression case)**: both end only the BLE session when sent over BLE (USB session untouched, `bleProtocol` cleared); on USB the uppercase variant errors — matches the documented asymmetry.
9. **Congestion/flow control:** `GET snapshot` (multi-KB) with an artificially slow central (nRF Connect, long conn interval) → JSON reassembles untorn; verify in-flight/congest counters via temporary DEBUG; confirm no long-session throughput decay (the old RMW-credit failure mode).
10. **Lifecycle:** enter a WiFi function → suspend `message`, disconnect; return to top menu → advertising resumes, reconnect works. **Repeat 5× on BOTH boards, recording free-heap delta per cycle** (deinit(false)/re-init gate + leak check). Multiplayer entry in Fight the Pileup mid-game → BLE drops safely, stays down until top menu. **Reconnect-replay check:** send a partial line, force-disconnect, reconnect, handshake → the stale fragment must never execute (D18). On S3, record the `esp_bt_controller_mem_release` return from the pre-change `deinit(true)` path for the DESIGN.md latent-bug note.
11. **Sleep with suspended session (D19 gate):** handshake over BLE, set a short timeout, enter a WiFi menu function (suspend while handshaken), return to top menu but do **not** reconnect → device must auto-sleep on schedule (`bleProtocol` was cleared synchronously by `stop()`). Also: **remote self-disable** — `PUT config/BLE Serial/0` over BLE → `"BLE serial off"` message arrives best-effort, link drops, no crash, device auto-sleeps later.
12. **Keyer interplay (V2):** `BLT Kbd Output` ≠ Nothing + BLE Serial ON → splash "BT Kbd off"/"BLE Serial on", BLE works in keyer; BLE Serial ON but init failed (simulate) → keyboard works as today (`isRunning` gate); BLE Serial OFF → keyboard exactly as today. Verify (and record for Willi) the pre-existing keyer→menu→keyer HID cycle with the `deinit(false)` change.
13. **V2 heap + timing regression:** WiFi upload/OTA **after** a BLE Serial session (controller BSS reserved — D16); `loraTrx` with BLE connected; 30-minute keyer soak with heap monitor above the 20 KB low-water mark **while listening for CW timing glitches, including a ≥35 WpM run with a stalled client** (echo drop-immediately path). Measure and record `init()` boot-latency delta.
14. **Snapshots:** store a snapshot → `bleSerial` key absent from `snap0..7` and no BLE stop fired during store; **recall a snapshot → live `posBleSerial` value unchanged** (read-side skip :1454-1456); factory reset → `bleSerial = 0` persisted (resetDefaults :1641).
15. **iOS smoke test:** nRF Connect/LightBlue on iPhone: connect, subscribe, handshake as UTF-8 — proves the no-pairing CoreBluetooth path.

## 9. Documentation updates

- **`Documentation/Protocol Description/M32 Protocol.md`** — version stays 1.3 (D13). Generalize the USB-only intro (:7) and battery note (:596); new **"Transports"** section: USB-CDC (unchanged) and BLE (NUS UUIDs, roles, name-in-scan-response, MTU/chunk reassembly, per-transport handshake and per-transport `protocol/off` incl. the case-sensitivity asymmetry and the goodbye-object discriminator rule, one-command-per-poll ordering guarantee per transport with the cross-transport-interleaving caveat, delivery-to-both when both transports are handshaken, suspension whenever the WiFi radio comes up — including multiplayer game entry and the remotely-executable `_wifi_mac`/`_wifi_check` self-cutoff — with link drop + re-advertise on return to the top menu, **remote self-disable outcome: message best-effort, no `jsonOK`, link drops**, sleep-suppression battery caveat). Dated doc-revision note under the header. **Written TODO for Willi to regenerate `M32 Protocol.pdf`** (no build script exists).
- **`Documentation/User Manual/Version 8.x/manual_en.md`** — Appendix 8 (:3203) new subsection "Connecting via Bluetooth (BLE)": enabling the pref, device name, no pairing, WiFi/multiplayer suspension ("starting a multiplayer game or a WiFi function drops the BLE link until you return to the top menu"), keyboard mutual exclusion (cross-ref Appendix 9), battery note. "BLE Serial" row in the preferences table (:2622). Glossary BLE entry widened (:3644).
- **`manual_de.md`** — identical changes in German (Anhang 8 :3611, Tabelle :3004, Glossar :4057), same session; PDFs rebuilt via `Version 8.x/build.sh` (keeps `normalize_ids.lua`).
- **`Documentation/FAQ/Morserino-32 Pocket FAQ.md`** §5 (:63-72): second BLE personality + exclusion rule.
- **`devdocs/ble-serial/DESIGN.md`** — transport/architecture/gating decisions; buffer sizes; **D18 reset contract and D17 single-writer counter scheme** with residual-loss note and CONF-vs-indication semantics; `deinit(false)` heap consequence on V2 + S3 controller-model nuance (incl. measured `mem_release` return); per-cycle heap deltas; boot-latency measurement; stop/start verification results; DEBUG + RadioLib-boot-print raw-Serial exemptions; and the **latent HID re-init hang** (deinit(true) leaves `initialized` true → `registerApp` deadlock + 10 KB stack leak per keyer cycle) with the verification outcome — flagged explicitly to Willi.
- **`devdocs/protocol-audit/PROTOCOL_SPEC.draft.md`** §1 — BLE transport rows beside USB-CDC.
- **`devdocs/UX_CONVENTIONS.md`** — "Background connectivity services" subsection (§4 draft).
- **`devdocs/consistency-audit/todo-resolutions.md`** §A1 — add `CONFIG_BLE_SERIAL`.
- **`CLAUDE.md` §3** — new hard-rule line (protocol emission gates on `protocolActive()`, output via `m32out`, echo via `m32out.echo()`), so the invariant outlives the one-time §8 grep.

## 10. Implementation order

1. Branch `ble-serial-protocol` (already created); add `CONFIG_BLE_SERIAL=1` to both env flag groups (+ propagation comment). **Build checkpoint** (flag unused).
2. Add `M32ProtocolOut.h/.cpp` (tee with BLE side stubbed, `protocolActive()` predicate, header-cycle-safe includes) and substitute `m32out` at every write site (§3 list). **Build checkpoint + USB regression** (protocol over USB byte-identical).
3. **D15 gating sweep:** all 40 `if (m32protocol)` emission/behavior sites → `protocolActive()` per the §3 inventory; `serialDecode` guard :3806. With `bleProtocol` still hardwired false this is behavior-neutral — verify by USB smoke test. **Build checkpoint both envs.**
4. `posBleSerial` preference: rule-9 triple, `BLESER` macro into all 13 option arrays, snapshot exemptions on read (:1456), write (:1603), and `jsonGetSnapshot`, guarded change-switch case (stub). **Build checkpoint both envs + boot test on one board** (rule-9 mistakes are boot-time NVS panics); pref visible in menu, `GET configs`, absent from a freshly stored snapshot, unchanged by snapshot recall.
5. Implement `BleByteRing.h` (rings + **D18 reset contract** + **D17 flow policy**) + host-side scratch tests; then `MorseBleSerial.cpp/.h` (server, flag/mark-only callbacks, filtered GATTS hook, defensive `init`, **D19 `stop()` with synchronous teardown and post-stop no-op contract**) and the `MorseBluetooth::stopBluetooth` `deinit(false)` change. Wire boot init + `menu_()` backstop. **Build checkpoint; hardware:** advertise (name in scan response), connect, loop-back RX bytes.
6. Wire RX dispatch (`bleProtocol`, `bleInputString` + discard flag, one-line-per-poll `bleSerialEvent()` inside `serialEvent()`, `checkShutDown` gate) and complete the tee's BLE side + echo channel (`txEnqueueEcho`). **Build checkpoint; hardware:** §8 steps 1-7 incl. the early-handshake variant.
7. Lifecycle hardening: `suspendBleSerialForWifi()` at the three hook sites, HID mutual-exclusion guards (`isRunning`-gated) + splash/message, `writePreferences` stop case live (message + flush + stop). **Hardware:** §8 steps 8, 10-12 incl. the 5× stop/start heap-delta cycle on **both** boards, the reconnect-replay check, the mid-game multiplayer drop, and the suspend-while-handshaken auto-sleep + remote self-disable cases (step 11).
8. Stress + soak: §8 steps 9, 13, 14, 15 (congestion, overflow paths, WiFi-upload-after-BLE on V2, LoRa+BLE, CW-timing soak incl. ≥35 WpM stalled-client run, snapshots, iOS smoke).
9. Documentation pass: all §9 items, EN + DE in the same session, PDF rebuild, written TODO for `M32 Protocol.pdf`, CLAUDE.md hard-rule line.
10. Final definition-of-done sweep (CLAUDE.md §8): both canonical builds + flag-off build clean, hard rules 2/3/9 re-checked, UX_CONVENTIONS addition committed, merge `ble-serial-protocol` → `master`.
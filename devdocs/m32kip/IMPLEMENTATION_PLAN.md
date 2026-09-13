# M32KIP — implementation plan and open decisions

Status: **Phases 0 and 1 complete, 2026-09-12.** Decisions ratified in `DECISIONS.md` (D2 differs
from the recommendation below). Phase 0 hardware measurements: `PHASE0_RESULTS.md`. Phase 1 outcome
and the three protocol findings it produced: `PHASE1_FINDINGS.md` — **F3 is open as D13 and needs a
decision before the Keyer side is built (Phase 3); the Rig side does not depend on it.** Next:
Phase 2, the Rig unit on the device.
Spec: `MKIP_protocol_spec_02.md` (Draft 0.2, currently in Willi's private notes,
`~/Documents/Privat/Claude/`; decision D10 below is whether it moves into this
directory as `SPEC.md`).

Section numbers in the form §n.n refer to the spec.

---

## 1. What the codebase already gives us, and where it diverges from the spec

These are facts checked against `master` on 2026-09-12. They shape the plan;
several of them are the reason a decision is needed at all.

| Topic | Reality on master | Consequence |
|---|---|---|
| **Toolchain** | Arduino core 2.0.17 on ESP-IDF 4.4 (`espressif32@~6.11.0`), prebuilt sdkconfig. `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD` is **not set** on either SoC; `esp_timer` callbacks run in a task on core 0, the WiFi/lwIP core. There is no GPTimer API in IDF 4.4. | §7.6's "esp_timer in ISR-dispatch mode" is not available. The Rig emitter uses the **hardware timer group through the Arduino `hw_timer` API** (`timerBegin` / `timerAttachInterrupt` / `timerAlarmWrite`), which is a true ISR. Allocated from the Arduino task it lands on **core 1** (Arduino runs on core 1, WiFi and lwIP are pinned to core 0, both variants). No hardware timer is in use anywhere in the firmware today, so group 0 / timer 0 is free. |
| **Key line, source 0** | The iambic keyer is a `loop()` state machine on `millis()`. The TX line is toggled in `keyOut()` → `keyTransmitter()` → `digitalWrite(keyerPin)` (`m32_v6.ino`). Elements carry the known `-6 ms` corrections; the I2S sidetone runs ~6 ms ahead of the key line (open item in `devdocs/cw-timing-audit/FINDINGS.md`). | The capture point for source 0 is the `digitalWrite` itself, timestamped with `esp_timer_get_time()`; §4.3's "≤ 100 µs relative to the key-line edge" is met by construction. But the key line has **millisecond granularity and `loop()` jitter** (touch reads, display draws). The wire faithfully carries that; the remote rig gets exactly what a local rig gets. Acceptance in §13 must therefore compare the Rig's TX-out against the **Keyer unit's own key line**, not against ideal element lengths. |
| **Key line, source 1** | Straight key is polled from `loop()` inside `Decoder::checkInput()` with a `millis()` noise blanker (`nbtime`), reading the paddle-jack tip (`PIN_PADDLE_LEFT`: GPIO 2 on the Pocket, GPIO 13 on the classic V2) OR either touch pad. Touch pads are sampled by `touchRead()`, which cannot be interrupt-driven. | §10.1's interrupt capture is feasible for the **jack only**. A cootie-style key on the touch pads stays loop-sampled (millisecond class) whatever we do. See D2. |
| **Network** | MOPP (`wifiTrx`) uses `AsyncUDP` on port 7373: `audp.onPacket()` callbacks run in the async-UDP task on core 0, sending is `audp.writeTo()` from `loop()`. WiFi credentials: three SSID / password / TRX-peer sets as NVS Strings, set via the on-device AP web form (`Config WiFi`) and the serial protocol (`PUT wifi/...`, password write-only). Entering any WiFi mode suspends BLE Serial. A boot-time WiFi warm-up exists because the first WiFi session fragments the heap; the heap-clearing reboot on WiFi-Trx exit is on branch `reboot-on-wifi-exit`, **not on master**. | Reuse `AsyncUDP` for both ends. Rig: `onPacket` verifies the MAC and pushes edges into the queue under a critical section; the ISR consumes. Keyer: a small send task fed by a FreeRTOS queue so `keyOut()` never touches the socket (§10). The KIP modes inherit the WiFi heap story, including whatever exit strategy `reboot-on-wifi-exit` ends up with. |
| **Crypto / RNG** | `mbedtls/md.h` and `sha256.h` ship in both SDKs; `esp_random()` available. | §9 needs no library additions. |
| **Preferences** | `pliste[]` values are `uint8_t`; a new `prefPos` needs three parallel arrays, a voice clip, and a manual entry (CLAUDE.md §3 rule 10, §8). String settings need their own NVS keys (max 15 chars) and are NVS-costed. | The §11 table cannot be exposed 1:1. Every knob we expose is scaled to fit a byte and paid for three times (code, voice, manual). See D4. |
| **Menu** | `menuNo` is persisted raw and exposed over the serial protocol, so the enum is **append-only**; new leaves are appended at the end and wired into a ring (precedent: Practice Stats, Preview Char). The Transceiver submenu holds LoRa Trx / WiFi Trx / iCW/Ext Trx. Labels are hand-fitted, ~12 chars. | Two new leaves appended and wired into the Transceiver ring. Names and placement: D5. |
| **Mode states** | `morserinoMode` has `wifiTrx`, `loraTrx`, and the safety-isolated `morseGame` / `morseQsoBot`. Every LoRa/WiFi/external-TX gate is a list of states in `keyOut()` and friends. `Key Ext Tx` (`posKeyExternalTx`) decides whether `keyTransmitter()` fires. | Two new states: `kipKeyer` (must be in the **noTx** set regardless of `Key Ext Tx` — §8 "keys nothing locally") and `kipRig` (drives `keyerPin` from the ISR only; `keyOut()` must never touch the pin in that state). |
| **PTT output** | Neither variant has a spare isolated output. Classic V2's GPIO budget is exhausted (pin map in `morsedefs.h`); the Pocket's free S3 GPIOs (35–37) are not on any connector. | See D6. |
| **Unattended operation** | `checkShutDown()` deep-sleeps after `posTimeOut` × 5 min without activity; only buttons/paddle/encoder reset the TOT. | Rig mode must suppress the timeout, or it sleeps mid-session. See D8. |
| **Flash headroom** | Last builds: classic V2 2.05 MB, Pocket 2.34 MB in a 2.95 MB app partition. | Comfortable; not a risk for either variant. |
| **Test assets** | `Software/tests/protocol/` is a Python bench harness for the serial protocol with a hardware-free selftest. Two Pockets are available (MorseGridNet was verified on them). | Model the KIP test tooling on it: a Python reference peer for both ends, with impairment injection built in (D11). |

---

## 2. Implementation plan

Branch: `m32kip` off `master` (major feature, own menu entries → dedicated
branch per CLAUDE.md §1). Feature flag: `CONFIG_M32KIP`, set in both
`heltec_wifi_lora_32_V2` and `pocketwroom` (and the a11y env, which derives from
pocketwroom) — unless D1 narrows the Rig side.

Module layout (new files, all under `Software/src/Version 6 and newer/`):

- `M32Kip.h/.cpp` — the protocol core: packed packet structs (little-endian,
  §6), encode/decode, HMAC (K_base / K_sess derivation), session state
  machines, offset/jitter/D estimator (§7.2–7.3), `dit_est` and idle-gap logic
  (§7.5), the edge queue. **No Arduino or FreeRTOS dependency** so it compiles
  on the host for unit tests.
- `MorseKipRig.h/.cpp` — Rig unit: socket, verify → enqueue, hw-timer ISR
  emitter, watchdogs (§8), STATS, PTT logic (behind `PIN_PTT`), display, mode
  entry/exit.
- `MorseKipKeyer.h/.cpp` — Keyer unit: edge capture hooks (source 0 in
  `keyTransmitter()` / key-off in `keyOut()`; source 1 via GPIO ISR + glitch
  filter), send task + queue, HELLO/BYE, STATS display, warnings.
- `Software/tests/kip/` — Python reference Keyer and Rig, packet/HMAC test
  vectors, estimator tests, impairment injection.

### Phase 0 — Groundwork and the two spikes (short)
1. Create the branch; add `CONFIG_M32KIP` to `platformio.ini`; commit the spec
   into `devdocs/m32kip/SPEC.md` (D10).
2. **Timer spike:** hw-timer ISR on core 1 toggling `keyerPin` under WiFi load
   (a `wifiTrx` session running); measure ISR latency and jitter with a logic
   analyser on both variants. This decides whether the Rig side can ship on the
   classic (§12.1, D1) and validates the emission target (1 ms after D2).
   No analyser is available (D11): the spike measures itself against the
   hardware timer and shows the statistics on the display (`M32KipSpike.cpp`).
3. **Cadence spike:** measure the gap between `loop()` passes under the same
   load (touch reads, display, UDP). With D2 decided for the polled path, this
   is the straight-key capture jitter; it must stay under the 1 ms target.

### Phase 1 — Protocol core, host-testable — **DONE**

> Delivered: `M32Kip.{h,cpp}` (crypto, packet codecs, replay window, speed estimate, edge queue,
> playout and offset tracking, Rig and Keyer state machines — platform-free, compiled into both
> images, not yet referenced by anything) and `Software/tests/kip/` (597 checks under the address
> and undefined-behaviour sanitizers, an independent Python implementation of the wire format
> cross-checking every vector, a reference peer for both roles, and the §13.2 impairment
> simulation). Acceptance met: the two implementations agree byte for byte, and no mark is
> shortened on any profile at any speed. Three findings and one bug came out of the runs —
> `PHASE1_FINDINGS.md`. The plan as written below was carried out unchanged.
- `M32Kip.cpp` as above; Python reference peer; test vectors for every packet
  type and both MAC keys; estimator tests replaying synthetic edge streams
  through jitter, loss, reorder, burst loss and drift (the §13.2 profiles, but
  in software). Acceptance: the Python Rig and the C++ core agree on every
  vector; "never shorten a mark" holds on every profile.

### Phase 2 — Rig unit on device — **DONE (builds; bench test owed)**

> Delivered: `MorseKipRig.{h,cpp}`, a self-contained mode on the QSO Bot pattern — UDP 7374, HELLO/
> HELLO_ACK/NACK(busy), KEY into `RigSession`, STATS once a second, BYE three times on the way out.
> Edges reach the key line from a hardware-timer ISR on core 1 that the run loop pre-arms, never from
> the loop itself. Safety per §8: the max-key-down limit, the keepalive watchdog, a local paddle touch
> and the encoder long-press all lift the key immediately. Wired in as menu leaf `_kipRig`
> ("Remote Rig"), spliced into the Transceiver ring in all four LoRa/QSO-Bot combinations, with the
> new `kipRig` mode state — which appears in none of `keyOut()`'s Key-Ext-Tx cases, so nothing but the
> ISR can drive the pin. Display is rate-limited to 4 Hz and the mode never times out (D8).
>
> **Pulled forward from Phase 4:** the pre-shared key had to have an entry path or the mode could only
> ever display "No key set", so `kipPsk` (NVS, one string, write-only) landed here, with both of D3's
> entry paths: the field on the WiFi configuration web page, and `PUT kip/psk/<pass phrase>` over the
> serial protocol. The protocol one followed because the web form needs a browser on the access point,
> which makes the mode impossible to bench-test over USB. `serialDecode()` lowercases only the type and
> the token, so a pass phrase keeps its case and may contain spaces and slashes. There is deliberately
> no matching GET: the key is write-only, like the WiFi password, so it cannot be read off a device or
> captured in a backup. What stays in Phase 4 is the protocol **version bump to 1.5** and the
> capability advertisement, with the Config Tool and manual work that goes with them.
>
> **Bench-tested 2026-09-13** on the classic, driven from the Mac by `reference_peer.py keyer`: the
> handshake authenticated with a key set over `PUT kip/psk`, `rig_state` bit 0 confirmed the key line
> actually keying, and the Rig's own speed estimate converged to exactly 48.0 ms, a dit at 25 WPM. The
> first run also drove the playout delay to its 600 ms ceiling, which turned out to be the peer
> blocking up to 500 ms per iteration on its receive and emitting edges in late bursts — the Rig
> responded exactly as specified, raising the delay and then decaying it 5 ms every 2 s. Fixed in the
> peer; spec §10's "the keying path never blocks on the socket" now has a demonstration of what
> happens when it does. The clean re-run: **zero late edges, zero underruns, the playout delay held at
> 150 ms throughout, and 68 KEY packets for 68 edges** with no loss. Reported jitter reached 54 ms,
> which is the WiFi-connected Mac rather than the device — the same probe-host effect Phase 0 found.
>
> **Known gap, confirmed by measurement 2026-09-13:** the run loop does not service `serialEvent()`,
> so the serial protocol is dead while Remote Rig runs and the mode can only be left by the encoder
> long-press. Inside one serial session the handshake and `PUT menu/start now/50` both answered, and
> then `GET device` and `PUT menu/stop` both returned nothing. Note the trap that made the first
> attempt at this test meaningless: **opening the port resets the board**, so a script that opens a
> fresh connection is talking to a device that has just rebooted out of the mode and will answer
> happily. The test has to start the mode and query it within one open session. Every other mode is
> reachable over the protocol through `loop()`. For a station meant to sit unattended at a remote site
> that is the wrong way round — `PUT menu/stop` ought to work — so the Keyer-side loop in Phase 3
> should poll it, and this loop should be given the same treatment. The voice clip for
> the new menu entry is done — the extractor needed `CONFIG_M32KIP` adding to its macro set, without
> which the entry would have shipped silent.

- Socket on 7374, HELLO/NACK/session handling, `onPacket` → verify → dedupe →
  enqueue; ISR emitter; keepalive / max-keydown / error-storm watchdogs; STATS
  once per second; local override (encoder long-press = universal exit,
  paddle = terminate); display at ≤ 4 Hz; TOT suppression (D8); menu wiring.
- Test with the Python Keyer, first on the LAN, then through the impairment
  injector. Logic analyser on TX-out.

### Phase 3 — Keyer unit on device — **DONE (builds; keying test owed)**

> Delivered: `MorseKipKeyer.{h,cpp}`, menu leaf `_kipKeyer` ("Remote Keyer") spliced into the
> Transceiver ring beside the Rig, and the `kipKeyer` mode state — absent from every Key-Ext-Tx case in
> `keyOut()`, so the operator's own transmitter output stays down whatever "Key Ext Tx" is set to.
>
> **Edge capture is a single hook in `keyOut()`**, at the exact point the local TX line would be
> toggled. That is what §10.1 asks for source 0, and because the straight-key decoder keys through the
> same function it captures source 1 as well — so D2's polled path needed no separate capture code at
> all. The hook does nothing but timestamp and post to a queue.
>
> **The two rules Phase 0 and Phase 2 turned into requirements are obeyed literally.** A dedicated task
> pinned to core 0 owns the socket, so the keying path never touches it; the Phase 2 bench run showed
> what happens otherwise. The display only redraws in a gap — never while the key is down and never
> within a quarter second of an edge — because a redraw is 37 ms on the OLED and 46 ms on the TFT,
> longer than a dit at any speed worth using.
>
> **The protocol-servicing gap is fixed in both modes.** Each loop now calls `serialEvent()` and honours
> `goToMenu`, so `PUT menu/stop` works. A station meant to sit unattended at a remote site should not be
> deaf to the one link that reaches it.
>
> Also here: the half-limit key-down warning driven by the limit the Rig reports in `HELLO_ACK`, a
> "no link" indication after five seconds without STATS, the host resolved once at entry so no DNS
> lookup can ever land on the keying path, and BYE three times on the way out.
>
> **Verified hands-free 2026-09-13**, classic as Keyer against `reference_peer.py rig` on the Mac:
> both leaves present and executable under Transceiver (Remote Rig 50, Remote Keyer 51); the handshake
> completes against an independent implementation of the wire format, which reports the session id,
> `source` 0 and the 3000 ms key-down limit back; BYE arrives on exit; and the send task runs at
> **4 packets a second**, exactly the 250 ms idle keepalive. `PUT menu/stop` now returns OK and ends
> the mode, which is the gap measured earlier the same day, fixed and confirmed.
>
> **Still owed, and it needs hands on the paddles.** No edge has ever crossed the wire from a real key:
> the run above sent 100 packets and zero edge slots because nobody keyed. So three things remain
> unevidenced on hardware — edge capture through the `keyOut()` hook, D13's *repeat* burst (it only
> fires after an edge, so only its idle half has been seen outside simulation), and the reproduced key
> line at the far end. The last of those is the whole point of the protocol, and the natural test is
> the two devices against each other: one in Remote Rig, one in Remote Keyer, someone sending.
>
> **That pair is now staged and linked, 2026-09-13.** Classic in Remote Rig at 192.168.1.237, Pocket in
> Remote Keyer pointed at it, same pass phrase on both, everything set over USB with nobody touching
> either device. The link was then confirmed **without disturbing it**: a HELLO sent from a third
> address is answered `NACK(busy)`, which is spec §8's one-session-per-Rig rule doing its job — so the
> Rig is listening and its session belongs to the Pocket. That rule had never been exercised on
> hardware before either.
>
> Two practical notes for whoever sets this up next. **Menu numbers are per build, not universal**:
> Rig/Keyer are 50/51 on the classic and 58/59 on the Pocket, whose enum also carries the games and
> Practice Stats, so read them from `GET menus`. And **the Pocket answers the serial protocol perfectly
> well** — it needs `dtr=True`, because native USB CDC only treats the host as present when DTR is
> asserted, whereas the classic's CP2102 wants `dtr=False`. A wrong DTR makes a healthy Pocket look
> dead, which cost this session a wrong conclusion.

- Source 0 hooks; source 1 ISR capture; send task + queue; HELLO retry, BYE ×3
  on exit; STATS display (`D`, loss, quality bar, rig key/PTT state); the
  half-limit key-down warning; "no link" after 5 s without STATS; forced noTx.
- Test against the Python Rig, then device-to-device.

### Phase 4 — Configuration and serial protocol
- NVS: PSK (write-only, ≤ 32 chars → 2 entries), Rig host (D3a), port (D3c).
  Cost each; check `put()` results.
- AP web form field(s) for PSK / host; `GET/PUT kip/...` in the serial
  protocol → protocol 1.5 (D9); the chosen preferences (D4) with their three
  arrays and `spokenName`s.

### Phase 5 — Verification (§13)
Bench loopback with logic analyser at 15/25/40 WPM; impairment profiles;
2-hour drift; safety tests; real Internet via hotspot + port forward;
straight key and bug; tune. Results go to `devdocs/m32kip/TEST_REPORT.md`.

### Phase 6 — Documentation and accessibility
EN + DE manuals (all three variants), `M32 Protocol.md`, `Software/README.md`
change log (which obligates the manual "What is new" sync), voice clips via
the extractor/generator, and a `UX_CONVENTIONS.md` addition for an
**unattended service mode** (Rig mode has no speed/volume, only the exit
gesture — the conventions do not cover that class yet).

Rough size: Phases 0–1 are a few sessions; 2 and 3 are the bulk; 4–6 are
each a session or two. The hardware measurements in Phases 0 and 5 need Willi
at the bench.

---

## 3. Decisions needed before coding

Ranked by how much the answer changes the work. Recommendations are marked;
nothing is assumed until Willi confirms.

**D1 — Rig side on both variants, or Pocket only?**
The Keyer side is timing-tolerant and runs on both. The Rig side depends on
ISR latency under WiFi load on the classic ESP32. *Recommend:* build both under
`CONFIG_M32KIP` and let the Phase 0 timer spike decide; flash headroom is not
an issue on either.

**D2 — How are straight-key / bug edges captured?** *(Decided: option (b), targets relaxed to 1 ms — `DECISIONS.md`.)*
(a) GPIO interrupt on the paddle-jack tip + glitch filter (meets §4.3; new
ISR code, ~1 session); (b) reuse the existing loop-polled decoder path and
timestamp at its `keyOut()` (no new capture code, but millisecond-class jitter
that misses the 0.5 ms target). Touch pads used as a cootie key are loop-polled
in either case. *Recommend:* (a) for the jack, documented millisecond accuracy
for touch pads.

**D3 — Configuration surface.**
(a) Rig host: reuse the selected WiFi entry's **TRX Peer** field (already in
the web form, the protocol and the config tool; MOPP peers use it the same
way) vs. a new dedicated field. (b) PSK entry: AP web form + serial protocol,
write-only like the WiFi password, vs. additionally on-device text entry
(`MorseTextEntry`) for a ≥ 12-char passphrase. (c) Port: fixed 7374 vs.
user-settable. *Recommend:* reuse TRX Peer; web form + protocol only; fixed
7374 with a protocol-only override.

**D4 — Which of the §11 parameters become on-device preferences?**
Each one costs a `prefPos` (three arrays), a byte-scaled range, a voice clip
and a manual entry. *Recommend:* Rig: Playout Delay (10 ms units, 0 =
adaptive), Max Key-Down keyer and manual (seconds), PTT Lead (ms, 0 = off),
PTT Hang (100 ms units). Keyer: Glitch Filter (1–5 ms). Everything else
(redundancy 4, keepalive 250 ms, safety 20 ms, D range, keepalive timeout)
compile-time constants in `M32Kip.h`.

**D5 — Menu naming and placement.**
Two leaves appended to the enum and wired into the Transceiver ring, next to
WiFi Trx. Labels ≤ 12 chars; suggestions: `Remote Keyer` / `Remote Rig`. Also
whether the working name "M32KIP" appears anywhere on the device.

**D6 — PTT.**
No isolated PTT output exists on either variant. Options: (a) implement the
protocol fields and the Rig logic behind a `PIN_PTT` build flag that is unset
in both shipped envs (Rig reports "PTT unavailable"; a future reduced Rig board
per §12.2 gets it for free); (b) drop PTT from v1 entirely; (c) designate a
pin and connector now. *Recommend:* (a).

**D7 — "External keyer" as a new keyer-mode option (source 2)?**
Adding it touches the `posCurtisMode` option list, the top-bar mode letter,
a voice clip and the manual. The spec says it is functionally source 1 with a
possibly shorter filter. *Recommend:* defer; source 1 covers it.

**D8 — Unattended behaviour of the Rig unit.**
Suppress the deep-sleep timeout entirely in Rig mode? Warn or refuse when on
battery? Keep listening for a new session after BYE or timeout (yes, I assume)?
Follow `reboot-on-wifi-exit` on leaving the mode? *Recommend:* timeout off,
battery warning on entry, always return to listening, exit strategy follows
whatever that branch settles on.

**D9 — Serial protocol and the tools.**
`GET/PUT kip/...` means protocol 1.5, a Config Tool update and an iOS-app
update (the app was just submitted). Alternative: web form only in v1.
*Recommend:* protocol in v1 (cheap firmware-side); tool and app as follow-ups,
tracked in `devdocs/m32kip/`.

**D10 — Where the spec lives.**
Commit Draft 0.2 as `devdocs/m32kip/SPEC.md` in the public repo so it is
versioned with the code, or keep it private and reference it? (The repo is
GPL; the protocol is meant to be open.)

**D11 — Test equipment.**
A logic analyser or two-channel scope is required for §13.1 and §13.6; is one
at hand? For network impairment I propose the Python reference peer injects
delay, jitter, loss and reorder itself, which removes the need for a Linux box
with `tc netem` (macOS `dnctl` has no jitter). Confirm that is acceptable as
evidence for §13.2.

**D12 — Spec clarifications to settle now (small).**
(a) §7.6: the timer mechanism is the hw-timer ISR, not `esp_timer` (see §1
above) — amend the spec. (b) §4.3 / §13.1: acceptance is relative to the
Keyer unit's own key line, which has millisecond granularity — amend. (c) §6.1
says bad-MAC packets are silently dropped, but `NACK` reason 3 = auth exists;
one of the two should go (silent drop is the safer choice, a wrong PSK is then
diagnosed on the Keyer by the HELLO timeout). (d) §8: the Keyer unit must
force noTx irrespective of the `Key Ext Tx` preference — state it.

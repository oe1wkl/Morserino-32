# iOS config app — design notes

*Scaffold created 2026-08-17 on branch `ios-config-app`. Build and run
instructions live with the code, in
[`Software/iOS/M32Config/README.md`](../../Software/iOS/M32Config/README.md);
this file records **why** it is built this way and which firmware behaviours it
is pinned to.*

## Why an app at all

iOS Safari has neither Web Serial nor Web Bluetooth, so the existing config tool
cannot reach the device from a browser on an iPhone under any circumstances. A
native shell is the only way in. (A third-party browser such as Bluefy shims Web
Bluetooth on top of WKWebView and would run the tool as-is if the tool grew a
Web Bluetooth transport — a cheaper option that stays open, and is orthogonal to
this app.)

## The decision that makes this small

**The M32 serial protocol is transport-agnostic, and so is the tool.** Of the
~2670 lines in `m32_config_tool.html`, the USB-specific part is about fifty:

| Global | Role |
|---|---|
| `sendLine(t)` | `writer.write(t + '\n')` |
| `readLoop()` | appends decoded text to `readBuffer` |
| `openTransport()` | opens the link — the tool's deliberate transport seam |
| `doDisconnect()` | closes writer/reader/port, every step null-guarded |

Everything else — preferences, snapshots, CW memories, WiFi credentials, Koch,
file upload/download, the TTY — is *send a line, read a JSON object*. Even file
transfer is in-band (`put file/begin` → base64 `put file/data` → `put file/end`),
so nothing in the tool needs USB specifically.

So the app supplies a `writer` object and feeds `readBuffer`, and the tool works
unchanged. Opening the link is the one genuinely transport-specific step, and
the tool factors it into **`openTransport()`** precisely so another host can
replace that one function; `Resources/Web/bridge.js` does exactly that.

`bridge.js` originally replaced the whole of `doConnect()` with a copy, and that
copy drifted within one release: protocol 1.4 added `await loadCapabilities()`
to `doConnect()`, the copy never got it, and the app would have quietly probed
for every feature it could have asked about once. The seam (added on master in
`a242d39`) exists so that cannot recur — **never go back to overriding
`doConnect()`**.

**Consequence to protect:** the tool is not forked. `sync-webtool.sh` copies it
from `Software/Utilities/` at build time and the copy is git-ignored. If someone
later "fixes something for iOS" inside the copied HTML, that property is gone.
iOS-specific behaviour belongs in `bridge.js`.

## Firmware contracts this client is pinned to

From `MorseBleSerial.cpp` and [`devdocs/ble-serial/DESIGN.md`](../ble-serial/DESIGN.md):

- **NUS UUIDs** `6E400001/2/3-B5A3-F393-E0A9-E50E24DCCA9E`. The 128-bit service
  UUID is in the ADV PDU and the name rides in the scan response, so
  `scanForPeripherals(withServices:)` is the correct way to find the device —
  scanning for all peripherals and filtering by name would be wrong and slower.
- **Static random address**, deliberately distinct from the BT-keyboard identity.
  It is derived from the BT MAC, so it is stable across reboots — which is why
  `retrievePeripherals(withIdentifiers:)` is a valid fast path, and why the app
  remembers the last device in `UserDefaults`.
- **No bonding, one central.** No pairing UI; a second phone simply cannot connect.
- **One completed line per poll of `loop()`** (`bleSerialEvent()`). Commands must
  be strictly serialised — one in flight, awaited. `M32Client.RequestGate`
  enforces this natively; the web tool's own `await sendAndParse(...)` already
  did. Do not add pipelining.
- **400-character inbound line cap** on BLE (`m32_v6.ino`, `BLE LINE TOO LONG`).
  The tool's 180-byte upload chunk becomes ~255 characters of base64 plus the
  command prefix, which fits — do not raise `CHUNK_SIZE`.
- **Notifications are paced**: at most 2 chunks per pump pass, fewer than 4 in
  flight, and a torn multi-KB reply is documented behaviour that the client is
  expected to recover from by re-issuing the GET. Hence the retry in
  `M32Client.request` and the blanket timeout multiplier in `bridge.js`.
- **WiFi bring-up suspends BLE Serial.** No command the tool sends today starts
  WiFi (the WiFi tab only stores credentials), but a future "connect now" button
  would drop the app's link underneath it.

## Choices worth knowing about

- **Writes are chunked to the write-*without*-response limit (ATT_MTU − 3) but
  sent *with* response.** Staying under that limit keeps every write one ATT
  packet, avoiding ATT long writes (Prepare/Execute) — a path the firmware's RX
  ring has never been exercised on. Using `.withResponse` keeps CoreBluetooth's
  ordering and backpressure for free.
- **The central manager runs on the main queue**, so every callback and every
  `@Published` mutation is on the main thread with no further locking. Moving it
  to a background queue would invalidate that.
- **Swift 5 language mode.** Swift 6 strict concurrency against a delegate-based
  C API is a fight that buys this app nothing.
- **Receiving is multicast, sending is not.** `M32BleTransport.addReceiver` lets
  the web bridge and the native client both watch the byte stream, but only one
  may issue commands — two drivers would read each other's replies. The Link
  test tab is disabled while the Config tab holds a connection.
- **A custom URL scheme (`m32app://local/`), not `file://`.** WKWebView gives
  `file://` pages an opaque origin and the tool's `fetch('m32_pref_help.json')`
  would fail.
- **`bridge.js` is injected into the page content world**, not an isolated
  world. It has to see and replace the tool's own globals; in an isolated world
  it would run without error and do nothing.

## Open questions

1. ~~**Is BLE fast enough for the Preferences tab?**~~ **Answered.** The Link
   test measured a small command at ~0.06 s and `get configs` at 0.12 s, so the
   transport was never the problem — the ~60 sequential `get config/<name>`
   round trips were. Protocol **1.4** added the bulk read (`GET configs/details`,
   `devdocs/protocol-audit/PROTOCOL_1.4_DESIGN.md`) and the tab now loads in
   about a second.
2. ~~**Distribution.**~~ **Decided:** the Apple Developer Program, heading for
   the App Store. Store copy and the reviewer notes that answer guidelines 2.1
   and 4.2 live in `Software/iOS/M32Config/store-listing.md`. TestFlight remains
   available on the same enrolment if a beta round is ever wanted.
3. **Does it want a native UI eventually?** The tool's eleven-tab desktop layout
   is cramped on a phone. The transport and `M32Client` are deliberately free of
   any WebKit dependency so a native UI can be grown incrementally beside the
   web tool rather than as a rewrite.
4. **VoiceOver.** A native UI would inherit iOS screen reading, which is a
   genuinely better configuration experience for blind operators than the device
   menus — worth weighing against the accessibility edition's own voice clips
   (see [`devdocs/audio-accessibility/HANDOFF.md`](../audio-accessibility/HANDOFF.md)).

## The Trainer tab (added 2026-11-24, branch `ios-trainer-tab`)

Willi chose (2026-11-24) to bring the **M32 Trainer** (`Software/Utilities/m32_trainer*`) to the
iPhone as a **tab in this app**, not as a second app: the firmware accepts one BLE central, so two
apps would fight over the device, and a second near-identical web-wrapper app risks App Store
guidelines 4.2/4.3. The app's name ("M32 Config") is unchanged for now — renaming it is Willi's call.

**Same rule as the Config tab: the page is not forked.** `sync-webtool.sh` copies the four trainer
files (page, link, engine, corpus) into `Resources/Web` at build time; they are git-ignored there.

**The trainer has its own host seam, so its bridge is not a patch.** The Config Tool's globals have to
be replaced after its scripts ran (`bridge.js`, document end). The trainer instead looks for
`window.M32Host` while its scripts run (contract at `---- host` in `m32_trainer.html`), so
`trainer-bridge.js` is injected at **document start** and only defines that object:

| `M32Host` | What it does in the app |
|---|---|
| `openTransport()` | `callNative('connect')`, then returns a transport as the link defines it (`write`/`close`/`onData`/`onClose`, `maxLine: 400` — the firmware's BLE line cap, `label: 'Bluetooth'`). No `reopen()`: on a loss the page ends the session and offers Connect again, with `err.hint` saying why ("BLE serial off" / "suspended" notices are recognised as in `bridge.js`). |
| `linkOptions` | `{timeScale: 3}` — the link stretches every wait it makes (command timeouts incl. handshake and probe, the late-reply grace, the framer's stall rule), as `bridge.js` does with `BLE_TIMEOUT_FACTOR`. The wait for a person (`CONSENT_MS`) is not stretched. |
| `saveFile()` | History export through the share sheet. With `wait: true` native answers when the sheet closes, and only "ok" if the file went somewhere: a cancelled export does not reset the export reminder. |
| `texts` | Replaces the `[data-host]` parts of the page that talk about USB cables, browsers, Ctrl+Enter and the Config Tool link. |

Trainer-page changes made for this (they also help iPad Safari users): `autocorrect="off"` on the
answer field (Safari "corrected" TNX to Thanks), a `confirm` handler for BLE consent ("press FN"),
and a lost-link path for a transport that cannot reopen itself.

**One link, one driver — and it follows the tab on screen.** Receiving was always multicast, sending never. With two web pages the
transport now records who holds the link (`M32BleTransport.driver`: "Config", "Trainer", "Link
test"). A page's Connect claims it, and is refused with "The … tab is connected" while another tab
holds it. Bytes, log lines and link-lost events reach only the holder: otherwise the Config Tool's
TTY would show the trainer's traffic, and its link-lost notice would pop up in a tab that was never
connected. `disconnect` from a page that does not hold the link does nothing. A page that already
holds a live link and connects again (reload, web-content restart) gets the same link back and
re-handshakes. The driver is cleared by `disconnect()` and by a real loss — not by the
`didDisconnect` that follows our own disconnect, because another tab may have claimed the link by
then.

**Hand-over (Willi, 2026-11-24, after the first phone bench).** First version: the user had to
disconnect in one tab and connect in the other — he connected in Config, switched to Trainer, saw
"Not connected". Of three options (Connect takes over / say so on the page / the link follows the
tab) he chose **the link follows the visible web tab, with a safety net: never in the middle of
something.** `LinkHandover.swift` drives it:

1. Showing a web tab that does not hold a connected link asks the holder to let go
   (`__m32Native.handover()`); the shown tab's bar says "Waiting for the … tab to finish…".
   Link test and Help move nothing.
2. The holder finishes first. **Config** (`bridge.js`): quiet for 1.5 s — nothing sent, no reply
   awaited, no Loading overlay, not connecting; a `confirm()` blocks the page's scripts, so it
   waits for the answer too. **Trainer** (the page's `handOver()`): the Connect steps, every
   command, and an item that is sounding; nothing new starts meanwhile; then the session pauses
   (an item being answered comes back later) and `put cw/practice/off` ends the practice settings
   (`endPractice()`: practice, spacing, display).
3. The holder detaches **without ending the M32's session**: Config with `doDisconnect(true)` (its
   "link gone" teardown, which sends nothing), Trainer with `link.disconnect({keepSession: true})`;
   neither drops Bluetooth. This matters: after `put device/protocol/off` a new BLE session needs
   FN consent again (unless within 60 s of the last grant) and outside the main menu the M32
   refuses it with DEVICE BUSY (`m32_v6.ino`, `bleSerialEvent()`); `put device/protocol/on` on an
   open session is simply answered, in any mode.
4. It reports `released`; native gives the link to the tab on screen and calls its `attach()`,
   which connects as the Connect button does (queued until the page's bridge said `ready` — a
   tab's web view is created the first time it is shown).
5. Back on the holder before it let go: `cancelHandover()`, it keeps the link
   (`handoverCancelled`). After it let go, the link simply comes back to it.

An explicit Disconnect still ends everything (protocol off, Bluetooth down) and nothing follows.
The hidden tab's timers are throttled by WebKit, so letting go may take a second longer.

**The screen stays on while the Trainer holds the link** (`isIdleTimerDisabled`, ContentView). Keyed
answers come from the paddle, so nobody touches the phone for minutes, and Auto-Lock would suspend
the app and drop the link mid-item. Only while the Trainer is connected — never for the Config tab.

**Links out of a page open in Safari** (both tabs, `WKNavigationDelegate` + `createWebViewWith`):
followed inside the web view they replaced the page with no way back, and `target="_blank"` links
(the manual, the word list's licence) did nothing. A tab only ever loads its own page.

**Open until benched on a phone** (`BENCH_TRAINER.md`): (IndexedDB persistence under `m32app://`:
confirmed by Willi 2026-11-24), the hand-over, whether the keyboard stays up between items (iOS shows it only for a focus inside a user
gesture), BLE timing of `cw/done` and keyed echo, and what a locked screen or a switch to the other
tab does to a running session.

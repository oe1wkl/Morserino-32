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

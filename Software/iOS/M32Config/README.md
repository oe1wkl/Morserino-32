# M32 Config — iPhone app (scaffold)

An iOS app that does what `m32_config_tool.html` does, but over **Bluetooth LE**
instead of USB. It works by hosting the *unmodified* web tool in a `WKWebView`
and replacing only its transport with CoreBluetooth.

```
   SwiftUI shell
        │
        ├── WebToolView ──── WKWebView ──── m32_config_tool.html   (unchanged)
        │                        │                 ▲
        │                        │   bridge.js ────┘  swaps `writer`, `openTransport`
        │                        ▼
        └── M32BleTransport ── CoreBluetooth ── Nordic UART ── Morserino-32
                    │
                    └── M32Client (native request/response — used by the Link test tab)
```

The web tool is **not forked**. `sync-webtool.sh` copies it out of
`Software/Utilities/` at build time, so the browser version and the app version
can never drift apart.

> **Status: scaffold, never compiled.** It was written on a machine with only
> the Command Line Tools installed, so no iOS target could be built. The
> platform-neutral half (`M32BleTransport`, `M32Client`, `BundleSchemeHandler`)
> *does* type-check against the macOS SDK, and `bridge.js` parses; the SwiftUI
> files and the whole app have never been through a compiler. Expect to fix a
> few small things on the first build.

## What you need

- **Xcode**, free from the Mac App Store (it is a large download — allow an hour).
  After installing, open it once so it can finish setting itself up.
- An **iPhone or iPad** with a Lightning/USB-C cable. The iOS Simulator has **no
  Bluetooth at all**, so it cannot be used to test this app — you need real hardware.
- A **free Apple ID**. That is enough to run the app on your own device. The
  $99/year Apple Developer Program is only needed for TestFlight or the App Store.
- A Morserino-32 running **V9 or newer** firmware.

## Building it

```bash
brew install xcodegen
```

```bash
cd "Software/iOS/M32Config" && ./sync-webtool.sh && xcodegen generate && open M32Config.xcodeproj
```

Then in Xcode:

1. Select the **M32Config** target → **Signing & Capabilities**.
2. Set **Team** to your Apple ID (*Add an Account…* if it is not listed yet).
3. If it complains that the bundle identifier is taken, change
   `cc.kraml.m32config` to anything unique.
4. Plug in your iPhone, pick it in the device menu at the top, and press ▶.

The first install fails on the phone with *"Untrusted Developer"*. On the phone:
**Settings → General → VPN & Device Management → your Apple ID → Trust**. Then
press ▶ again.

With a free Apple ID the app **stops working after 7 days**; re-running it from
Xcode renews it. That is Apple's rule for free accounts, not a bug here.

<details>
<summary>Creating the project by hand instead of using XcodeGen</summary>

File → New → Project → iOS → App; interface SwiftUI, language Swift. Then:

- Delete the generated `ContentView.swift` and `…App.swift`, and drag in
  everything from `Sources/`.
- Drag `Resources/Web` in and choose **Create folder references** (blue folder,
  *not* yellow group) — the code looks for a real `Web` directory in the bundle.
- Set the deployment target to iOS 16.
- In the target's **Info** tab add **Privacy - Bluetooth Always Usage
  Description** with a sentence explaining the app talks to the Morserino.
  Without it the app is killed the instant it touches Bluetooth.
</details>

## On the Morserino side

In the device preferences set **Bluetooth Use → BLE Serial**. The device then
advertises the Nordic UART service, and the app can find it. Nothing else is
needed: no pairing, no PIN — the firmware uses no bonding.

Only **one** central at a time. If your Mac still has the tool open over USB
that is fine, but a second phone will not get in.

## First run: use the Link test tab

Before trusting the Config tab, run the **Link test**. It connects, handshakes,
and times `get hardware` and `get configs`, which is the honest measure of
whether BLE is pleasant or merely possible on your device. The firmware paces
notifications deliberately (at most two chunks per poll, fewer than four in
flight), so everything is slower than USB — the question is by how much.

Only one side may drive the conversation at a time: disconnect in the Config tab
before running the link test.

## Known rough edges

- **The Preferences tab will be slow.** The tool fetches every preference's
  detail individually — around sixty round trips with a 50 ms pause between them
  ([`loadPreferences`](../../Utilities/m32_config_tool.html)). That is tolerable
  over USB and tedious over BLE. The obvious fix is a protocol addition (a bulk
  detail read), not a client-side workaround.
- **All timeouts are multiplied by 3** in `bridge.js` (`BLE_TIMEOUT_FACTOR`)
  rather than tuned per command. Revisit once the link test gives real numbers.
- **Device picking is automatic**: strongest signal after a short settle window.
  With two Morserinos on the bench you get the nearer one, with no say in it.
  `M32BleTransport.discovered` already publishes the full list — a picker sheet
  is maybe thirty lines of SwiftUI.
- **No background mode.** Lock the phone and the link drops. Fine for a
  configuration tool; wrong for anything that wants to keep logging.
- **The file upload path is untested over BLE.** It should work — the base64
  chunks are 180 bytes, comfortably under the firmware's 400-character line
  limit — but nobody has pushed an MP3 through it yet.

## File map

| File | What it is |
|---|---|
| `Sources/M32BleTransport.swift` | The BLE byte pipe. Scan, connect, notify, chunked writes. |
| `Sources/M32Client.swift` | Native request/response + JSON assembly. Not used by the web tool. |
| `Sources/WebToolView.swift` | `WKWebView` host and the JS ⇄ CoreBluetooth bridge. |
| `Sources/BundleSchemeHandler.swift` | Serves the bundled tool under `m32app://local/`. |
| `Sources/ContentView.swift` | Two tabs: the tool, and the link test. |
| `Resources/Web/bridge.js` | The ninety lines that re-point the tool at Bluetooth. |
| `sync-webtool.sh` | Copies the tool out of `Software/Utilities/`. |

Design rationale and the firmware constraints this all has to respect:
[`devdocs/ios-app/DESIGN.md`](../../../devdocs/ios-app/DESIGN.md).

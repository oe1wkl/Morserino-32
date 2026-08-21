# M32 Config — iPhone app

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

> **Status: working, bench-tested, not yet submitted.** It builds, signs and
> runs on an iPhone against real hardware. Exercised on the bench: connecting
> and the on-device consent prompt, reading and changing preferences, the File
> Builder end to end, and the power/battery reporting. The App Store groundwork
> below is done; the submission itself has not been made.
>
> Untested: pushing a large file (an MP3) to the device over BLE.

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
cd "Software/iOS/M32Config" && cp -n Local.xcconfig.example Local.xcconfig; ./sync-webtool.sh && xcodegen generate && open M32Config.xcodeproj
```

The `cp -n` is there because `xcodegen generate` refuses to run without
`Local.xcconfig`; it never overwrites an existing one, so it is safe to leave in
the command you run every time.

Then in Xcode:

1. Select the **M32Config** target → **Signing & Capabilities**.
2. Set **Team** to your Apple ID (*Add an Account…* if it is not listed yet).
   Then put the Team ID into `Local.xcconfig` so it survives the next
   `xcodegen generate` — the file is git-ignored and exists precisely for this.
   `security find-identity -v -p codesigning` prints it in parentheses.
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

- **All timeouts are multiplied by 3** in `bridge.js` (`BLE_TIMEOUT_FACTOR`)
  rather than tuned per command. Measured on an M32 Pocket: a small command
  round-trips in ~0.06 s and `get configs` returns in 0.12 s, so the margin is
  ample and there has been no reason to tune it per command.
- **Device picking is automatic**: strongest signal after a short settle window.
  With two Morserinos on the bench you get the nearer one, with no say in it.
  `M32BleTransport.discovered` already publishes the full list — a picker sheet
  is maybe thirty lines of SwiftUI.
- **No background mode.** Lock the phone and the link drops. Fine for a
  configuration tool; wrong for anything that wants to keep logging.
- **The file upload path is untested over BLE.** It should work — the base64
  chunks are 180 bytes, comfortably under the firmware's 400-character line
  limit — but nobody has pushed an MP3 through it yet.

## Before submitting to the App Store

Already in place, and verified in the built bundle:

- **`Resources/PrivacyInfo.xcprivacy`** — declares the one required-reason API
  the app touches (`UserDefaults`, reason `CA92.1`, for remembering the last
  device). Collects nothing, tracks nobody. Its own comment says what a future
  version would have to add.
- **`ITSAppUsesNonExemptEncryption = false`** — so App Store Connect stops
  asking at every upload. The app implements no encryption, and the BLE link is
  unencrypted because the firmware uses no bonding.
- **iPhone only** (`TARGETED_DEVICE_FAMILY: "1"`) — declaring iPad would oblige
  a second full set of 13-inch screenshots.
- **The app icon** — `Resources/Assets.xcassets`, built from the M32 wordmark
  (white on near-black). A single 1024×1024 source is enough since Xcode 14;
  `Resources/AppIcon.svg` is what it was rendered from and carries the
  regeneration recipe, including the alpha-stripping step the build needs.

Still to do, roughly in order:

1. **Enrol in the Apple Developer Program** ($99/year). Needed for TestFlight as
   well as the store, so it is unavoidable either way. Decide individual vs
   organization first — an individual listing carries your personal legal name,
   and switching later is a support ticket, not a checkbox.
2. **Screenshots, from real hardware.** The simulator has no Bluetooth, so it
   can never show a connected state. Apple's slot is the 6.9-inch display,
   1320×2868; a 14 Pro shoots 1179×2556 and is refused at upload, so run them
   through `./store-screenshots.sh <folder>` first. Take them all from ONE
   build — a shot from an older build showing "USB · Chrome/Edge" in the
   connection bar is exactly the confusion the bridge now fixes.
3. **A privacy policy URL.** Mandatory for every app, even one that collects
   nothing. A short page on morserino.info is enough.
4. **Reviewer notes and a demo video.** The reviewer has no Morserino, so
   guideline 2.1 ("we could not test your app") is the likely first rejection.
   Say plainly that it configures a physical CW device, and link a video.
5. **Be ready for guideline 4.2** (repackaged website). This app is defensible:
   it loads **no remote content** — the tool is bundled — and its core function
   is native CoreBluetooth doing something no website can do on iOS. Put that in
   the reviewer notes rather than waiting to be asked.

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
| `Local.xcconfig.example` | Template for your signing Team ID; copy to the git-ignored `Local.xcconfig`. |
| `Resources/Assets.xcassets` | App icon. Regenerate from `Resources/AppIcon.svg`. |

Design rationale and the firmware constraints this all has to respect:
[`devdocs/ios-app/DESIGN.md`](../../../devdocs/ios-app/DESIGN.md).

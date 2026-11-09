# M32 Config 1.2 (build 4) — release notes, bench checklist, upload steps

*Prepared 2026-11-09. **Bench passed the same evening** (Willi: snapshots, dialogs and device
switching work, in the web tool over USB and in the app over BLE, on a standard Pocket running
a master build with `c94c865`). App 1.1 (build 2) is the version in the App Store. 1.2 carries the
Config Tool as it is on master: `c94c865` plus the stale-device fix of the same evening (§3);
nothing in the tool was changed for iOS only. Willi archives, uploads and submits.*

## 1. What 1.2 brings in

Config Tool commits since the 1.1 commits (`310514d`, 2026-10-05):

| Commit | What it means in the app |
|---|---|
| `3eddb80` Remote Keying pass phrase card | WiFi tab: card appears when the device lists the `kip` capability (protocol 1.5, V10 firmware with remote keying) |
| `507c5c7` goodbye on tab close | `pagehide` sends `put device/protocol/off` through our writer. WKWebView rarely fires `pagehide` (not when the app is killed), so it does nothing harmful and little useful here; a BLE disconnect ends the device's session anyway |
| `193980c` USB Output → USB Keying | General group now lists **Bluetooth Use** and **USB Keying**. In 1.1 the group asked for `BLT Kbd Output`, a name V9 firmware no longer has, so Bluetooth Use was hidden. It is now editable from the app — see 3. |
| `8298993` link-ends notice | New `linkEnded()` / `doDisconnect(linkGone)` / `#linkNotice`. The USB path (`readLoop`) never runs in the app; the bridge now provides the Bluetooth equivalent |
| `c94c865` snapshot replies | `sendAndParse(cmd, timeout, {expect:key})` skips stray replies; store/recall/clear wait 8 s and report device errors; footer says protocol v1.6 |

`m32_pref_help.json` is unchanged since 1.1.

## 2. Bridge review against the tool

Every global `bridge.js` touches still exists under the same name and with a compatible
signature: `sendLine`, `waitForResponse`, `sendAndParse`, `openTransport`, `doDisconnect`,
`writer`, `readBuffer`, `ttyProcessChunk`, `saveTextFile`, `log`, `setStatus`, plus the
elements `#connectBtn`, `.banner-sub`, `.conn-hint`, `#log`, `#connPlaceholder` and the
File Builder save button. New ones the bridge now uses: `linkClosing`, `showLinkNotice`.

Three things the tool changes broke or left uncovered, all fixed in `bridge.js`:

1. **`doDisconnect(linkGone)` lost its argument.** The bridge's wrapper called
   `origDoDisconnect()` with nothing, so the tool's "link already gone, send nothing"
   teardown turned back into an ordinary one. Now `apply(this, arguments)`.
2. **`sendAndParse()` keeps its own deadline**, `Date.now() + timeout`, unstretched. The
   bridge only multiplied `waitForResponse()` by 3. After a stray reply the remaining time
   was computed from the USB deadline: a stray at 3.5 s left the real answer 150 ms. Now
   `sendAndParse()` gets the stretched timeout and its inner `waitForResponse()` calls pass
   through; every other caller of `waitForResponse()` is stretched as before. Over BLE the
   snapshot store/recall/clear wait is therefore 24 s, `get snapshots` 9 s, `get snapshot/n`
   and `get configs` 15 s — nothing in the tool now times out earlier than in 1.1.
3. **Link loss showed no notice**, and once Bluetooth Use became editable it would have
   blamed range for a drop the user caused. `linkLost()` now does `doDisconnect(true)` and
   shows one of three texts in the tool's `#linkNotice`, chosen from the message the firmware
   sends before it stops BLE Serial (`MorseBleSerial::stopWithNotice`): "BLE serial off"
   (Bluetooth Use changed), "BLE serial suspended: wireless mode" (WiFi), otherwise "lost".
   No notice when the tool expected the drop (`linkClosing`, i.e. factory reset).

4. **Found on Willi's first bench (2026-11-09), in the Swift host, not the bridge:**
   Snapshot Recall and Clear did nothing. `WebToolView` never set a `WKUIDelegate`, and
   without one WKWebView answers every `confirm()` with Cancel and drops every `alert()`,
   silently. It had been like that since 1.0. It affected all eleven `confirm()` guards in
   the tool: snapshot Recall / Clear / Import, factory reset, clearing game scores, CW memory,
   custom characters, practice stats, file delete and both sound-pack buttons, plus the
   snapshot store-failure `alert()`. The Coordinator now implements the alert / confirm /
   prompt panels as native alerts. Build bumped to **4**, so an upload works even if build 3
   went to TestFlight.

**Verified without a phone:** the app builds for a generic iOS device (Release, signed for
team F44LJ6Y4F9, Info.plist 1.2 / 3, bundle carries the current tool and bridge). The real
tool plus the bridge were run in a browser against a scripted mock device standing in for
CoreBluetooth: connect with the consent prompt, capabilities incl. `kip`; a stray OK at
3.5 s followed by the real reply at 4.0 s (**the 1.1 bridge times out here, the new one
succeeds**); a 10 s snapshot store; unexpected link loss (no `protocol/off` sent, notice
shown, cleared on reconnect); Disconnect (`protocol/off` then native disconnect); factory
reset (no notice); `pagehide`. The three notices, including a message split across two
notifications, were checked separately in node.

**Not verified:** anything on real BLE timing, the share sheet, and the firmware side —
that is the bench below.

## 3. Points for Willi (not changed)

- **Help texts missing** for *Bluetooth Use* and *USB Keying*: `m32_pref_help.json` still
  has `BLT Kbd Output` and nothing for the other two. A web-tool change, so not done here;
  it would reach the app at the next build.
- **Bluetooth Use is now editable over the link it controls.** Changing it away from BLE
  Serial in the app ends the connection, and the user must fix it on the device. The new
  notice explains that; whether the tool should also warn before sending, or hide it when
  connected over BLE, is a tool/UX decision (the tool cannot tell the transports apart
  today, the bridge could tell it).
- **The remote keying pass phrase travels over an unencrypted BLE link** (the firmware uses
  no bonding), so in clear text within radio range for the moment it is sent. The consent
  prompt limits who can be connected, not who can listen. Worth a sentence in the manual,
  or a decision that it is acceptable for this use.
- ~~**Tool bug: switching devices kept the old device's data.**~~ **Fixed in the web tool on
  master, 2026-11-09, with Willi's OK** (changelog V. 10.0, Bug Fixes). Connected to a 9.1
  device, disconnected, connected to a 10.0 beta 2 device on the Snapshots tab: the grid
  still showed the first device's three slots and Inspect of slot 3 got `NO SUCH SNAPSHOT`.
  Worse, `prefDetails` survived the disconnect and `loadPreferences()` skips the device
  while it is filled, so Preferences showed the previous device's values and option lists
  until Reload. `doDisconnect()` now clears `prefDetails`/`allPrefs`, the snapshot list and
  grid, and the Preferences, CW Memories, WiFi and Koch content; `doConnect()` reloads the
  tab on screen. Checked in the mock harness: device A (slots 1–3) → Disconnect (grid empty,
  placeholder back) → device B (slots 1–2, without a tab switch); a BLE link loss on the
  Preferences tab clears the cache and the reconnect sends `get configs`; on the Connection
  tab nothing extra is loaded.
- **What's New names "firmware 10.0 beta 3"** for the recall fix. That is my assumption for
  the next beta after `V10.0-beta.2`; correct it if the fix ships under another name.
- **German "What's New"?** The app and its store record are English-only so far. A German
  text is only needed if a German localization is added to the store record.

## 4. Bench checklist (real iPhone, real Morserino)

Use a Pocket on a **master build with `c94c865`** (the recall fix) and, if possible, one
device on **9.1** for the "older firmware" lines. Bluetooth Use = BLE Serial on both.
Install via Xcode ▶ or a TestFlight build of 1.2 (4).

**Connect**
- [ ] Connect → "Confirm on device" → FN → Connected. Log shows `Device capabilities: …`,
      footer says `Protocol v1.6`.

**Dialogs** (build 4 fix)
- [ ] Snapshot Recall and Clear show an OK / Cancel alert; Cancel does nothing, OK acts.
- [ ] Config tab → factory reset asks first (cancel it); Files tab → delete asks first.

**Snapshots tab** (the main reason for this release)
- [ ] List loads and shows the occupied slots correctly.
- [ ] Store into an empty slot: "Storing snapshot n…" overlay, log `Stored snapshot n`,
      the list then shows that slot filled **and the others still filled** (1.1 could show all empty).
- [ ] Store over an occupied slot: same.
- [ ] Inspect a slot: values appear with the comparison against the current settings.
- [ ] Recall a slot whose Theme differs from the current one: the Pocket changes colour at
      once; the Preferences tab (reload) shows the recalled values. Power-cycle, reconnect:
      still the recalled values. *(9.1 device: the recall reports OK, the theme does not
      change until a reboot and the values are not kept — expected, firmware-side.)*
- [ ] Clear a slot: it shows empty, the detail box closes.
- [ ] Export a snapshot: the share sheet appears; save to Files; Import it back into a free slot.
- [ ] Note any `Skipped a stray reply …` lines in the log (harmless, but tell me how often).

**Preferences tab**
- [ ] Loads in about a second; change Tone Pitch, the device follows.
- [ ] General shows Bluetooth Use (and USB Keying on firmware that has it).

**Switching devices** (tool fix)
- [ ] On the Snapshots tab: connect device A, Disconnect, connect device B with other
      snapshots: the grid shows B's slots at once. Same on Preferences: B's values, not A's.

**Link ends** (new notices)
- [ ] Switch the Morserino off while connected → "Bluetooth connection lost …" notice
      (CoreBluetooth can take several seconds to notice). Reconnect → notice gone.
- [ ] Preferences → Bluetooth Use → anything but BLE Serial → "The Morserino turned BLE
      Serial off …". Set it back on the device, reconnect.
- [ ] Config tab → factory reset → device reboots; log says reconnect, **no** "lost" notice.
- [ ] Disconnect button → no notice; the device leaves the protocol session (normal display,
      auto-power-off works again).
- [ ] *(optional)* lock the phone while connected, unlock → the "lost" notice.

**WiFi tab**
- [ ] V10 firmware with remote keying: the pass phrase card shows set / not set; set one →
      "pass phrase set"; the log does not contain the phrase. 9.x firmware: no card.

**File Builder**
- [ ] Build a text with accented letters, send it to the device, the File Player plays it.
- [ ] "Save / Share…" opens the share sheet.

**Link test tab**
- [ ] After disconnecting the Config tab, the Link test still connects and times its GETs.

## 5. Archive and upload (Willi)

```bash
cd ~/Documents/GitHub/Morserino-32/Software/iOS/M32Config && ./sync-webtool.sh && xcodegen generate && open M32Config.xcodeproj
```

1. In Xcode: destination **Any iOS Device (arm64)** → Product → **Archive**.
2. Organizer: check the archive says **1.2 (4)** (1.1 once came out as 1.0 (1); `project.yml`
   now maps the plist version, so it should be right). **Distribute App → App Store Connect
   → Upload**, automatic signing.
3. Optional but what 1.1 did: wait for processing, test the build through **TestFlight** —
   that can be the bench run above.
4. App Store Connect → the app → **+** next to iOS → version **1.2**. Paste "What's New" from
   `Software/iOS/M32Config/store-listing.md`, pick build 4. Screenshots, description and the
   App Review notes carry over; the notes need no change for this version.
5. **Add for Review → Submit**. Choose the release option (1.1: automatic release).
6. If App Review sends a letter: answer it **and resubmit** (see README, the 1.0 lesson).

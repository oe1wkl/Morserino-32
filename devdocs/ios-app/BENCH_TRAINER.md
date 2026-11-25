# Trainer tab — bench checklist

*Branch `ios-trainer-tab` (worktree `~/Documents/GitHub/Morserino-32-ios-trainer`), 2026-11-24.
Design: `DESIGN.md` § "The Trainer tab". Needs firmware 10 (protocol 1.6, BLE Serial) on the M32,
and an iPhone — the Simulator has no Bluetooth.*

## Already verified without a phone (2026-11-24)

- **Hand-over, in the mock** (both real pages + their bridges, mock coordinator, simulated M32): the Trainer
  waits out a sounding item (~0.5 s), sends only `put cw/practice/off`, no `protocol/off`, no native
  disconnect, the M32's session stays open, then `released`; `attach()` reconnects on the same link with a
  re-handshake; a hand-over cancelled while the item sounds leaves the item running (`handoverCancelled`).
  The Config Tool, asked right after Connect, waits until its loading is over (1.7 s), releases without
  `protocol/off`, and `attach()` connects it again; a plain Disconnect still sends `protocol/off` and drops
  Bluetooth. Link: `disconnect({keepSession})` and `idle()` tested (146 tests). Not tested anywhere:
  `LinkHandover.swift` itself — only on the phone.

- Both builds: `node --test Software/tests/trainer/*.test.js` (144 pass, incl. the new `timeScale`
  test, mutation-checked), and the app builds for the iOS Simulator.
- In a browser at 375 px, the real page + `trainer-bridge.js` against a mock of the Swift coordinator and
  the simulated M32 (`fake_m32_browser.js`), every reply split in two as BLE notifications are:
  host texts applied, no sideways scroll; Connect → ready over the bridge (`maxLine` 400,
  `timeScale` 3); a typed item scored and stored; export saved (reminder reset) and cancelled
  (reminder kept, "Not exported: cancelled"); link lost → Connect offered with the Bluetooth hint;
  "BLE serial off" before the drop → the right hint; refusal while the other tab holds the link;
  consent (CONFIRM ON DEVICE, FN pressed after 2.6 s) → "Confirm on the M32", then connected and the
  note cleared (that clearing was a bug, fixed).
- The web build (no host) unchanged: original texts, `timeScale` 1, cable lost → reconnects by itself.

## On the phone

Flash nothing new: any firmware 10 beta works. Build and run from Xcode (`xcodegen generate` first).

1. **First connect.** Trainer tab → Connect, M32 at its main menu. Expect "Confirm on the M32", press FN,
   then "Connected · CW Keyer"; the M32 is in the CW Keyer.
2. **Typed copy, five items** — **works as expected (Willi, 2026-11-25).** Does the keyboard stay up from item to item, or must you tap the field each
   time? (iOS raises it only for a focus inside a tap.) Is TNX left alone (no autocorrect)? Return checks
   and goes on.
3. **Keyed answers** — **works as expected (Willi, 2026-11-25).** (Answer: Keyed on the paddle), five items. Timing feels like USB? Next item goes on
   by itself?
4. **Replay / Pause buttons**, Effective speed, M32 screen Hide — all as on the desktop.
5. **Leave it alone for longer than Auto-Lock** while connected: the screen must stay on. Disconnect: Auto-Lock works again.
6. **Lock the phone by hand** mid-item, unlock after 30 s: what does the page show? (Expected: link lost
   → Connect again; anything else, note it.) **Willi, 2026-11-25: nothing special — after unlocking the
   page is exactly where it was, still connected.** The link survives a short lock, so the "link lost"
   path is not taken there (it is still covered by step 8).
7. **Hand-over (the link follows the tab)** — **works on the phone (Willi, 2026-11-24).** No FN press is expected at any step after the first connect.
   a. Connect in Config, switch to Trainer: it connects by itself ("Connected · CW Keyer"); back to Config:
      it connects by itself and loads its tabs. Repeat a few times, also with the M32 **not** at its main menu.
   b. Start a Trainer item and switch to Config **while it sounds**: the Config bar says "Waiting for the
      Trainer tab to finish…" until the item has sounded, then Config connects. Back in the Trainer: the
      paused item comes back a few items later.
   c. Start a Config file upload (Files tab) and switch to Trainer at once: the Trainer waits for the upload.
   d. Switch Config → Trainer → Config quickly (before the hand-over finishes): Config keeps the link.
   e. Switch to Help or Link test: the link stays where it was. Link test's button stays disabled while
      either tab holds the link.
   f. Disconnect explicitly in one tab: nothing reconnects by itself, the M32 leaves the session.
8. **Switch the M32 off** mid-session: "The Bluetooth connection to the M32 is gone…", Connect works again
   after switching it on.
9. ~~**History survives**~~ — **confirmed by Willi 2026-11-24.** Practise, quit the app, reopen: "N attempts stored" is unchanged.
   If it says "This browser window cannot keep your history", IndexedDB does not work under `m32app://` —
   stop and report.
10. **Export** → share sheet → Save to Files; then Import that file again ("already here"). Cancel the sheet once:
    the "Time for an export" reminder must stay.
11. **Links**: open the help, tap "all Morserino-32 user manuals": Safari opens, the app keeps its page.
12. **VoiceOver** (if time): status and results are read, not talked over the Morse.
13. **Config tab regression**: connect, read preferences, disconnect; File Builder save; the TTY shows only
    the Config tab's own traffic.

## Still owed before a release (not done on this branch)

- **Manuals EN + DE**: the M32 Trainer section says it needs a desktop browser; add "or the M32 Config app on
  an iPhone (Trainer tab), over Bluetooth". Do it when the app version with the tab is released, so the manual
  never points at an app that does not have it yet.
- **App Store**: What's New, store listing (description, screenshots of the Trainer tab), version bump (1.3?)
  — Willi. The reviewer notes (guideline 4.2) gain a second reason the app is more than a web page.
- **Name**: "M32 Config" no longer covers it all — Willi's decision whether to rename.

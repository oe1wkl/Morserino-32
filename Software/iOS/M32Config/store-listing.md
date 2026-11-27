# App Store listing — copy for App Store Connect

Draft text for the store record, kept here so the next release starts from
something rather than from a blank field. Character limits are Apple's; the
counts were checked against them.

---

## App name (30 max)

    Morserino

Renamed from "M32 Config" with the first version that has the Trainer tab (Willi,
2026-11-25). The name is changed in App Store Connect together with that version and
goes live when it is approved; bundle ID, ratings and installs stay. The home-screen
name is `CFBundleDisplayName` in `project.yml`, also "Morserino" (fits in full).

## Subtitle (30 max)

    Config & Trainer for the M32

(Up to 1.2: "Configure your Morserino-32".)

## Category

Primary: **Utilities**. (Education is defensible, but people looking for this
will search by name, and Utilities is where hardware companions live.)

## Promotional text (170 max — editable without a new review)

    Set up your Morserino-32 over Bluetooth, and practise copying Morse with
    the M32 Trainer: your Morserino plays, the app checks what you heard.

## Keywords (100 max, comma-separated, no spaces)

    morse,cw,ham,radio,amateur,telegraphy,keyer,koch,trainer,copy,practice,m32,bluetooth

("morserino" left out: words of the app name are indexed anyway, so the slot went to
"trainer,copy,practice".)

## What's New in This Version (4000 max) — next version (1.3), DRAFT

    Now called Morserino
    The app is no longer only for settings, so it has a new name. Everything you had stays as it was.

    New: the M32 Trainer
    A new Trainer tab trains your copy. Your Morserino plays groups, words, phrases, call signs or prefixes from your Koch lesson; you type what you heard, or key it back on the paddle, and the app checks it, keeps a record of every character and decides what comes next. Characters you miss come back more often. When all of your lesson's characters are going well, the app offers the next lesson.
    Your practice history stays in the app on this phone. Export it now and then — to Files, or by mail — and import it again here or in the M32 Trainer on a computer.
    The Trainer needs firmware 10 or later on the Morserino.

    One connection for both tabs
    Connect once, in either tab: the connection follows you when you switch between Config and Trainer, without asking the Morserino again. It never switches in the middle of something — a setting being written or an item being played is finished first.
    While the Trainer is connected the screen stays on, so it does not lock in the middle of practice.

    Works without internet
    The app no longer loads anything from the internet: its fonts and the File Builder's readers for PDF, Word and EPUB files are now built in, so the File Builder works offline too.

## What's New — 1.2, build 5 (kept for reference)

    Snapshot bundles from your CW course
    A course can now hand out all its practice presets in a single file. On the Snapshots tab, tap "Import Snapshot Bundle…" and pick the file your course gave you, for example from the Files app. The app checks every preset against your Morserino first and shows any problem before anything is written, then stores each preset in its slot. Your own settings are put back afterwards.
    Importing a bundle needs firmware 10.0 beta 3 or later on the Morserino; with older firmware the check stops with "Timeout". The same update also makes the menu list on the Dashboard work over Bluetooth.

    Snapshots you can rely on
    Storing, recalling and clearing a snapshot now waits for the Morserino's answer, and tells you when something went wrong — for example when a snapshot could not be stored because the device's memory is full. Before, the app reported success either way, and a slow store could make the snapshot list look empty.
    With firmware 10.0 beta 3 or later, a snapshot recalled from the app also takes effect at once (a stored colour theme shows immediately) and is kept after the Morserino is switched off.

    Know when the connection ends
    When the Bluetooth connection ends without you tapping Disconnect, the app now says so and why: the Morserino was switched off or went out of range, "Bluetooth Use" was changed away from "BLE Serial", or a WiFi mode took over the radio.

    Help for every preference
    Every current preference now comes with its help text, among them Bluetooth Use, USB Keying, Font Size, Tone Softness, Echo Think Time and the rig control settings. A tap on or near the "?" opens it, and a tap anywhere else closes it; before, only an exact hit on the small "?" worked.

    Bluetooth Use and USB Keying in the preferences
    Both settings now appear under General, on firmware that has them. Keep in mind that this app talks to the Morserino through "BLE Serial": choosing anything else under Bluetooth Use ends the connection, and you set it back on the device itself.

    Switching between Morserinos
    After disconnecting from one Morserino and connecting to another, the tabs now show the new one. Before, the Snapshots and Preferences tabs could go on showing the first device's snapshots and settings.

    Confirmations work
    Recalling or clearing a snapshot, a factory reset, deleting a file and the other actions that ask "Are you sure?" did nothing in the app, because the question never appeared. It now appears, and the action runs when you tap OK.

    Remote keying pass phrase
    On Morserinos with remote keying over the internet (firmware 10.0 beta), the WiFi tab lets you set the pass phrase. The app only ever shows whether one is set, never the phrase itself.

## What's New — 1.1, build 2 (shipped 2026-10, kept for reference)

    Training files with accented letters
    The File Builder now keeps letters with umlauts and accents (ä, é, ñ, å, ø and so on). From firmware 9.1 the Morserino's File Player sends them in Morse code, so a French, German or Scandinavian text no longer loses its letters on the way.
    For older firmware, the new option "Convert accented letters to plain ASCII" writes them the way each language does without them: ä→ae, ö→oe, ü→ue, ß→ss, å→aa, æ→ae, ø→oe, é→e. When the connected Morserino runs firmware older than 9.1, the option is switched on for you.

    Help for the new preferences of firmware 9.1
    Decoder Chars (national letters and the ITU brackets in decoded text) and Word Length (now also "4 or more", for practising longer words) come with their help texts.

## Description (4000 max)

    This app sets up your Morserino-32 from your iPhone over Bluetooth — no
    cable, and no computer — and trains your Morse copy on it.

    The Morserino-32 is an open-source Morse code (CW) training device for
    radio amateurs. Everything you would otherwise change by turning the
    encoder and stepping through menus can be done here instead, on a screen,
    with a keyboard.

    PRACTISE WITH THE M32 TRAINER

    • Your Morserino plays groups, words, phrases, call signs or prefixes
      from your Koch lesson; type what you heard, or key it on the paddle
    • The app checks every character and brings back the ones you miss
    • It offers the next Koch lesson when you are ready, never by itself
    • Your history stays on your phone; export it to Files or by mail
    • The Trainer needs firmware 10 or later on the Morserino

    SET UP YOUR MORSERINO

    • Read and change every preference — keyer mode, speed, tone, spacing,
      Koch lesson and the rest
    • Store and recall snapshots of complete settings
    • Edit the CW memories
    • Set your call sign and operator name
    • Manage the Koch character sequence and your practice sets
    • Combine several text files into one training file and send it across
    • Read practice statistics and game scores, on the models that keep them
    • Send text to be keyed as CW, and watch what the device decodes

    YOU NEED A MORSERINO-32

    This is a companion to the hardware and does nothing on its own. You need:

    • A Morserino-32 — classic or Pocket — running firmware 9.0 or later
    • "Bluetooth Use" set to "BLE Serial" in the device preferences

    CONNECTING ASKS THE DEVICE FIRST

    Anything within radio range can reach a Bluetooth device, so the Morserino
    asks you before it hands over control: it shows "Allow connect?" and waits
    for you to press its FN button. Nothing gets in without you agreeing at the
    device itself.

    No account, no advertising, no data collection.

    Firmware, manuals and the desktop version of this tool: morserino.info

## App Review Information → Notes (4000 bytes max)

Live in App Store Connect since 2026-09-13. App Review's first letter for 1.0
(guideline 2.1 – Information Needed) asked six numbered questions and wanted
the answers both as a reply and in this field, so this is that reply, word
for word. A later version keeps the structure and updates the facts; the
opening two lines belonged to the reply and are gone (removed 2026-11-25).

Apple's limit for this field is 4000 **bytes**, not characters, so every
em-dash costs three. As of 1.3 (2026-11-27): 3,705 bytes (3,697 characters), or
3,783 if App Store Connect stores CRLF line endings (1.0–1.2: 3,810 / 3,891). This
is the one field where the limit bites, so measure after editing, in bytes,
rather than estimating.

**Point 1 assumes a new recording is attached for 1.3** (App Review Information →
Attachment), showing Connect, a setting changed, and a Trainer item played and
checked. 1.0's two recordings (iPhone 14 Pro screen + camera) went with the reply
to that review; if no new one is attached, rewrite point 1.

    1. SCREEN RECORDING

    A screen recording on a physical iPhone is attached. It starts at the home
    screen, then shows Connect, granting the connection at the device, settings
    being changed, and the new Trainer tab: the Morserino plays an item, and the
    typed answer is checked.

    No simulator is involved: the iOS Simulator has no Bluetooth and cannot run
    this app. The app has no accounts, so there is no registration, login or
    deletion flow; no user-generated content; no paid content or features.

    2. PURPOSE AND TARGET AUDIENCE

    The Morserino-32 is an open-source Morse code (CW) training device for radio
    amateurs and for anyone learning Morse code (morserino.info). This app is its
    companion and has no standalone function: it configures the device and runs
    copy practice on it.

    The device holds around sixty settings, reached by turning a rotary encoder
    through nested menus on a screen an inch across. Changing several is slow and
    error-prone; entering a call sign or editing the CW memories is painful that
    way. This app puts all of it on a phone screen with a keyboard, over Bluetooth.
    The audience is people who own a Morserino-32.

    3. SETTING UP AND ACCESSING THE MAIN FEATURES

    No login credentials or sample files are required or possible: there is no
    account system, no server and no sign-in. The only prerequisite is hardware — a
    Morserino-32 on firmware 9.0 or later, with "Bluetooth Use" set to "BLE Serial".
    The app's Help tab gives that procedure step by step and is readable with no
    device present.

    Without a Morserino to hand the app can still be confirmed complete: it
    launches, the whole interface is visible, the Help and Link test tabs work, and
    Connect starts a real Bluetooth scan — the iOS permission prompt appears there —
    which reports no Morserino found.

    4. EXTERNAL SERVICES, TOOLS AND PLATFORMS

    None. No data provider, authentication service, payment processor, AI service,
    analytics, advertising or crash reporting. The app makes no network requests at
    all. Its interface, fonts and libraries are bundled inside the binary, not
    loaded from anywhere. The only external communication is Bluetooth Low Energy
    to the user's own Morserino-32, over the Nordic UART Service. Informational
    links (manuals, project site) open Safari; nothing depends on them.

    5. REGIONAL DIFFERENCES

    None. One English-language build behaves identically everywhere: nothing is
    geo-restricted, and there is no server-side configuration that could differ.
    Bluetooth Low Energy uses the licence-free 2.4 GHz band worldwide.

    6. REGULATED INDUSTRY OR THIRD-PARTY MATERIAL

    Neither applies. The app transmits nothing over radio and controls no
    transmitter: it exchanges settings over Bluetooth with a practice device that
    produces Morse code as an audio tone. No amateur radio licence is needed.

    Apart from open-source components used under their licences (pdf.js, mammoth,
    JSZip, the Lato font; listed in Help), it contains no third-party material: the
    Morserino-32 is my own open-source project, I am its designer and maintainer,
    and the app, its interface and its artwork are mine.

    ON GUIDELINE 4.2

    The bundled HTML interface is the one this project also ships for desktop
    browsers over USB. The app loads no remote content, and its core function is
    native: CoreBluetooth discovers the device, connects to its Nordic UART service
    and carries the protocol — precisely what a web page cannot do on iOS, and why
    the app exists.

    The Trainer tab works the same way: the device plays the Morse code; the app
    tells it what to play over Bluetooth and checks the user's copy.

    PRIVACY

    Nothing is collected or transmitted anywhere; see the bundled privacy manifest.

    Contact: info@morserino.info

## Answers to the questionnaires

**App Privacy → Data Collection:** *No, we do not collect data from this app.*
This matches `Resources/PrivacyInfo.xcprivacy`, and the two must stay
consistent — a mismatch is a rejection.

**Age Rating:** every question *None* / *No*. Result: 4+.

**Export compliance:** not asked per build, because
`ITSAppUsesNonExemptEncryption` is already false in the Info.plist.

**Content Rights:** contains no third-party content. (The bundled open-source code and
font are not "content" in Apple's sense — media, text or data owned by others — and are
used under their licences, shown in Help → Open-source components.)

## Screenshots

**Check the size the slot asks for before converting anything.** App Store Connect
names the accepted sizes in its help text for each slot. For 1.3 (2026-11-27) it
asked for **1206×2622 or 1179×2556** (6.3-inch, portrait or landscape) — exactly
what Willi's iPhone takes, so the originals go in **unchanged**. A set scaled up to
1320×2868 (the 6.9-inch size) was refused there. (Up to 1.2 this note said the
6.9-inch slot was the required one; that is no longer what App Store Connect shows.)

Worth capturing, with a device connected:

1. The Config tab with device information filled in
2. The Preferences tab populated
3. The File Builder with a few parts assembled
4. **Trainer tab** mid-session: an item just checked (the green/red result line) and
   the boxes chart below it
5. **Trainer tab**, the history panel (attempts stored, per-character boxes)
6. The consent step, from the app's side: the app waiting for the button
   press on the device. A photograph of the Morserino showing "Allow
   connect?" belongs in the video, not here — guideline 2.3.3 wants
   screenshots of the app in use, and a picture of the hardware is not one

For the first version named Morserino, put a Trainer screenshot **first**: the first
two or three are what a store search shows, and the Trainer is what is new.

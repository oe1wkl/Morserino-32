# App Store listing — copy for App Store Connect

Draft text for the store record, kept here so the next release starts from
something rather than from a blank field. Character limits are Apple's; the
counts were checked against them.

---

## App name (30 max)

    M32 Config

## Subtitle (30 max)

    Configure your Morserino-32

## Category

Primary: **Utilities**. (Education is defensible, but people looking for this
will search by name, and Utilities is where hardware companions live.)

## Promotional text (170 max — editable without a new review)

    Configure your Morserino-32 from your phone over Bluetooth: preferences,
    snapshots, CW memories, Koch lessons and training files. No cable needed.

## Keywords (100 max, comma-separated, no spaces)

    morse,cw,ham,radio,amateur,telegraphy,keyer,koch,morserino,m32,bluetooth

## Description (4000 max)

    Morserino-32 Config sets up your Morserino from your iPhone over
    Bluetooth — no cable, and no computer.

    The Morserino-32 is an open-source Morse code (CW) training device for
    radio amateurs. Everything you would otherwise change by turning the
    encoder and stepping through menus can be done here instead, on a screen,
    with a keyboard.

    WHAT YOU CAN DO

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
opening two lines belong to the reply and go.

Apple's limit for this field is 4000 **bytes**, not characters, so every
em-dash costs three. As pasted: 3,810 bytes (3,798 characters), or
3,891 if App Store Connect stores CRLF line endings. This is the one
field where the limit bites, so measure after editing, in bytes, rather
than estimating. The two recordings point 1 describes were attached to the
reply; they are not part of this field.

    Thank you for the review. The six points are answered below; this text is also
    in the App Review Information notes.

    1. SCREEN RECORDING

    Two recordings are attached. The first is a screen recording on a physical
    iPhone 14 Pro running iOS 26.6.2. It begins at the home screen with the
    app being launched, then covers Help, the link test, Connect, granting the
    connection at the device, the preferences loading, and settings being changed.

    The second is filmed with a camera so the hardware is visible, since the app
    exists to configure a physical device. It shows the same session, including the
    step performed on the device and the Morserino's display changing as a setting
    is applied from the phone.

    No simulator is involved: the iOS Simulator has no Bluetooth and cannot run
    this app. The app has no accounts, so there is no registration, login or
    deletion flow; no user-generated content; no paid content or features.

    2. PURPOSE AND TARGET AUDIENCE

    The Morserino-32 is an open-source Morse code (CW) training device for radio
    amateurs and for anyone learning Morse code (morserino.info). M32 Config is its
    companion app and has no standalone function.

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
    all. Its interface is HTML bundled inside the binary, not loaded from anywhere.
    The only external communication is Bluetooth Low Energy to the user's own
    Morserino-32, over the Nordic UART Service. Two informational links in the Help
    tab open Safari; nothing depends on them.

    5. REGIONAL DIFFERENCES

    None. One English-language build behaves identically everywhere: nothing is
    geo-restricted, and there is no server-side configuration that could differ.
    Bluetooth Low Energy uses the licence-free 2.4 GHz band worldwide.

    6. REGULATED INDUSTRY OR THIRD-PARTY MATERIAL

    Neither applies. The app transmits nothing over radio and controls no
    transmitter: it exchanges settings over Bluetooth with a practice device that
    produces Morse code as an audio tone. No amateur radio licence is needed.

    It contains no third-party material: the Morserino-32 is my own open-source
    project — I am its designer and maintainer — and the app, its interface and
    its artwork are mine.

    ON GUIDELINE 4.2

    The bundled HTML interface is the one this project also ships for desktop
    browsers over USB. The app loads no remote content, and its core function is
    native: CoreBluetooth discovers the device, connects to its Nordic UART service
    and carries the protocol — precisely what a web page cannot do on iOS, and why
    the app exists.

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

**Content Rights:** contains no third-party content.

## Screenshots

Required slot is the **6.9-inch display, 1320×2868**. Screenshots taken on a
6.1-inch iPhone are 1179×2556 and will be refused at upload; the aspect ratios
are near enough (0.4613 vs 0.4602) that scaling is invisible.

Worth capturing, with a device connected:

1. The Config tab with device information filled in
2. The Preferences tab populated
3. The File Builder with a few parts assembled
4. The consent step, from the app's side: the app waiting for the button
   press on the device. A photograph of the Morserino showing "Allow
   connect?" belongs in the video, not here — guideline 2.3.3 wants
   screenshots of the app in use, and a picture of the hardware is not one

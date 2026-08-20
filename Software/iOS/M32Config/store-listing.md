# App Store listing — copy for App Store Connect

Draft text for the store record, kept here so the next release starts from
something rather than from a blank field. Character limits are Apple's; the
counts were checked against them.

Fill in the two placeholders before submitting: `<CONTACT EMAIL>` and
`<DEMO VIDEO URL>`.

---

## App name (30 max)

    Morserino-32 Config

## Subtitle (30 max)

    Set up your Morserino by CW

## Category

Primary: **Utilities**. (Education is defensible, but people looking for this
will search by name, and Utilities is where hardware companions live.)

## Promotional text (170 max — editable without a new review)

    Configure your Morserino-32 from your phone over Bluetooth: preferences,
    snapshots, CW memories, Koch lessons and training files. No cable needed.

## Keywords (100 max, comma-separated, no spaces)

    morse,cw,ham,radio,amateur,telegraphy,keyer,koch,morserino,bluetooth

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

## App Review Information → Notes

    Morserino-32 Config is a companion app for the Morserino-32, an
    open-source Morse code (CW) training device used by radio amateurs
    (morserino.info). It has no standalone function: it configures the device
    over Bluetooth Low Energy.

    HARDWARE REQUIRED — PLEASE SEE THE DEMO VIDEO

    The app needs a physical Morserino-32 running firmware 9.0 or later, with
    "Bluetooth Use" set to "BLE Serial". We appreciate that the review team
    will not have one, so a video of a complete session is here:
    <DEMO VIDEO URL>

    Without the hardware you can still confirm the app is complete and
    functional: it launches, the full configuration interface is visible, and
    tapping Connect starts a Bluetooth scan (the system permission prompt
    appears at that point) which then reports that no Morserino was found.

    REGARDING GUIDELINE 4.2

    The interface is HTML bundled inside the app — the same interface this
    open-source project ships for desktop browsers over USB. The app loads no
    remote content of any kind: nothing is fetched over the network, and there
    is no web server involved. Its core function is native. CoreBluetooth
    discovers the device, connects to its Nordic UART service and carries the
    protocol. That is precisely what a web page cannot do on iOS, and why the
    app exists at all.

    PRIVACY AND ACCOUNTS

    No account, no login, no in-app purchases, no advertising, no analytics.
    Nothing is collected or transmitted anywhere. The only value the app
    stores is the identifier of the last Morserino it connected to, so it can
    reconnect without scanning again; that is declared in the bundled privacy
    manifest as NSPrivacyAccessedAPICategoryUserDefaults, reason CA92.1.

    Contact: <CONTACT EMAIL>

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
4. The Morserino showing "Allow connect?" — the consent step, which is
   unusual enough to be worth showing

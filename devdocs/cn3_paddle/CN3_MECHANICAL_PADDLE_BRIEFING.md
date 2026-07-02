# Briefing: Mechanical Paddle Support on CN3 (Touch Connector) — M32 Pocket

**Status:** IMPLEMENTED on branch `cn3-mechanical-paddle` (see Implementation notes at end).
Original status was: Feasibility confirmed, not yet implemented.
**Scope:** M32 Pocket only (classic M32 has no CN3). Firmware-only change, no hardware modification.

## Background

The M32 Pocket has two paddle inputs:

- **J1** (3.5 mm jack): mechanical paddle/key → `PADDLE_LEFT` = IO1, `PADDLE_RIGHT` = IO2, active low, internal pull-ups, debounced in firmware.
- **CN3** (JST HC-1.25-3A): capacitive touch paddles → pin 1 = GND, pin 2 = `TOUCH_LEFT` = IO4, pin 3 = `TOUCH_RIGHT` = IO5.

Schematic reference: `Schematic_m32-pocket-wroom-tlv320aic31xx_rev4_2_LoRa` (rev 4.2/4.3, 2025-04-25).

## Feasibility (verified against schematic)

CN3's TOUCH_LEFT/TOUCH_RIGHT nets run directly to IO4/IO5 with **no series components, no coupling caps, no analog front-end**. IO4/IO5 are touch-capable pins (T4/T5) on the ESP32-S3 but are equally usable as plain digital inputs. Neither is a strapping pin. CN3 provides GND on pin 1, so a mechanical paddle (two contacts closing to ground) works electrically as-is.

## Requirements

1. **New hardware-config menu entry** (in the existing hardware configuration menu):
   - Name suggestion: "CN3 Connector" or "Paddle Input"
   - Values: `Touch` (default, current behavior) / `Mechanical`
   - Persist in NVS alongside other hardware config parameters.

2. **Mechanical mode behavior:**
   - Do **not** initialize the touch peripheral / touch FSM on IO4/IO5.
   - Configure IO4/IO5 as `INPUT_PULLUP` (internal ~45 kΩ is sufficient).
   - Route them into the **same debounced active-low paddle read path** already used for PADDLE_LEFT/RIGHT (IO1/IO2). Reuse the existing paddle abstraction — do not duplicate debounce logic.
   - Mapping: CN3 pin 2 = left, pin 3 = right (matches touch assignment). The existing paddle-swap setting, if any, should apply here too.

3. **Touch mode behavior:** unchanged.

## Design decisions already made

- **Explicit mode switch, no auto-detect.** Once the touch FSM runs on a pin, a contact closure to GND reads as huge capacitance; auto-detection is unreliable and not worth the complexity.
- **Touch and mechanical cannot coexist on the same pins** — the menu setting is exclusive per connector.
- Both J1 (mechanical) and CN3-in-mechanical-mode may be active simultaneously; treat them as OR'ed paddle inputs if that falls out naturally from the shared read path, but J1 behavior must not regress.

## Open points / check during implementation

- **Sleep/wakeup:** if touch wakeup is configured anywhere for IO4/IO5, that init must be conditional on the mode. In mechanical mode, use EXT0/EXT1 GPIO wakeup instead if wake-on-paddle is supported.
- **ESD:** CN3 lines have no ESD protection — same as J1, so acceptable; no firmware action, just noted for documentation.
- **Serial protocol / config tool:** if hardware-config parameters are exposed via the USB serial protocol (JSON), add the new parameter there and flag it for the protocol document (per the no-default-authority audit rule: any protocol/firmware/host discrepancy goes to Willi for decision).
- **User manual:** add a short note in the hardware configuration section (EN, and DE translation).

## Standing rules (from CLAUDE.md — apply as always)

- `resetTOT()` on all user paddle activity, including CN3 mechanical input.
- Follow existing UX conventions per `UX_CONVENTIONS.md` for the new menu entry.
- Incremental changes with hardware testing between steps; Willi tests on real hardware.

## Implementation notes (branch `cn3-mechanical-paddle`)

**Config flag:** new named flag `CONFIG_CN3_PADDLE=1`, added to `[env:pocketwroom]`
in `platformio.ini` (inherited by `pocketwroom-lora` and `pocketwroom-170x240`).
Not set on other TFT boards (heltec-vision-master, etc.) — they have no CN3.

**Setting:** `MorsePreferences::cn3Mechanical` (bool), stored under its own key
`cn3Mech` in the `morserino` NVS namespace (modeled on `leftHanded`). Default
`false` = capacitive touch. Read early in `readScreenPref()` (before `checkKey()`
and `initSensors()`).

**Menu:** added a "CN3 Paddle" slot to the boot-time Hardware Config menu
(`hwConf` rotary). The label shows the current value (`CN3: Touch` / `CN3:
Mechan.`). Selecting it opens a Touch/Mechanical chooser (`cn3PaddleConfig()`,
modeled on `confirmDelete()`); on change it persists and reboots so the pins come
up clean. Slot numbering is kept in sync across the label switch, the rotary
modulo and the `.ino` action dispatch via `HWCONF_CN3_SLOT` / `HWCONF_NUM_SLOTS`
in `MorsePreferences.h` (slot index differs between LoRa and non-LoRa pocket
builds).

**Read path:** in mechanical mode, `initSensors()` claims IO4/IO5 as `INPUT_PULLUP`
and skips touch calibration (it runs *after* the `checkKey()` touch probe, which
would otherwise leave the pins in touch mode — so this is the authoritative place
to set the session pin mode); `checkPaddles()` reads IO4/IO5 via
`digitalRead` (active-low) into the same debounced `newL/newR` path, OR'd with the
J1 jack exactly as touch was. Pin mapping matches touch (pin2=left, pin3=right);
dit/dah orientation is handled downstream by `posPolarity`, same as touch. J1 is
unaffected in both modes.

**Menu-entry puzzle (resolved):** `checkKey()` is left **unchanged** — it always
probes IO4/IO5 via `touchRead` regardless of the configured mode. A grounded
mechanical contact reads as a very strong "touch" (well above the S3 threshold),
so the Hardware Config menu is always reachable whatever is physically plugged in,
including the case where the user swaps to a mechanical paddle while the setting is
still on Touch. Electrically safe: the S3 touch front-end is a current-limited
measurement circuit, not a push-pull driver, so grounding a pad only saturates the
reading. The only touch access in mechanical mode is this one-shot boot probe;
the whole running session uses digital reads.

**Sleep/wakeup open point (resolved):** deep-sleep wake is on the FN button
(`ext0`, GPIO0) in `shutMeDown()`, **not** on the touch paddles — so no
mode-conditional wakeup init is needed.

**Serial protocol:** `GET hardware` now also returns read-only
`"cn3Paddle": "touch"|"mechanical"` on the Pocket (`jsonGetHardware`). Documented
in `Documentation/Protocol Description/M32 Protocol.md`. This is an **additive,
read-only** field, consistent with the existing read-only `GET hardware`; it does
not change any command semantics. **For Willi:** flagged per the audit rule — a
candidate to fold into the deferred protocol 1.4 bump (with the other 1.4 items);
left the advertised protocol version at 1.3 for now.

**Manuals:** EN + DE Appendix 1 (Hardware Configuration Menu) updated with a "CN3
Connector" subsection and the new menu option in the list.

**Build status:** both canonical variants build clean (`pocketwroom`,
`heltec_wifi_lora_32_V2`). `pocketwroom-lora` fails to build, but that is a
**pre-existing** breakage on `master` (missing `RadioLib.h` lib_dep for that env),
unrelated to this change.

**Not yet done:** on-hardware testing by Willi (mechanical paddle on CN3: keying,
menu entry, mode switch + reboot, J1 still works, touch mode unchanged).

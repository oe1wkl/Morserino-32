# USB Keying in the standard Pocket image

Issue [#227](https://github.com/oe1wkl/Morserino-32/issues/227) asked for MIDI like a Vail adapter. PR #224 had
USB Output only in separate `pocketwroom-usbio` envs, and §H.7 of `devdocs/TODO.md` listed why those could not
become the standard build. Willi did not want another official build. This design has none.

## The idea

The ESP32-S3 has two USB controllers on one PHY. In core 2.0.17, `CONFIG_TINYUSB_ENABLED` does not depend on
`ARDUINO_USB_MODE`, so a stock `ARDUINO_USB_MODE=1` image can also link TinyUSB, and `USB.begin()` at run time moves
the PHY to the OTG controller.

| State | Port is | Computer sees |
|---|---|---|
| Boot, menu, every mode, CW Keyer with USB Keying Off | HWCDC | Serial port |
| CW Keyer, USB Keying not Off | TinyUSB | Keyboard + MIDI, **no serial port** |

The way back is a restart, the only return path bench-tested. Copying the core's static
`usb_switch_to_cdc_jtag` was not tried. `MorseUsbIO::begin()` hands the PHY back at every boot, because the selection
survives a software restart and deep sleep.

Why this clears the §H.7 blockers: boot stays on HWCDC, so its silicon reset, flashing, the DTR behaviour, the
protocol harness and the port name are all unchanged. A core fork would not have helped: TinyUSB is firmware,
HWCDC's reset is hardware.

Cost: +34.4 KB flash and +12.2 KB static RAM on every Pocket (measured on the spike).

## Decisions (Willi, 2026-11-02)

- **Name:** USB Keying, which was USB Output until this work, never in a release image. Serial Output sends characters, this sends the keying, and "Output" made them sound like rivals (Willi, 2026-11-03). The NVS key `usbOut` and the code's `posUsbOut`/`USB_OUT_*` keep the old word. The all-preferences view lists it right after Serial Output and Bluetooth Use; CW Keyer's own list keeps it with the keyer settings, where it acts.
- **Values:** Off / VBand / MIDI Paddles / MIDI Keyer. The real choice is who forms the elements: paddle
  events (the computer is the keyer) or the M32 as keyer (as for VBand). Default Off. Stored numbers 0-2 unchanged.
- **MIDI Keyer sends notes 0 and 30 together.** Vail's tools take note 0 as the straight key and ignore 30;
  CTR2-style programs use 30. Whether SmartSDR ignores note 0 is unverified.
- **Quick Start into CW Keyer switches as usual.** The restart when leaving must not show the splash or start-up
  screens, and must not run Quick Start again (or the Pocket could never reach the menu): `USB_BACK_REBOOT_MAGIC`,
  on top of the memory-clearing reboot's RTC mechanism.
- **Accessibility Edition includes it**, with clips (MIDI Keyer, MIDI Paddles, USB keying, VBand; Off already had one).
- **Serial loss is accepted** and documented. No on-screen notice.

## Traps found on the bench

- **`periph_module_disable()` alone wraps the 8-bit module reference count** from 0 to 255; `periph_module_enable()`
  then skips the clock and the Pocket reboots on `USB.begin()`. The handback must be enable + reset + disable.
- **An open protocol session at the switch latches no-sleep** (`checkShutDown`), and could never be closed over a
  port that is gone. `startOtg()` sends the farewell and ends it.
- **The menu runs blocking in `menu_()`**, so `tick()` does not run there: the restart hangs off `menuEntered()`.
- **The Pocket's slide switch only disconnects the battery.** A real power cycle needs the USB cable out as well.
- **`PUT config/USB Keying/<value>` is numeric.** A string such as `MIDI` parses to 0.

Also guarded: a paddle or encoder held during the restart would read as the Hardware Config gesture or be calibrated
into the touch sensors, so `menuEntered()` waits for release (up to 2 s) and setup skips the check.

## Vail site settings (from the bench)

Keyer **Straight Key** (on its paddle keyers note 0 is a dit paddle) and Decoder Tolerance **Standard** (Auto picks
Relaxed for manual keys and misreads tight spacing). With paddle notes the site decoded only 80-90 %, because two
keyers ran in parallel. Do not emulate Vail's CC/program-change control; `tick()` only discards those messages.

## Bench status

The spike passed on a standard Pocket, macOS, Chrome (MIDI with note 0 only): entering gives MIDI, leaving restores
serial, sleep then wake restores serial, CW Keyer works again with no reboot, flashing works after OTG sessions. See
`BENCH.md` for what the final code still owes.

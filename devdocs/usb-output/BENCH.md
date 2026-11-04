# USB Keying single image: bench checklist

Spike results are in `SINGLE_IMAGE.md`. Everything unticked is owed on the final code on this branch. Ticks below come from Willi's benches on 2026-11-03 and 2026-11-04, on macOS, on a standard and an Accessibility Pocket.

## Standard Pocket

- [x] All-preferences view: Serial Output, Bluetooth Use, USB Keying in a row. CW Keyer's list: USB Keying among the keyer settings. Config Tool: all three under General, none in Other..
- [x] First boot: USB Keying reads Off. Serial port unchanged, CW Keyer leaves the port alone..
- [x] MIDI Keyer: notes 0 and 30 balanced, in every keyer mode, Straight Key and memories. Vail decodes.
- [x] MIDI Paddles: notes 20/21, Straight Key note 30 only. VBand: vBand decodes, no stuck Ctrl.
- [x] Leaving CW Keyer: no splash, about a second, cursor on CW Keyer, serial back, no stuck note on the computer..
- [x] Encoder held long during exit, finger on paddle during exit: no Hardware Config, touch paddles still calibrated.
- [x] Quick Start on, CW Keyer remembered: MIDI from power-on; leaving stays at the menu. A later power-on runs Quick Start.
- [x] Protocol session open, start CW Keyer (from the Config Tool, then by hand): farewell seen, port goes, sleep still works.
      Config Tool and File Manager must show "Disconnected" with the reason. On 2026-11-03 the Config Tool stayed on "Connected": neither tool handled the farewell or a lost port. Fixed in both; passed 2026-11-04 on a standard Pocket in Firefox with the Config Tool. The File Manager was tested against a simulated port only (goodbye, lost port, reconnect, Disconnect button).
- [x] Pull the cable with the Config Tool connected: "Connection lost", and Connect works again once it is back.
- [x] Sleep from MIDI mode, wake: serial back, then CW Keyer without a reboot.
- [x] Serial Output on while in MIDI mode: keying not delayed.
- [x] BLE keyboard and BLE Serial alongside USB Keying; VBand still blocks the BLE keyboard.
- [x] Games: free heap and largest block against stock, for the +12.2 KB of RAM. Trigger the sprite allocation check.
      Measured 2026-11-04, see "Game memory" in `SINGLE_IMAGE.md`. The first version looped for ever with BLE Serial on; fixed.
- [x] Forced sprite failure (temporary image): one "Clearing memory", then "Not enough memory", back at the menu, no loop. Morse Invaders, Morsel, Radio Cave.
- [x] GET device has hardware and firmware after a "Clearing memory" restart and after leaving CW Keyer with USB Keying on.
- [x] Flash over the port after a MIDI session (macOS, 2026-11-04). Protocol harness on this image: 122 passed, 0 failed.
- [ ] WinKeyer bench still fine.

## Accessibility Edition

- [x] Clips heard: USB keying, Off, VBand, MIDI Paddles, MIDI Keyer. Pronunciation of MIDI and VBand.
- [x] Leaving CW Keyer: no boot speech, the menu position is announced, battery line as usual.
- [x] MIDI and speech at the same time: no audio damage.

## Other hosts

These wait for the second V10 beta, which gives testers access (Willi, 2026-11-04).

- [ ] Linux (the #227 reporter's system) and Windows: enumeration, MIDI, return to serial.
- [ ] SmartSDR on one of Ian's Flex stations: MIDI Paddles, and whether MIDI Keyer's note 0 does harm.

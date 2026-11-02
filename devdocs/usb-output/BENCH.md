# USB Output single image: bench checklist

Spike results are in `SINGLE_IMAGE.md`. Everything below is owed on the final code on this branch.

## Standard Pocket

- [ ] First boot: USB Output reads Off. Serial port unchanged, CW Keyer leaves the port alone.
- [ ] MIDI Keyer: notes 0 and 30 balanced, in every keyer mode, Straight Key and memories. Vail decodes.
- [ ] MIDI Paddles: notes 20/21, Straight Key note 30 only. VBand: vBand decodes, no stuck Ctrl.
- [ ] Leaving CW Keyer: no splash, about a second, cursor on CW Keyer, serial back, no stuck note on the computer.
- [ ] Encoder held long during exit, finger on paddle during exit: no Hardware Config, touch paddles still calibrated.
- [ ] Quick Start on, CW Keyer remembered: MIDI from power-on; leaving stays at the menu. A later power-on runs Quick Start.
- [ ] Protocol session open, start CW Keyer (from the Config Tool, then by hand): farewell seen, port goes, sleep still works.
- [ ] Sleep from MIDI mode, wake: serial back, then CW Keyer without a reboot.
- [ ] Serial Output on while in MIDI mode: keying not delayed.
- [ ] BLE keyboard and BLE Serial alongside USB Output; VBand still blocks the BLE keyboard.
- [ ] Games: free heap and largest block against stock, for the +12.2 KB of RAM. Trigger the sprite allocation check.
- [ ] Flash over the port after a MIDI session. Protocol harness on this image, WinKeyer bench still fine.

## Accessibility Edition

- [ ] Clips heard: USB output, Off (charge), VBand, MIDI Paddles, MIDI Keyer. Pronunciation of MIDI and VBand.
- [ ] Leaving CW Keyer: no boot speech, the menu position is announced, battery line as usual.
- [ ] MIDI and speech at the same time: no audio damage.

## Other hosts

- [ ] Linux (the #227 reporter's system) and Windows: enumeration, MIDI, return to serial.
- [ ] SmartSDR on one of Ian's Flex stations: MIDI Paddles, and whether MIDI Keyer's note 0 does harm.

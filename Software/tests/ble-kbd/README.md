# Bluetooth keyboard (vBand) soak — TODO I2

Automated test of the Morserino's Bluetooth keyboard: the classic M32 plays the computer, the Mac drives both.

| Part | What it is |
|---|---|
| `central/` | PlatformIO project for a **classic M32 / Heltec V2**: a BLE central that connects to "Morserino32 Keyboard", bonds, subscribes to the HID input report, and prints every report and disconnect (with its HCI reason) over USB. It can cut the link on command — at the next Ctrl-down report, the moment a lost key-up would leave a real host keying. **Not Morserino firmware:** re-flash the M32 firmware afterwards. |
| `mac_soak.py` | The same tests with **the Mac as the host** — macOS's own Bluetooth/HID stack, what a vBand user has. Watches the Control key as macOS sees it (what vBand reads) and the keyboard's presence, forces drop-outs with `blueutil`, parks the classic test host meanwhile. Setup and run instructions in its header; run it from **Terminal.app** (Bluetooth permission), overnight (it presses Control on the Mac). |
| `soak.py` | Runs on the Mac. Drives the Morserino under test over the serial protocol (CW Keyer, `PUT cw/repeat`, `PUT cw/stop`) and the test host, and judges: forced-drop cycles (key released on the host after the reconnect = the firmware's re-sync), the exit test (key released before the keyboard is torn down), and a long soak (links lost without being asked, stuck keys). |

```bash
pio run -d Software/tests/ble-kbd/central -t upload --upload-port /dev/cu.usbserial-0001
~/.platformio/penv/bin/python Software/tests/ble-kbd/soak.py --dut /dev/cu.usbmodem12301 \
    --host /dev/cu.usbserial-0001 --drops 20 --exit-test --minutes 100
```

- Neither port is reset by opening it (DTR and RTS asserted), so the Morserino keeps its state.
- The script switches **Bluetooth Use** to VBand Kbd and **Serial Output** to Nothing (so the firmware's
  `BLE kbd: link lost, reason 0x..` lines reach USB) and restores both at the end, also when a step fails.
  If it is killed, restore them by hand.
- A drop cycle is *inconclusive* when the key-up report got through before the cut; those are not counted.
- In the iCloud-synced checkouts, give `central/` a `.pio.nosync` folder behind a `.pio` symlink before building.
- Background runs from Claude Code are capped at 2 hours — keep `--minutes` below ~110.

Observed 2026-10-03: the **very first** pairing between a new host and the Pocket failed (the Pocket hung up,
0x16, and then believed itself connected, so it stopped advertising until CW Keyer was entered again);
every later connection worked. Not reproduced since — it would need the Pocket's stored bonds cleared.

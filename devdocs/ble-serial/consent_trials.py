#!/usr/bin/env python3
"""Fail-closed check for BLE Serial consent (ACCESS_CONTROL.md): an UNANSWERED
handshake request must be declined after the 20 s window, every time.

Hands OFF the device for the whole run. Trials 1..N-1 start from a true hard
reset (esptool; opening the classic's USB port does not reliably reset it),
the last trial runs on the already-booted device.

Usage:  python3 consent_trials.py [trials] [port] [chip]
        defaults: 6 /dev/cu.usbserial-0001 esp32   (Pocket: /dev/cu.usbmodem* esp32s3)
Run from macOS Terminal (Bluetooth permission, see ble_bench_common.py).
Found 2026-09-30: 6/6 declined after 20.2 s, plus one earlier hands-off rerun;
one unexplained admission after 9.5 s in the very first run of the evening.
"""
from ble_bench_common import *

TRIALS = int(sys.argv[1]) if len(sys.argv) > 1 else 6
PORT = sys.argv[2] if len(sys.argv) > 2 else "/dev/cu.usbserial-0001"
CHIP = sys.argv[3] if len(sys.argv) > 3 else "esp32"

async def main():
    cue("Decline trials. Keep your hands off the Morserino until the end")
    for trial in range(1, TRIALS + 1):
        reset = trial < TRIALS
        if reset:
            hard_reset(PORT, CHIP); await asyncio.sleep(12)
        ble = Ble()
        try:
            await ble.connect(timeout=30)
        except Exception as e:
            check(f"T0 trial {trial}: connect", False, str(e)); continue
        ble.stream.objects.clear()
        await ble.send("PUT device/protocol/on")
        t0 = time.time(); got = None; confirm_t = None
        while time.time() - t0 < 35 and got is None:
            for i, o in enumerate(ble.stream.objects):
                if "message" in o and "CONFIRM" in json.dumps(o): confirm_t = time.time() - t0; ble.stream.objects.pop(i); break
                if "error" in o or "device" in o: got = ble.stream.objects.pop(i); break
            await asyncio.sleep(0.02)
        dt = time.time() - t0
        verdict = ("DECLINED" if got and "DECLINED" in json.dumps(got)
                   else "ADMITTED WITHOUT A PRESS" if got and "device" in got else "other: " + json.dumps(got))
        check(f"T0 trial {trial} ({'after hard reset' if reset else 'steady state'}): declined", verdict == "DECLINED",
              f"{verdict} after {dt:.1f}s, prompt announced at {None if confirm_t is None else round(confirm_t, 2)}s")
        await ble.disconnect(); await asyncio.sleep(3)
    sys.exit(summary("trials declined"))

asyncio.run(main())

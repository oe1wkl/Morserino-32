#!/usr/bin/env python3
"""Log a Morserino's USB serial output with timestamps during a Bluetooth keyboard (vBand) soak (TODO I2).

The firmware prints "BLE kbd: link lost, reason 0x.. (n since start, up m min)" on every disconnect - but only
while the preference "Serial Output" is set to "Nothing" (DEBUG output shares the port with the character echo).
HCI reasons: 0x08 supervision timeout (link went silent: radio, or one side stalled), 0x13 the computer hung up,
0x16 the Morserino did, 0x3e connection never established.

    ~/.platformio/penv/bin/python soak_log.py /dev/cu.usbmodem12301 [logfile]

Opens the port with DTR and RTS both asserted, which does NOT reset the board (see Software/tests/protocol/
m32_link.py), so the keyboard session under test is not disturbed. Ctrl-C to stop.
"""
import sys, time, serial

port = sys.argv[1]
log = open(sys.argv[2] if len(sys.argv) > 2 else "ble_soak.log", "a", buffering=1)
s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.5; s.dtr = True; s.rts = True
s.open()
start = time.time()
print(f"logging {port} - Ctrl-C to stop", flush=True)
try:
    buf = b""
    while True:
        buf += s.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", "replace").rstrip()
            if text:
                stamp = time.strftime("%H:%M:%S") + f" +{(time.time() - start) / 60:6.1f} min  "
                print(stamp + text, flush=True); log.write(stamp + text + "\n")
except KeyboardInterrupt:
    pass
finally:
    s.rts = False; s.dtr = False; s.close()    # RTS first: closing must not pass through the reset state

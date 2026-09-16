#!/usr/bin/env python3
"""M32KIP Phase 5 bench driver: Pocket Remote Keyer -> (optional relay) -> classic Remote Rig, hands-free.

The classic must run a `-D KIP_MEASURE=1` build (see devdocs/m32kip/TEST_REPORT.md). The Pocket keys continuously
from its own keyer via `PUT cw/repeat`, so every edge is a real source-0 edge captured at `keyOut()`; the classic
reports every 10 s how exactly it reproduced the Keyer's marks and spaces, and those reports are logged here.

    python3 bench_run.py --wpm 25 --minutes 10 --log clean25.log

Both serial ports stay open for the whole run. That matters on the classic, whose USB-serial chip resets the board
when the port opens: the mode has to be started, and observed, inside one session.
"""

import argparse, re, sys, threading, time
import serial

CLASSIC_RIG_MENU = 50     # read from GET menus; per build, not universal
POCKET_KEYER_MENU = 59


class Port:
    def __init__(self, name, dtr, label, log):
        self.label, self.log = label, log
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = name, 115200, 0.1
        self.s.dtr, self.s.rts = dtr, False
        self.s.open()
        self.buf = ""
        self.lines = []
        self.lock = threading.Lock()
        self.stop = False
        self.t = threading.Thread(target=self.reader, daemon=True)
        self.t.start()

    def reader(self):
        while not self.stop:
            try:
                chunk = self.s.read(4096)
            except Exception:
                break
            if not chunk:
                continue
            self.buf += chunk.decode(errors="replace")
            # protocol objects are brace-framed; split on the closing brace of a top-level object
            while True:
                depth, end = 0, -1
                for i, c in enumerate(self.buf):
                    if c == "{":
                        depth += 1
                    elif c == "}":
                        depth -= 1
                        if depth == 0:
                            end = i
                            break
                if end < 0:
                    break
                obj, self.buf = self.buf[:end + 1].strip(), self.buf[end + 1:]
                stamp = time.strftime("%H:%M:%S")
                with self.lock:
                    self.lines.append((time.time(), obj))
                self.log.write(f"{stamp} {self.label} {obj}\n")
                self.log.flush()
                if "KIPM" in obj or '"error"' in obj or "Remote" in obj or "End:" in obj:
                    print(f"{stamp} {self.label} {obj}", flush=True)

    def send(self, line, wait=1.0):
        self.s.write((line + "\r\n").encode())
        time.sleep(wait)

    def close(self):
        self.stop = True
        time.sleep(0.2)
        self.s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--classic", default="/dev/cu.usbserial-0001")
    ap.add_argument("--pocket", default="/dev/cu.usbmodem12301")
    ap.add_argument("--wpm", type=int, default=25)
    ap.add_argument("--minutes", type=float, default=10)
    ap.add_argument("--text", default="PARIS CODEX THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789")
    ap.add_argument("--log", default="bench.log")
    ap.add_argument("--rig-settle", type=float, default=20,
                    help="seconds to let the Rig boot and join WiFi before the Keyer calls it")
    ap.add_argument("--keep-keyer", action="store_true", help="Pocket already in Remote Keyer: do not restart it")
    args = ap.parse_args()

    log = open(args.log, "a")
    log.write(f"\n==== {time.ctime()} wpm={args.wpm} minutes={args.minutes} text={args.text!r}\n")

    def wait_for(port, pattern, timeout, mark, stop_pattern=None):
        """Wait for an object matching `pattern` after index `mark`; returns "OK", "STOP" or None on timeout."""
        start = time.time()
        while time.time() - start < timeout:
            with port.lock:
                new = [o for _, o in port.lines[mark:]]
            for obj in new:
                if stop_pattern and re.search(stop_pattern, obj):
                    return "STOP"
                if re.search(pattern, obj):
                    return "OK"
            time.sleep(0.2)
        return None

    rig = Port(args.classic, False, "RIG  ", log)
    print("classic: waiting out the reset the port open caused ...", flush=True)
    time.sleep(7)
    rig.send("PUT device/protocol/on", 2.5)
    with rig.lock:
        mark = len(rig.lines)
    rig.send(f"PUT menu/start now/{CLASSIC_RIG_MENU}", 1)
    if wait_for(rig, r"Remote Rig", 20, mark) != "OK":
        # The classic once served a session while saying nothing at all over USB - its protocol session was off and
        # opening the port had not reset it - and the driver then "measured" for ten minutes against a Rig that
        # reported nothing. Positive evidence from the Rig itself, or no run.
        print("the classic never reported Remote Rig starting - protocol off, or the port did not reset it?", flush=True)
        rig.close(); log.close()
        return 1
    print(f"rig started; letting it reach the network ({args.rig_settle}s) ...", flush=True)
    time.sleep(args.rig_settle)

    # Reboot the Pocket before every run. A Remote Keyer stopped and restarted often enough within one boot stops
    # linking altogether: it reports "No answer" while the Rig answers other clients on the same LAN instantly, and
    # afterwards it will not start any mode at all until rebooted. Cheap here (~8 s) and it keeps a measurement from
    # being spent on a Keyer that cannot transmit. See TEST_REPORT.md - this may be a real defect in the Keyer's
    # socket teardown, and it is NOT something a measurement should paper over silently.
    import os, subprocess
    esptool = os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")
    if os.path.exists(esptool):
        print("rebooting the Pocket ...", flush=True)
        subprocess.run([sys.executable, esptool, "--chip", "esp32s3", "--port", args.pocket,
                        "--after", "hard_reset", "chip_id"], capture_output=True, timeout=90)
        time.sleep(6)
    else:
        print("esptool not found - running without the Pocket reboot", flush=True)

    keyer = Port(args.pocket, True, "KEYER", log)
    time.sleep(2)
    keyer.send("PUT device/protocol/on", 2.5)

    # Always restart the Keyer AFTER the Rig, so its announcement is fresh: a message carrying the Rig's address
    # means the handshake completed, "No answer" means it did not. Asking an already-running Keyer to announce
    # itself never worked (it does not re-announce), and reading the Rig's reports failed when its protocol was off.
    linked = False
    for attempt in range(1, 4):
        keyer.send("PUT menu/stop", 3)
        with keyer.lock:
            mark = len(keyer.lines)
        keyer.send(f"PUT menu/start now/{POCKET_KEYER_MENU}", 1)
        got = wait_for(keyer, r"Remote Keyer\d", 30, mark, r"No answer|No rig host|No key set")
        print(f"keyer start attempt {attempt}: {'linked' if got == 'OK' else (got or 'timeout')}", flush=True)
        if got == "OK":
            linked = True
            break
    if not linked:
        print("the Keyer never linked - nothing to measure", flush=True)
        rig.close()
        keyer.close()
        log.close()
        return 1
    keyer.send(f"PUT control/speed/{args.wpm}", 1)
    keyer.send(f"PUT cw/repeat/{args.text}", 1)
    print(f"keying at {args.wpm} WPM for {args.minutes} min", flush=True)

    end = time.time() + args.minutes * 60
    try:
        while time.time() < end:
            time.sleep(5)
    except KeyboardInterrupt:
        print("interrupted", flush=True)
    keyer.send("PUT cw/stop", 1)
    print("stopped keying; waiting for the final report ...", flush=True)
    time.sleep(12)

    with rig.lock:
        reports = [o for _, o in rig.lines if "KIPM" in o]
    print("\nFINAL:", reports[-1] if reports else "no KIPM report received", flush=True)
    rig.close()
    keyer.close()
    log.close()
    return 0 if reports else 1


if __name__ == "__main__":
    sys.exit(main())

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

    def link_state():
        """BUSY while the Rig reports a session, FREE otherwise - read from the Rig's own serial stream.

        The Rig's 10 s measurement report (KIP_MEASURE build) is emitted only inside the session branch, so its
        arrival IS the proof that a Keyer is connected. Two earlier approaches failed: waiting for the Keyer to
        announce itself missed a Keyer that had reconnected by itself (D14) and never re-announced, and a network
        probe run from inside this driver always came back SILENT - on macOS a subprocess inherits this
        interpreter's Local Network attribution, and a third-party python is blocked from the local network.
        """
        now = time.time()
        with rig.lock:
            fresh = [t for t, obj in rig.lines if "KIPM" in obj and now - t < 15]
        return "BUSY" if fresh else "FREE"

    rig = Port(args.classic, False, "RIG  ", log)
    print("classic: waiting out the reset the port open caused ...", flush=True)
    time.sleep(7)
    rig.send("PUT device/protocol/on", 2.5)
    rig.send(f"PUT menu/start now/{CLASSIC_RIG_MENU}", 4)
    # The Rig still has to join WiFi before it can answer a HELLO, and the Keyer gives up after ten tries in 5 s.
    print(f"rig starting; letting it reach the network ({args.rig_settle}s) ...", flush=True)
    time.sleep(args.rig_settle)

    keyer = Port(args.pocket, True, "KEYER", log)
    time.sleep(1)
    keyer.send("PUT device/protocol/on", 2.5)

    # A Keyer left in the mode reconnects on its own once the Rig is back (D14), so give it a moment and ask the
    # Rig before touching anything. Restarting a mode that is already running is what stuck the Pocket twice:
    # menu/stop answered OK, the start was swallowed, and no mode ran at all until the device was rebooted.
    state = "FREE"
    for _ in range(10):
        state = link_state()
        if state == "BUSY":
            break
        time.sleep(2)
    print(f"link after rig start: {state}", flush=True)

    if state != "BUSY" and not args.keep_keyer:
        for attempt in range(1, 4):
            keyer.send("PUT menu/stop", 3)
            keyer.send(f"PUT menu/start now/{POCKET_KEYER_MENU}", 8)
            for _ in range(10):
                state = link_state()
                if state == "BUSY":
                    break
                time.sleep(2)
            print(f"keyer start attempt {attempt}: {state}", flush=True)
            if state == "BUSY":
                break
    if state != "BUSY":
        print(f"the Keyer is not linked ({state}) - nothing to measure", flush=True)
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

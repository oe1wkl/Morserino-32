#!/usr/bin/env python3
"""
mac_soak.py -- the Bluetooth keyboard (vBand) soak with the Mac itself as the host (TODO I2).

soak.py uses a classic M32 as a stand-in computer. This one uses the real thing: macOS's own Bluetooth and
HID stack, i.e. what a vBand user has. It watches
  * the Control key as macOS sees it (CGEventSourceFlagsState - what vBand in the browser reads), and
  * whether the Morserino's HID keyboard is present (hidapi enumeration),
while it keys the Morserino over USB (M32 serial protocol, CW Keyer, PUT cw/repeat) and forces drop-outs with
blueutil.

ONE-TIME SETUP (Terminal.app, not an IDE shell - macOS asks for Bluetooth permission there):
    brew install blueutil
    python3 -m venv ~/m32-ble-venv
    ~/m32-ble-venv/bin/pip install pyserial hidapi pyobjc-framework-Quartz
  Pair the Morserino with the Mac: Morserino in CW Keyer with Bluetooth Use = VBand Kbd, then System Settings ->
  Bluetooth -> "Morserino32 Keyboard" -> Connect. (While pairing, keep the classic test host from grabbing the
  Morserino: run this script once with --park-only, or switch the classic off.)

RUN (overnight - the Morserino presses Control on this Mac many times a second; do not use the Mac meanwhile):
    ~/m32-ble-venv/bin/python mac_soak.py --dut /dev/cu.usbmodem12301 --classic /dev/cu.usbserial-0001 \\
        --drops 20 --minutes 120

What it reports
  * drop cycles: Control held on the Mac when the link is cut, keying stopped while it is down; after the
    reconnect, is Control still held on the Mac (STUCK - vBand keeps sounding) or released?
  * soak: every time the keyboard disappears by itself (time, and the Morserino's own "link lost, reason 0x.."
    line), Control held > --stuck s, and "silent" stretches (keyboard present, keying, no Control activity).
  * all of it in --log, one line per event.
"""
import argparse, json, os, re, shutil, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "protocol"))
import serial                      # noqa: E402
import hid                         # noqa: E402  (hidapi)
import Quartz                      # noqa: E402  (pyobjc-framework-Quartz)
from soak import Dut, TEXT, DROP_TEXT   # noqa: E402  - the same Morserino driver as the classic rig

NAME = "Morserino32 Keyboard"
VID, PID = 0xE502, 0xA111          # the PnP IDs the firmware announces


def now():
    return time.time()


class MacHost:
    """macOS as the host: Control state and keyboard presence, sampled in a thread."""

    def __init__(self, log, sample=0.005):
        self.log = log
        self.lock = threading.Lock()
        self.ctrl = False
        self.ctrl_since = now()
        self.present = False
        self.present_since = now()
        self.events = []                       # (wallclock, kind, detail): CTRL 1/0, LINK up/down
        self.stop = False
        self.sample = sample
        threading.Thread(target=self._watch_ctrl, daemon=True).start()
        threading.Thread(target=self._watch_link, daemon=True).start()

    def _note(self, kind, detail):
        t = now()
        with self.lock:
            self.events.append((t, kind, detail))
        self.log(f"mac  {kind} {detail}")

    def _watch_ctrl(self):
        while not self.stop:
            flags = Quartz.CGEventSourceFlagsState(Quartz.kCGEventSourceStateHIDSystemState)
            c = bool(flags & Quartz.kCGEventFlagMaskControl)
            if c != self.ctrl:
                self.ctrl, self.ctrl_since = c, now()
                self._note("CTRL", int(c))
            time.sleep(self.sample)

    def _watch_link(self):
        while not self.stop:
            p = any(d.get("product_string") == NAME or (d.get("vendor_id"), d.get("product_id")) == (VID, PID)
                    for d in hid.enumerate())
            if p != self.present:
                self.present, self.present_since = p, now()
                self._note("LINK", "up" if p else "down")
            time.sleep(0.25)

    def mark(self):
        with self.lock:
            return len(self.events)

    def wait_for(self, kind, detail, after_idx, timeout):
        end = now() + timeout
        while now() < end:
            with self.lock:
                for ev in self.events[after_idx:]:
                    if ev[1] == kind and ev[2] == detail:
                        return ev
            time.sleep(0.01)
        return None

    def ctrl_activity_since(self, t):
        with self.lock:
            return any(ev[1] == "CTRL" and ev[0] > t for ev in self.events)


class Blue:
    """blueutil: find the paired Morserino, drop and restore the link."""

    def __init__(self, method, say):
        self.method = method
        self.addr = None
        if not shutil.which("blueutil"):
            sys.exit("blueutil not found - brew install blueutil")
        out = subprocess.run(["blueutil", "--paired", "--format", "json"], capture_output=True, text=True).stdout
        for dev in json.loads(out or "[]"):
            if NAME.lower() in (dev.get("name") or "").lower():
                self.addr = dev.get("address")
        say(f"blueutil: Morserino paired as {self.addr or 'NOT FOUND (pair it first, see the header)'}; drop method {method}")

    def drop(self):
        if self.method == "disconnect" and self.addr:
            subprocess.run(["blueutil", "--disconnect", self.addr], capture_output=True)
        else:
            subprocess.run(["blueutil", "--power", "0"], capture_output=True)

    def restore(self):
        if self.method == "disconnect" and self.addr:
            subprocess.run(["blueutil", "--connect", self.addr], capture_output=True)
        else:
            subprocess.run(["blueutil", "--power", "1"], capture_output=True)


def park_classic(port, on, say):
    """Keep the classic test host (central/) from grabbing the Morserino: auto 0 + drop; or back to auto 1."""
    if not port:
        return
    try:
        s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.2; s.dtr = True; s.rts = True
        s.open()
        s.write(b"auto 1\n" if on else b"auto 0\ndrop\n")
        time.sleep(0.5)
        s.rts = False; s.dtr = False; s.close()
        say("classic test host " + ("re-enabled (auto 1)" if on else "parked (auto 0, link dropped)"))
    except Exception as exc:
        say(f"could not reach the classic test host on {port}: {exc}")


def main():
    ap = argparse.ArgumentParser(description="BLE keyboard soak with the Mac as host (TODO I2)")
    ap.add_argument("--dut", required=True, help="the Morserino's USB port")
    ap.add_argument("--classic", help="the classic test host's port, to park it during the run")
    ap.add_argument("--park-only", action="store_true", help="just park the classic test host (for pairing) and exit")
    ap.add_argument("--drops", type=int, default=20)
    ap.add_argument("--drop-method", choices=["disconnect", "power"], default="disconnect",
                    help="disconnect: blueutil --disconnect the Morserino only; power: Mac Bluetooth off/on "
                         "(drops every Bluetooth device, e.g. a Magic Keyboard, too)")
    ap.add_argument("--minutes", type=float, default=0)
    ap.add_argument("--resync", type=float, default=3.0)
    ap.add_argument("--stuck", type=float, default=2.0)
    ap.add_argument("--log", default=os.path.expanduser("~/m32_mac_soak.log"))
    args = ap.parse_args()

    logf = open(args.log, "a", buffering=1)
    t0 = now()
    def log(msg):
        logf.write(f"{time.strftime('%H:%M:%S')} +{now() - t0:8.2f}  {msg}\n")
    def say(msg):
        print(msg, flush=True); log("## " + msg)

    park_classic(args.classic, False, say)
    if args.park_only:
        return 0

    say(f"mac soak start, log {args.log}")
    mac = MacHost(log)
    blue = Blue(args.drop_method, say)
    dut = Dut(args.dut, log)
    dut.log_all = True                    # every reply and DEBUG line: a refused cw/repeat must be visible
    d = dut.link.device.get("device", dut.link.device)
    say(f"DUT: {d.get('hardware')} firmware {d.get('firmware')} edition {d.get('edition')}")
    saved = {n: dut.link.get_config(n) for n in ("Bluetooth Use", "Serial Output", "Key ext TX")}
    say(f"saved preferences {saved}")
    for n, v in {"Bluetooth Use": 1, "Serial Output": 0, **({"Key ext TX": 1} if saved.get("Key ext TX") == 0 else {})}.items():
        dut.link.command(f"PUT config/{n}/{v}")
    menus = dut.link.command("GET menus", timeout=5)
    keyer = next((m["menu number"] for m in menus.get("menus", []) if m.get("content") == "CW Keyer"), None)
    dut.link._serial.write(b"PUT menu/stop\n"); time.sleep(1.5)
    dut.link._serial.reset_input_buffer()
    dut.link.handshake()
    dut.link.command(f"PUT menu/start/{keyer}", allow_error=True)
    dut.start_reader()

    results = {"drop_pass": 0, "drop_stuck": 0, "drop_inconclusive": 0, "no_reconnect": 0,
               "self_drops": 0, "stuck": 0, "silent": 0}
    try:
        say("waiting for the Mac to connect the Morserino keyboard ...")
        if not mac.present and not mac.wait_for("LINK", "up", 0, 60):
            say("FAIL: the Morserino keyboard never appeared on the Mac (paired? classic parked?)")
            return 1
        time.sleep(2)

        def keying(text):
            for _ in range(3):
                m = mac.mark()
                dut.fire(f"PUT cw/repeat/{text}")
                if mac.wait_for("CTRL", 1, m, 6):
                    return True
                time.sleep(2)
            return False

        for i in range(args.drops):
            if not keying(DROP_TEXT):
                say(f"drop {i+1}: no Control activity from the Morserino - skipped"); results["drop_inconclusive"] += 1
                continue
            time.sleep(1.0)
            m = mac.mark()
            if not mac.wait_for("CTRL", 1, m, 5):
                results["drop_inconclusive"] += 1; continue
            blue.drop()                                   # cut while Control is held
            t_cut = now()
            down = mac.wait_for("LINK", "down", m, 8)
            dut.fire("PUT cw/stop")                       # key goes up while the Mac cannot hear it
            held_at_cut = mac.ctrl
            time.sleep(1.0)
            blue.restore()
            up = mac.wait_for("LINK", "up", m, 30)
            if not down:
                say(f"drop {i+1}: inconclusive - the link did not go down ({args.drop_method})"); results["drop_inconclusive"] += 1
                continue
            if not up:
                results["no_reconnect"] += 1
                say(f"drop {i+1}: NO RECONNECT within 30 s - Morserino: {dut.debug[-1][1] if dut.debug else 'no log line'}")
                continue
            time.sleep(args.resync)
            if mac.ctrl:
                results["drop_stuck"] += 1
                say(f"drop {i+1}: STUCK - Control still held on the Mac {args.resync:.0f} s after the reconnect "
                    f"(reconnect {up[0] - down[0]:.1f} s)")
                dut.fire("PUT cw/repeat/e"); time.sleep(1); dut.fire("PUT cw/stop")   # un-stick for the next cycle
            elif held_at_cut:
                results["drop_pass"] += 1
                say(f"drop {i+1}: pass - Control released on the Mac (reconnect {up[0] - down[0]:.1f} s)")
            else:
                results["drop_inconclusive"] += 1
                say(f"drop {i+1}: inconclusive - macOS had already released Control at the cut "
                    f"(reconnect {up[0] - down[0]:.1f} s)")
            time.sleep(1.0)

        if args.minutes > 0:
            say(f"soak: keying for {args.minutes:g} min")
            keying(TEXT)
            seen = mac.mark()
            t_end = now() + args.minutes * 60
            next_note = now() + 600
            while now() < t_end:
                time.sleep(0.5)
                with mac.lock:
                    new = mac.events[seen:]; seen = len(mac.events)
                for t, kind, detail in new:
                    if kind == "LINK" and detail == "down":
                        results["self_drops"] += 1
                        lines = [l for (tt, l) in dut.debug if abs(tt - t) < 5]
                        say(f"soak: keyboard gone by itself after {(t - t0)/60:.1f} min; Morserino: "
                            f"{lines[-1] if lines else 'no log line'}")
                    if kind == "LINK" and detail == "up":
                        say(f"soak: keyboard back after {now() - t:.1f} s")
                if mac.ctrl and now() - mac.ctrl_since > args.stuck:
                    results["stuck"] += 1
                    say(f"soak: STUCK - Control held for {now() - mac.ctrl_since:.1f} s")
                    mac.ctrl_since = now()
                if mac.present and not mac.ctrl_activity_since(now() - 30) and now() - mac.present_since > 30:
                    results["silent"] += 1
                    say("soak: SILENT - keyboard present, no Control activity for 30 s; re-starting the keying")
                    keying(TEXT)
                if now() > next_note:
                    next_note += 600
                    say(f"soak: {(now() - t0)/60:.0f} min")
        say(f"RESULT {results}")
    finally:
        mac.stop = True
        try:
            dut.fire("PUT cw/stop"); time.sleep(0.3)
            dut.fire("PUT menu/stop"); time.sleep(1.5)
            dut.stop_reader()
            for n, v in saved.items():
                for attempt in range(6):
                    try:
                        dut.link.command(f"PUT config/{n}/{v}", allow_error=True); break
                    except Exception:
                        time.sleep(5)
                        try: dut.link.handshake()
                        except Exception: pass
            say(f"restored preferences {saved}")
        finally:
            dut.link.close()
            if args.drop_method == "power":
                subprocess.run(["blueutil", "--power", "1"], capture_output=True)   # never leave Bluetooth off
            park_classic(args.classic, True, say)
    return 0


if __name__ == "__main__":
    sys.exit(main())

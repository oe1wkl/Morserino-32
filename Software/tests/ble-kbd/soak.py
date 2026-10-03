#!/usr/bin/env python3
"""
soak.py -- automated soak of the Morserino's Bluetooth keyboard (vBand mode), TODO I2.

Two devices on USB:
  * the Morserino under test (DUT), running the firmware, driven over the M32 serial protocol;
  * a classic M32 / Heltec V2 running central/ (the "test host"): it connects to "Morserino32 Keyboard"
    like a computer would and reports every HID input report and every disconnect.

What it does
  1. Saves and adjusts three DUT preferences (restored at the end): Bluetooth Use = VBand Kbd,
     Serial Output = Nothing (so the firmware's "BLE kbd: link lost" lines reach USB), and Key ext TX =
     CW Keyer only if it was Never (that would keep the keyer from sending Ctrl reports at all).
  2. Starts CW Keyer and keys text continuously (PUT cw/repeat) - Ctrl down/up reports flow to the host.
  3. Drop cycles (--drops N): the host cuts the link at a Ctrl-down report, the DUT stops keying while
     disconnected (so its key-up report is lost), the host reconnects. PASS if the host's key state is
     back to UP within --resync s of re-subscribing - the firmware's re-sync after (re)connect. Without
     it the host stays DOWN: exactly the stuck vBand key.
  4. Exit cycle (--exit-test): leave CW Keyer while the key is down; PASS if the last report the host
     saw before the link went away was UP (the release before teardown).
  5. Soak (--minutes M): keying continuously; reports every disconnect the host did not ask for (with
     both sides' reasons) and every stuck key (host DOWN for longer than --stuck s).

    ~/.platformio/penv/bin/python soak.py --dut /dev/cu.usbmodem12301 --host /dev/cu.usbserial-0001 \\
        --drops 20 --exit-test --minutes 60

Neither port is reset by opening it (DTR and RTS both asserted, see ../protocol/m32_link.py), so the DUT
keeps whatever state it is in.
"""
import argparse, os, re, sys, threading, time, json

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "protocol"))
import serial                      # noqa: E402
import m32_link                    # noqa: E402

TEXT = "paris paris cq de oe1wkl"
DROP_TEXT = "ttttt mmmm oooo"     # long dahs: the host's cut lands inside the element, so the key-up is lost


def now():
    return time.time()


class Host:
    """The test host on the classic: line protocol, see central/src/main.cpp."""

    def __init__(self, port, log):
        s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.05
        s.dtr = True; s.rts = True; s.open()
        self.s, self.log = s, log
        self.lock = threading.Lock()
        self.events = []            # (wallclock, kind, fields)
        self.state = None           # last Ctrl state seen: True down / False up / None unknown
        self.down_since = None
        self.subscribed = False
        self.stop = False
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        buf = b""
        while not self.stop:
            try:
                buf += self.s.read(512)
            except Exception:
                time.sleep(0.1); continue
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").strip()
                if not text:
                    continue
                parts = text.split()
                t = now()
                self.log("host " + text)
                with self.lock:
                    self.events.append((t, parts[0], parts[1:]))
                    if parts[0] == "R" and len(parts) >= 3:
                        down = bool(int(parts[2]) & 1)
                        if down and not self.state:
                            self.down_since = t
                        if not down:
                            self.down_since = None
                        self.state = down
                    elif parts[0] == "SUB":
                        self.subscribed = True
                    elif parts[0] == "DISC":
                        self.subscribed = False

    def send(self, cmd):
        self.s.write((cmd + "\n").encode())

    def wait_for(self, kind, after, timeout):
        """First event of `kind` after wallclock `after`, or None."""
        end = now() + timeout
        while now() < end:
            with self.lock:
                for ev in self.events:
                    if ev[0] > after and ev[1] == kind:
                        return ev
            time.sleep(0.02)
        return None

    def reports_between(self, t0, t1):
        with self.lock:
            return [e for e in self.events if t0 < e[0] <= t1 and e[1] == "R"]

    def close(self):
        self.stop = True
        time.sleep(0.1)
        self.s.rts = False; self.s.dtr = False; self.s.close()


class Dut:
    """The Morserino under test: protocol commands, plus its DEBUG lines ("BLE kbd: ...")."""

    def __init__(self, port, log):
        self.link = m32_link.M32Link(port)
        self.link.open(handshake=True, boot_wait=0.3)
        self.log = log
        self.debug = []             # (wallclock, line)
        self.reader = None

    def start_reader(self):
        # From here on M32Link is not used (it would compete for the bytes): fire-and-forget commands only.
        self.stop = False
        self.reader = threading.Thread(target=self._reader, daemon=True)
        self.reader.start()

    def _reader(self):
        buf = b""
        s = self.link._serial
        while not self.stop:
            try:
                buf += s.read(512)
            except Exception:
                time.sleep(0.1); continue
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").strip()
                if "BLE" in text or "Stopping" in text:
                    self.debug.append((now(), text))
                    self.log("dut  " + text)

    def stop_reader(self):
        self.stop = True
        if self.reader:
            self.reader.join(timeout=1)
        self.link._buffer = ""
        try:
            self.link._serial.reset_input_buffer()
        except Exception:
            pass

    def fire(self, cmd):
        self.log("dut> " + cmd)
        self.link._serial.write((cmd + "\n").encode())


def main():
    ap = argparse.ArgumentParser(description="BLE keyboard (vBand) soak, TODO I2")
    ap.add_argument("--dut", required=True)
    ap.add_argument("--host", required=True)
    ap.add_argument("--drops", type=int, default=20, help="forced drop/re-sync cycles")
    ap.add_argument("--exit-test", action="store_true", help="leave CW Keyer with the key down, once")
    ap.add_argument("--minutes", type=float, default=0, help="continuous soak afterwards")
    ap.add_argument("--resync", type=float, default=3.0, help="seconds allowed for the re-sync after re-subscribing")
    ap.add_argument("--stuck", type=float, default=2.0, help="host DOWN longer than this = stuck key")
    ap.add_argument("--log", default="ble_kbd_soak.log")
    args = ap.parse_args()

    logf = open(args.log, "a", buffering=1)
    t_start = now()

    def log(msg):
        logf.write(f"{time.strftime('%H:%M:%S')} +{now() - t_start:8.2f}  {msg}\n")

    def say(msg):
        print(msg, flush=True); log("## " + msg)

    say(f"soak start, log {args.log}")
    dut = Dut(args.dut, log)
    d = dut.link.device.get("device", dut.link.device)
    say(f"DUT: {d.get('hardware')} firmware {d.get('firmware')} edition {d.get('edition')}")

    # -- preferences: save, adjust ------------------------------------------------------------------------
    saved = {n: dut.link.get_config(n) for n in ("Bluetooth Use", "Serial Output", "Key ext TX")}
    say(f"saved preferences {saved}")
    wanted = {"Bluetooth Use": 1, "Serial Output": 0}
    if saved.get("Key ext TX") == 0:
        wanted["Key ext TX"] = 1
    for n, v in wanted.items():
        dut.link.command(f"PUT config/{n}/{v}")

    # -- start CW Keyer -------------------------------------------------------------------------------------
    menus = dut.link.command("GET menus", timeout=5)
    keyer = next((m["menu number"] for m in menus.get("menus", []) if m.get("content") == "CW Keyer"), None)
    if keyer is None:
        say(f"cannot find CW Keyer in GET menus: {str(menus)[:300]}")
        return 2
    dut.link.command("PUT menu/stop", allow_error=True)
    time.sleep(1.0)
    dut.link.command(f"PUT menu/start/{keyer}", allow_error=True)
    dut.start_reader()

    host = Host(args.host, log)
    host.send("auto 1")
    say("waiting for the test host to connect and subscribe ...")
    if not host.wait_for("SUB", t_start, 60):
        say("FAIL: the test host never subscribed (is the DUT in CW Keyer with Bluetooth Use = VBand?)")
        return finish(dut, host, saved, say, 1)
    say("subscribed")

    results = {"drop_pass": 0, "drop_fail": 0, "drop_inconclusive": 0, "unasked_disc": 0, "stuck": 0}

    # -- forced drop cycles ---------------------------------------------------------------------------------
    for i in range(args.drops):
        dut.fire(f"PUT cw/repeat/{DROP_TEXT}")
        if not host.wait_for("R", now(), 10):
            say(f"drop {i+1}: no reports from the DUT within 10 s - skipped"); results["drop_inconclusive"] += 1
            continue
        time.sleep(1.0)
        t0 = now()
        host.send("drop-on-down")
        disc = host.wait_for("DISC", t0, 10)
        if not disc:
            say(f"drop {i+1}: link did not drop"); results["drop_inconclusive"] += 1; continue
        dut.fire("PUT cw/stop")                       # key goes up while the host cannot hear it
        last = host.reports_between(t0, disc[0])
        down_at_drop = bool(last and int(last[-1][2][0]) & 1)
        sub = host.wait_for("SUB", disc[0], 20)
        if not sub:
            say(f"drop {i+1}: FAIL - host did not get back within 20 s"); results["drop_fail"] += 1; continue
        deadline = sub[0] + args.resync
        while now() < deadline and host.state:
            time.sleep(0.05)
        reconnect = sub[0] - disc[0]
        if not down_at_drop:
            results["drop_inconclusive"] += 1
            say(f"drop {i+1}: inconclusive (key-up slipped through before the drop), reconnect {reconnect:.1f} s")
        elif host.state:
            results["drop_fail"] += 1
            say(f"drop {i+1}: FAIL - host still sees the key DOWN {args.resync:.0f} s after re-subscribing (stuck)")
        else:
            results["drop_pass"] += 1
            say(f"drop {i+1}: pass - key released on the host after reconnect ({reconnect:.1f} s to reconnect)")
        time.sleep(1.0)

    # -- leave CW Keyer with the key down -------------------------------------------------------------------
    if args.exit_test:
        dut.fire(f"PUT cw/repeat/{TEXT}")
        host.wait_for("R", now(), 10)
        t0 = now()
        while now() - t0 < 10 and not host.state:
            time.sleep(0.005)
        dut.fire("PUT menu/stop")                     # leaves the mode -> stopBluetooth()
        disc = host.wait_for("DISC", t0, 10)
        last = host.reports_between(t0, disc[0] if disc else now())
        released = bool(last) and not (int(last[-1][2][0]) & 1)
        say(f"exit test: {'pass - last report before the link went was UP' if released else 'FAIL - host left with the key DOWN'}")
        results["exit"] = "pass" if released else "FAIL"
        dut.fire(f"PUT menu/start/{keyer}")
        host.wait_for("SUB", now(), 30)

    # -- continuous soak ------------------------------------------------------------------------------------
    if args.minutes > 0:
        say(f"soak: keying for {args.minutes:g} min")
        dut.fire(f"PUT cw/repeat/{TEXT}")
        t_end = now() + args.minutes * 60
        seen = len(host.events)
        next_note = now() + 600
        while now() < t_end:
            time.sleep(0.2)
            with host.lock:
                new = host.events[seen:]; seen = len(host.events)
            for t, kind, f in new:
                if kind == "DISC":
                    results["unasked_disc"] += 1
                    dut_lines = [l for (tt, l) in dut.debug if abs(tt - t) < 5]
                    say(f"soak: link dropped by itself after {(t - t_start)/60:.1f} min, host reason {f[1] if len(f) > 1 else '?'}"
                        f"; DUT: {dut_lines[-1] if dut_lines else 'no log line'}")
            if host.down_since and now() - host.down_since > args.stuck:
                results["stuck"] += 1
                say(f"soak: STUCK - host has seen the key DOWN for {now() - host.down_since:.1f} s")
                host.down_since = None
            if now() > next_note:
                next_note += 600
                say(f"soak: {(now() - t_start)/60:.0f} min, {sum(1 for e in host.events if e[1] == 'R')} reports so far")
        dut.fire("PUT cw/stop")

    say(f"RESULT {results}")
    ok = results["drop_fail"] == 0 and results["stuck"] == 0 and results.get("exit", "pass") == "pass"
    return finish(dut, host, saved, say, 0 if ok else 1)


def finish(dut, host, saved, say, code):
    # Each step on its own: one that fails must not keep the preferences from being restored.
    # "PUT menu/stop" sends no reply when it leaves a mode, so it is fired, not awaited.
    try:
        dut.fire("PUT cw/stop"); time.sleep(0.3)
        dut.fire("PUT menu/stop"); time.sleep(1.5)
        dut.stop_reader()
        failed = []
        for n, v in saved.items():
            if v is None:
                continue
            try:
                dut.link.command(f"PUT config/{n}/{v}", allow_error=True)
            except Exception as exc:
                failed.append(f"{n} ({exc})")
        say(f"restored preferences {saved}" + (f" - FAILED: {failed}" if failed else ""))
    finally:
        host.close()
        dut.link.close()
    return code


if __name__ == "__main__":
    sys.exit(main())

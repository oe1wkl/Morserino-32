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
                hms = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
                with self.lock:
                    # the host prints queued events only after a blocking reconnect, so the Mac's arrival
                    # time can put a DISC after the CONN/SUB that followed it: keep the host's own clock
                    self.events.append((t, parts[0], parts[1:], hms))
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

    def mark(self):
        """A position in the event list: later events are 'after' it, whatever their arrival order."""
        with self.lock:
            return len(self.events)

    def wait_for(self, kind, after_idx, timeout, after_ms=None):
        """First event of `kind` past list position `after_idx` (and, if given, with a host clock later
        than `after_ms`), or None."""
        end = now() + timeout
        while now() < end:
            with self.lock:
                for ev in self.events[after_idx:]:
                    if ev[1] == kind and (after_ms is None or (ev[3] or 0) > after_ms):
                        return ev
            time.sleep(0.02)
        return None

    def reports_between_ms(self, ms0, ms1):
        with self.lock:
            return [e for e in self.events if e[1] == "R" and e[3] is not None and ms0 < e[3] <= ms1]

    def last_ms(self):
        with self.lock:
            return max((e[3] for e in self.events if e[3] is not None), default=0)

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
                if "BLE" in text or "Stopping" in text or getattr(self, "log_all", False):
                    self.debug.append((now(), text))
                    self.log("dut  " + text)

    def lost_count(self):
        n = [int(m.group(1)) for _, l in self.debug for m in [re.search(r"\((\d+) since start", l)] if m]
        return n[-1] if n else 0

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
    ap.add_argument("--reconnect-delay", type=float, default=0,
                    help="seconds the test host waits after a disconnect before reconnecting")
    ap.add_argument("--reboot-first", action="store_true",
                    help="reboot the DUT before starting, so its connection count starts from zero")
    ap.add_argument("--until-fail", action="store_true",
                    help="stop the drop cycles at the first reconnect failure instead of recovering")
    args = ap.parse_args()

    logf = open(args.log, "a", buffering=1)
    t_start = now()

    def log(msg):
        logf.write(f"{time.strftime('%H:%M:%S')} +{now() - t_start:8.2f}  {msg}\n")

    def say(msg):
        print(msg, flush=True); log("## " + msg)

    say(f"soak start, log {args.log}")
    dut = Dut(args.dut, log)
    if args.reboot_first:
        dut.link.reboot(boot_wait=4.0)
        say("DUT rebooted")
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
    host.send(f"delay {int(args.reconnect_delay * 1000)}")
    say("waiting for the test host to connect and subscribe ...")
    if not host.wait_for("SUB", 0, 60):
        say("FAIL: the test host never subscribed (is the DUT in CW Keyer with Bluetooth Use = VBand?)")
        return finish(dut, host, saved, say, 1)
    say("subscribed")

    results = {"drop_pass": 0, "drop_fail": 0, "drop_inconclusive": 0, "unasked_disc": 0, "stuck": 0}

    # -- forced drop cycles ---------------------------------------------------------------------------------
    for i in range(args.drops):
        if not start_keying(dut, host, DROP_TEXT, say):
            say(f"drop {i+1}: no reports from the DUT - skipped"); results["drop_inconclusive"] += 1
            if not recover(dut, host, keyer, say):
                break
            continue
        time.sleep(1.0)
        m0, ms0 = host.mark(), host.last_ms()
        host.send("drop-on-down")
        disc = host.wait_for("DISC", m0, 10, after_ms=ms0)
        if not disc:
            say(f"drop {i+1}: link did not drop"); results["drop_inconclusive"] += 1; continue
        dut.fire("PUT cw/stop")                       # key goes up while the host cannot hear it
        last = host.reports_between_ms(ms0, disc[3])
        down_at_drop = bool(last and int(last[-1][2][1]) & 1)
        sub = host.wait_for("SUB", m0, 20, after_ms=disc[3])
        if not sub:
            results["drop_fail"] += 1
            say(f"drop {i+1}: FAIL - the host could not reconnect within 20 s (DUT link-lost count "
                f"{dut.lost_count()})")
            results["failed_at_cycle"] = i + 1
            if args.until_fail or not recover(dut, host, keyer, say):
                break
            continue
        deadline = now() + args.resync
        while now() < deadline and host.state:
            time.sleep(0.05)
        reconnect = (sub[3] - disc[3]) / 1000
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
        if not start_keying(dut, host, TEXT, say):
            say("exit test: inconclusive - no live link/keying"); results["exit"] = "inconclusive"
        else:
            t0 = now()
            while now() - t0 < 10 and not host.state:
                time.sleep(0.005)
            m0, ms0 = host.mark(), host.last_ms()
            dut.fire("PUT menu/stop")                 # leaves the mode -> stopBluetooth()
            disc = host.wait_for("DISC", m0, 10, after_ms=ms0)
            if not disc:
                say("exit test: inconclusive - the link did not go away"); results["exit"] = "inconclusive"
            else:
                last = host.reports_between_ms(ms0 - 1, disc[3])
                released = not host.state if not last else not (int(last[-1][2][1]) & 1)
                say(f"exit test: {'pass - the host was left with the key UP' if released else 'FAIL - host left with the key DOWN'}")
                results["exit"] = "pass" if released else "FAIL"
            time.sleep(1.5)
            dut.fire(f"PUT menu/start/{keyer}")
            host.wait_for("SUB", host.mark(), 30)

    # -- continuous soak ------------------------------------------------------------------------------------
    if args.minutes > 0:
        say(f"soak: keying for {args.minutes:g} min")
        if not start_keying(dut, host, TEXT, say):
            say("soak: could not get keying reports flowing - soak skipped")
            args.minutes = 0
        t_end = now() + args.minutes * 60
        seen = len(host.events)
        next_note = now() + 600
        while now() < t_end:
            time.sleep(0.2)
            with host.lock:
                new = host.events[seen:]; seen = len(host.events)
            for t, kind, f, _ms in new:
                if kind == "DISC" and f and f[-1] != "0x16":   # 0x16 on the host = its own deliberate cut
                    results["unasked_disc"] += 1
                    dut_lines = [l for (tt, l) in dut.debug if abs(tt - t) < 5]
                    say(f"soak: link dropped by itself after {(t - t_start)/60:.1f} min, host reason {f[1] if len(f) > 1 else '?'}"
                        f"; DUT: {dut_lines[-1] if dut_lines else 'no log line'}")
            last_r = max((e[0] for e in host.events[-50:] if e[1] == "R"), default=None)
            if last_r and now() - last_r > 30 and host.subscribed:
                results["silent"] = results.get("silent", 0) + 1
                say(f"soak: SILENT - subscribed but no report for {now() - last_r:.0f} s (DUT keying stopped, or the link is dead)")
                start_keying(dut, host, TEXT, say)
            if host.down_since and now() - host.down_since > args.stuck:
                results["stuck"] += 1
                say(f"soak: STUCK - host has seen the key DOWN for {now() - host.down_since:.1f} s")
                host.down_since = None
            if now() > next_note:
                next_note += 600
                say(f"soak: {(now() - t_start)/60:.0f} min, {sum(1 for e in host.events if e[1] == 'R')} reports so far")
        dut.fire("PUT cw/stop")

    say(f"RESULT {results}")
    ok = results["drop_fail"] == 0 and results["stuck"] == 0 and results.get("exit", "pass") in ("pass", "inconclusive")
    return finish(dut, host, saved, say, 0 if ok else 1)


def start_keying(dut, host, text, say, tries=3):
    """cw/repeat, and wait until reports actually reach the host (a CW Keyer that has just started may
    refuse with "Keyer not active")."""
    for _ in range(tries):
        if not host.subscribed and not host.wait_for("SUB", host.mark(), 20):
            continue
        m = host.mark()
        dut.fire(f"PUT cw/repeat/{text}")
        if host.wait_for("R", m, 6):
            return True
        time.sleep(2)
    return False


def recover(dut, host, keyer, say):
    """Leave and re-enter CW Keyer (re-initialises the DUT's Bluetooth) and wait for the host."""
    say("recover: re-entering CW Keyer on the DUT")
    dut.fire("PUT cw/stop"); time.sleep(0.3)
    dut.fire("PUT menu/stop"); time.sleep(2.0)
    m = host.mark()
    dut.fire(f"PUT menu/start/{keyer}")
    ok = host.wait_for("SUB", m, 40) is not None
    say("recover: " + ("host connected again" if ok else "host could NOT connect even after re-entering CW Keyer"))
    time.sleep(2.0)
    return ok


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
            for attempt in range(6):                  # the DUT can be busy for a while after a BLE fault
                try:
                    dut.link.command(f"PUT config/{n}/{v}", allow_error=True)
                    break
                except Exception as exc:
                    if attempt == 5:
                        failed.append(f"{n} ({exc})")
                    time.sleep(5)
                    try:
                        dut.link.handshake()
                    except Exception:
                        pass
        say(f"restored preferences {saved}" + (f" - FAILED: {failed}" if failed else ""))
    finally:
        host.close()
        dut.link.close()
    return code


if __name__ == "__main__":
    sys.exit(main())

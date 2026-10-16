#!/usr/bin/env python3
"""WinKeyer bench test for the M32's WinKeyer mode (devdocs/winkeyer/DESIGN.md).

Talks to the M32 the way a logger does: 1200 baud 8N2, and -- by default -- DTR on, RTS off,
which is what CQRLOG and flwkey do (DESIGN.md §2.4, §8). Each step prints what it sent, what came
back, and PASS/FAIL; the paddle step asks you to key something.

    python3 wk_bench.py /dev/cu.usbserial-0001            # classic M32 (CP2102)
    python3 wk_bench.py /dev/cu.usbmodem1101 --lines both # Pocket, DTR+RTS both on
    python3 wk_bench.py PORT --only open,send,xoff        # a subset (names: see STEPS below)

Before running: on the classic, start Transceiver > WinKeyer on the M32 first (its port is at
115200 until then). On the Pocket either do the same, or leave it at the top menu -- the Host Open
below enters the mode on its own (auto-detect).

CAUTION: with "Key Ext Tx" set to anything but Never, the send and tune steps key the transmitter.
Use a dummy load or leave the key jack empty.

Needs pyserial (pip install pyserial).
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("needs pyserial: pip install pyserial")


class WK:
    def __init__(self, port, baud, lines, verbose):
        self.verbose = verbose
        s = serial.Serial()
        s.port = port
        s.baudrate = baud
        s.bytesize = serial.EIGHTBITS
        s.parity = serial.PARITY_NONE
        s.stopbits = serial.STOPBITS_TWO
        s.timeout = 0.02
        # macOS raises DTR and RTS when the port opens, and pyserial then applies the levels asked for
        # one at a time, DTR first - asking for both off would walk through DTR=0/RTS=1, the ESP32
        # auto-reset state, and reset the M32 on every open (see tests/protocol/m32_link.py). So open
        # with both on (nothing moves), then drop RTS before DTR: the only state passed through is
        # DTR=1/RTS=0, which holds GPIO0 (the classic's FN) low for an instant but never resets.
        # "dtr" = CQRLOG/flwkey (DTR on, RTS off: on the classic that holds FN down - BENCH.md).
        s.dtr = True
        s.rts = True
        t0 = time.time()
        s.open()
        if lines in ("dtr", "none"):
            s.rts = False
        if lines == "none":
            s.dtr = False
        self.s = s
        self.opened_at = t0
        self.status = None          # last status byte seen
        self.pot = None             # last speed-pot byte seen
        self.echo = bytearray()     # echo bytes, in order
        self.breakin_echo = bytearray()

    def send(self, data, note=""):
        if isinstance(data, int):
            data = bytes([data])
        if self.verbose:
            print(f"   -> {data.hex(' ')}  {note}")
        self.s.write(data)

    def read_for(self, seconds, until=None):
        """Collect bytes for up to `seconds`, sorting them as a host must (K1EL p.3)."""
        got = bytearray()
        end = time.time() + seconds
        while time.time() < end:
            chunk = self.s.read(64)
            for b in chunk:
                got.append(b)
                if b & 0xC0 == 0xC0:
                    self.status = b
                elif b & 0xC0 == 0x80:
                    self.pot = b
                else:
                    (self.breakin_echo if self.status is not None and self.status & 0x02 else self.echo).append(b)
            if until and until(got):
                break
        if self.verbose and got:
            print(f"   <- {got.hex(' ')}")
        return got

    def close(self):
        try:
            self.s.rts = False
            self.s.dtr = False
        except Exception:
            pass
        self.s.close()


def describe_status(b):
    if b is None:
        return "none"
    bits = [n for n, m in (("WAIT", 0x10), ("KEYDOWN", 0x08), ("BUSY", 0x04), ("BREAKIN", 0x02), ("XOFF", 0x01)) if b & m]
    return f"0x{b:02X} ({' '.join(bits) or 'idle'})"


results = []


def check(name, ok, detail=""):
    results.append((name, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" - {detail}" if detail else ""))


# ---------------------------------------------------------------------------- steps

def step_echo(wk):
    """K1EL start-up as CQRLOG does it: nulls, then Echo Test (CQRLOG waits 300 ms)."""
    wk.send(b"\x13\x13\x13\x13", "4 x null")
    t = time.time()
    wk.send(b"\x00\x04U", "echo test 'U'")
    got = wk.read_for(0.3, until=lambda g: b"U" in g)
    check("echo test answered within 300 ms", b"U" in got, f"{(time.time() - t) * 1000:.0f} ms")


def step_values(wk):
    """Closed-state admin reads: Get Values (WK2: 15 bytes), Vcc, Dump EEPROM (flwkey's 'read parameters')."""
    wk.send(b"\x00\x07", "get values")
    got = wk.read_for(0.6, until=lambda g: len(g) >= 15)
    check("get values: 15 bytes", len(got) == 15, got.hex(" "))
    if len(got) == 15:
        print(f"      mode 0x{got[0]:02X}, speed {got[1]}, sidetone 0x{got[2]:02X}, weight {got[3]}, "
              f"pot {got[6]}+{got[7]}, ratio {got[12]}, pincfg 0x{got[13]:02X}")
    wk.send(b"\x00\x15", "read back Vcc")
    got = wk.read_for(0.3, until=lambda g: len(g) >= 1)
    check("Vcc: one byte", len(got) == 1, f"{26214 / got[0] / 100:.2f} V" if got and got[0] else "")
    wk.send(b"\x00\x0C", "dump EEPROM")
    got = wk.read_for(4.0, until=lambda g: len(g) >= 256)
    check("dump EEPROM: 256 bytes", len(got) == 256, f"first 16: {bytes(got[:16]).hex(' ')}")


def step_open(wk):
    t = time.time()
    wk.send(b"\x00\x02", "host open")
    got = wk.read_for(0.5, until=lambda g: len(g) >= 1)
    ms = (time.time() - t) * 1000
    check("host open: revision byte", len(got) >= 1, f"revision {got[0] if got else '-'}")
    check("host open: answered within 50 ms (CQRLOG's limit)", bool(got) and ms <= 50, f"{ms:.0f} ms")
    wk.read_for(0.8)            # the Pocket repaints after replying; let it settle


def step_send(wk, wpm=20):
    """Serial echo on, a word at a known speed, then check echo and timing."""
    wk.send(bytes([0x0E, 0x44]), "mode: serial echo + paddle echo")
    wk.send(bytes([0x02, wpm]), f"speed {wpm}")
    wk.echo.clear()
    text = b"PARIS "
    t = time.time()
    wk.send(text, "text")
    wk.read_for(8.0, until=lambda g: bytes(wk.echo).endswith(b"PARIS ") and wk.status is not None and not wk.status & 0x04)
    secs = time.time() - t
    check("serial echo = text sent", bytes(wk.echo) == text, repr(bytes(wk.echo)))
    # PARIS + word space = 50 units = 60/wpm seconds
    expect = 60.0 / wpm
    check(f"PARIS at {wpm} WpM takes ~{expect:.2f} s", abs(secs - expect) < 0.25 * expect + 0.15, f"{secs:.2f} s")
    check("status back to idle", wk.status is not None and not wk.status & 0x04, describe_status(wk.status))


def step_prosigns(wk):
    wk.echo.clear()
    wk.send(b"<", "AR (mapped)")
    wk.send(bytes([0x1B]) + b"SK", "merge S+K")
    wk.read_for(4.0, until=lambda g: len(wk.echo) >= 3)
    check("prosign + merge echoed", bytes(wk.echo) == b"<SK", repr(bytes(wk.echo)))


def step_xoff(wk):
    """Flood the buffer: XOFF must rise above 2/3; Clear Buffer must empty it at once."""
    wk.send(bytes([0x02, 10]), "speed 10 (slow)")
    wk.send(b"E" * 100, "100 x E")
    wk.read_for(1.0, until=lambda g: wk.status is not None and wk.status & 0x01)
    check("XOFF set", wk.status is not None and bool(wk.status & 0x01), describe_status(wk.status))
    wk.send(0x0A, "clear buffer")
    wk.read_for(1.0, until=lambda g: wk.status is not None and not wk.status & 0x05)
    check("clear buffer: idle, XOFF gone", wk.status is not None and not wk.status & 0x05, describe_status(wk.status))
    wk.send(bytes([0x02, 20]), "speed 20")


def step_status(wk):
    wk.send(0x15, "request status")
    got = wk.read_for(0.5, until=lambda g: any(b & 0xC0 == 0xC0 for b in g))
    check("request status", any(b & 0xC0 == 0xC0 for b in got), describe_status(wk.status))
    wk.send(0x07, "get speed pot")
    got = wk.read_for(0.5, until=lambda g: any(b & 0xC0 == 0x80 for b in g))
    check("get speed pot", any(b & 0xC0 == 0x80 for b in got), f"pot 0x{wk.pot:02X}" if wk.pot is not None else "")
    print("      turn the encoder one detent now (10 s) ...")
    before = wk.pot
    wk.read_for(10.0, until=lambda g: any(b & 0xC0 == 0x80 for b in g))
    check("encoder reported as speed pot", wk.pot is not None and wk.pot != before, f"pot 0x{(wk.pot or 0):02X}")


def step_potspeed(wk):
    """E1 in pot mode: the host never sends a speed, so the encoder is the sending speed.
    Sends PARIS ten times and measures each word from the serial echo (PARIS + space = 50 units)."""
    wk.send(bytes([0x0E, 0x04]), "mode: serial echo (no speed command: pot mode)")
    wk.send(0x07, "get speed pot")
    wk.read_for(0.3)
    start_pot = wk.pot
    wk.echo.clear()
    wk.send(b"PARIS " * 10, "10 x PARIS")
    print("      ENCODER: after the 2nd PARIS, turn it a few clicks UP (clockwise) ...")
    t0 = time.time()
    last_space, words, pots = None, [], []
    seen = 0
    end = time.time() + 60
    while time.time() < end and wk.echo.count(b" ") < 10:
        before_pot = wk.pot
        wk.read_for(0.05)
        now = time.time() - t0
        if wk.pot != before_pot:
            pots.append((now, wk.pot & 0x3F))
        spaces = wk.echo.count(b" ")
        if spaces > seen:
            if last_space is not None:
                words.append((now, 60.0 / (now - last_space)))
            last_space, seen = now, spaces
    for t, w in words:
        print(f"      word ending at {t:5.1f} s: {w:4.1f} WpM")
    for t, p in pots:
        print(f"      pot byte at {t:5.1f} s: {p} (+ pot min)")
    check("encoder reported as speed pot", bool(pots), f"start 0x{(start_pot or 0):02X}")
    if len(words) >= 4:
        first, last = words[0][1], words[-1][1]
        check("sending speed followed the encoder", abs(last - first) >= 2, f"{first:.1f} -> {last:.1f} WpM")
    else:
        check("sending speed followed the encoder", False, "too few words timed")


def step_buffered(wk):
    wk.echo.clear()
    wk.send(bytes([0x1C, 30]) + b"FAST" + bytes([0x1E]) + b" " + bytes([0x1A, 2]) + b"T", "speed 30, FAST, cancel, wait 2 s, T")
    saw_wait = False
    end = time.time() + 10
    while time.time() < end and bytes(wk.echo) != b"FAST T":      # "FAST" alone also ends in T
        wk.read_for(0.1)
        saw_wait |= wk.status is not None and bool(wk.status & 0x10)
    check("buffered speed + wait: all echoed", bytes(wk.echo) == b"FAST T", repr(bytes(wk.echo)))
    check("WAIT status seen", saw_wait)


def step_tune(wk):
    print("      TUNE: keys the transmitter for 1 s")
    wk.send(bytes([0x0B, 1]), "key immediate on")
    wk.read_for(1.0)
    busy = wk.status is not None and bool(wk.status & 0x0C)
    wk.send(bytes([0x0B, 0]), "key immediate off")
    wk.read_for(0.5)
    check("tune: busy/keydown while on, idle after", busy and not wk.status & 0x0C, describe_status(wk.status))


def step_defaults(wk):
    """A Load Defaults block as flwkey sends it (WK2 layout)."""
    block = bytes([0x44, 22, 0x08, 50, 0, 0, 10, 25, 0, 0, 0, 50, 50, 0x06, 0])
    wk.send(bytes([0x0F]) + block, "load defaults")
    wk.echo.clear()
    wk.send(b"OK ", "text after defaults")
    wk.read_for(4.0, until=lambda g: bytes(wk.echo).endswith(b"OK "))
    check("load defaults parsed (stream still in sync)", bytes(wk.echo) == b"OK ", repr(bytes(wk.echo)))


def step_breakin(wk):
    wk.send(bytes([0x0E, 0x44]), "mode: serial echo + paddle echo (Host Open resets it)")
    wk.send(bytes([0x02, 12]), "speed 12")
    wk.send(b"VVV VVV VVV VVV VVV", "long text")
    wk.read_for(0.8)
    print("      PADDLE: key a short word (e.g. 'TEST') now, within 15 s ...")
    wk.breakin_echo.clear()
    saw_breakin = False
    end = time.time() + 15
    while time.time() < end:
        wk.read_for(0.1)
        saw_breakin |= wk.status is not None and bool(wk.status & 0x02)
        if saw_breakin and wk.status is not None and not wk.status & 0x02 and wk.breakin_echo:
            break
    check("break-in status seen", saw_breakin)
    check("paddle echo received while BREAKIN high", len(wk.breakin_echo) > 0, repr(bytes(wk.breakin_echo)))
    wk.send(0x15, "request status")
    wk.read_for(0.5)
    check("host text was dropped (not busy after break-in)", wk.status is not None and not wk.status & 0x04,
          describe_status(wk.status))
    wk.send(bytes([0x02, 20]), "speed 20")


def step_close(wk):
    wk.send(b"\x00\x03", "host close")
    wk.read_for(0.5)
    wk.send(b"\x00\x04Z", "echo test after close")
    got = wk.read_for(0.5, until=lambda g: b"Z" in g)
    check("closed state still answers admin commands", b"Z" in got)


STEPS = {
    "echo": step_echo, "values": step_values, "open": step_open, "send": step_send,
    "prosigns": step_prosigns, "xoff": step_xoff, "status": step_status, "potspeed": step_potspeed,
    "buffered": step_buffered,
    "defaults": step_defaults, "tune": step_tune, "breakin": step_breakin, "close": step_close,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=1200, help="1200 (default); 9600 only after Set High Baud")
    ap.add_argument("--lines", choices=("dtr", "both", "none"), default="dtr",
                    help="DTR/RTS while open: dtr = DTR on/RTS off like CQRLOG and flwkey (default)")
    ap.add_argument("--only", help="comma-separated step names: " + ",".join(STEPS))
    ap.add_argument("--quiet", action="store_true", help="do not print the bytes")
    a = ap.parse_args()

    names = a.only.split(",") if a.only else list(STEPS)
    wk = WK(a.port, a.baud, a.lines, not a.quiet)
    print(f"opened {a.port} at {a.baud} 8N2, lines={a.lines} - if the M32 rebooted now, note it (DTR/RTS check)")
    time.sleep(0.4)
    wk.s.reset_input_buffer()
    try:
        for n in names:
            print(f"\n== {n}")
            STEPS[n](wk)
    finally:
        wk.close()
    failed = [n for n, ok in results if not ok]
    print(f"\n{len(results) - len(failed)}/{len(results)} checks passed" + (": FAILED " + ", ".join(failed) if failed else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

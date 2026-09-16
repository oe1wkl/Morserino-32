#!/usr/bin/env python3
"""M32KIP D17 remote configuration, tested against a real Remote Rig.

    /usr/bin/python3 config_tests.py --rig 192.168.1.237 --psk "..."

Apple's python on macOS: a third-party interpreter is blocked from the local network (see
devdocs/m32kip/TEST_REPORT.md), and this needs nothing outside the standard library.

The Rig must be listening with NO session (spec §8 allows one), so stop Remote Keyer on the other device first.

Four properties, each one a thing that could plausibly be wrong:

  capability  HELLO_ACK advertises ACK_CFG_CAPABLE, or a Keyer would never offer the items at all.
  fetch       CFG_REQ is answered with the rig's current six values, flagged as stored.
  set         CFG_SET changes a value, and the reply reports what was STORED rather than echoing the request.
  clamp       a value outside a parameter's range is clamped BY THE RIG - a stale or hostile Keyer must not be
              able to put a transmitter's key-down limit out of range.

The NVS round trip (does the value survive a reboot?) is checked by --verify-only after the rig has been restarted.
"""

import argparse, hashlib, hmac, os, socket, struct, sys, time

MAGIC, VERSION = 0x4B, 0x01
PKT_HELLO, PKT_HELLO_ACK, PKT_BYE, PKT_NACK = 1, 2, 5, 6
PKT_CFG_REQ, PKT_CFG_VAL, PKT_CFG_SET = 7, 8, 9
ACK_CFG_CAPABLE, CFG_STORED = 0x02, 0x01

# index -> (name, minimum, maximum), mirroring pliste[] on the device
FIELDS = [("Rig Delay", 0, 9), ("Rig Limit Kyr", 1, 30), ("Rig Limit SK", 1, 30),
          ("Rig 1st Ext", 0, 30), ("Rig Hang Unit", 0, 1), ("Rig Hang", 0, 60)]


def header(ptype, session, seq):
    return struct.pack("<BBBBIHH", MAGIC, VERSION, ptype, 0, session, seq, 0)


def seal(body, key):
    return body + hmac.new(key, body, hashlib.sha256).digest()[:8]


def opened(data, key):
    if len(data) < 20 or data[0] != MAGIC:
        return None
    if not hmac.compare_digest(hmac.new(key, data[:-8], hashlib.sha256).digest()[:8], data[-8:]):
        return None
    return data[2], data[12:-8]


results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (f"   {detail}" if detail and not ok else ""), flush=True)


def note(text):
    print(f"     {text}", flush=True)


class Link:
    def __init__(self, rig, port, psk):
        self.addr = (rig, port)
        self.base = hashlib.sha256(psk.encode()).digest()
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.settimeout(1.0)
        self.t0 = time.monotonic()
        self.seq = 0
        self.session = 0
        self.ksess = None
        self.ack_flags = 0

    def ticks(self):
        return int((time.monotonic() - self.t0) * 10000) & 0xFFFFFFFF

    def handshake(self):
        nonce_c = os.urandom(8)
        body = header(PKT_HELLO, 0, 1) + nonce_c + struct.pack("<IBBBB", self.ticks(), 25, 4, 0, 0)
        for _ in range(6):
            self.s.sendto(seal(body, self.base), self.addr)
            try:
                data, _ = self.s.recvfrom(256)
            except socket.timeout:
                continue
            got = opened(data, self.base)
            if not got:
                continue
            ptype, payload = got
            if ptype == PKT_NACK:
                return "BUSY"
            if ptype == PKT_HELLO_ACK and payload[:8] == nonce_c:
                self.session = struct.unpack_from("<I", payload, 16)[0]
                self.ack_flags = payload[23]
                self.ksess = hmac.new(self.base, nonce_c + payload[8:16], hashlib.sha256).digest()
                return "OK"
        return "SILENT"

    def cfg_req(self):
        self.seq += 1
        self.s.sendto(seal(header(PKT_CFG_REQ, self.session, self.seq & 0xFFFF), self.ksess), self.addr)
        return self.await_val()

    def cfg_set(self, values):
        self.seq += 1
        body = header(PKT_CFG_SET, self.session, self.seq & 0xFFFF) + bytes(list(values) + [0, 0])
        self.s.sendto(seal(body, self.ksess), self.addr)
        return self.await_val()

    def await_val(self, timeout=6.0):
        """The rig answers when it next has the key up and no edge due, so allow generous time."""
        end = time.time() + timeout
        while time.time() < end:
            try:
                data, _ = self.s.recvfrom(256)
            except socket.timeout:
                continue
            got = opened(data, self.ksess)
            if got and got[0] == PKT_CFG_VAL:
                return list(got[1][:6]), got[1][6]
        return None, None

    def bye(self):
        for i in range(3):
            self.seq += 1
            self.s.sendto(seal(header(PKT_BYE, self.session, self.seq & 0xFFFF), self.ksess), self.addr)
            time.sleep(0.1)


def show(values):
    return ", ".join(f"{FIELDS[i][0]}={values[i]}" for i in range(6))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rig", required=True)
    ap.add_argument("--port", type=int, default=7374)
    ap.add_argument("--psk", required=True)
    ap.add_argument("--expect", help="verify these six comma-separated values survived (after a rig reboot)")
    args = ap.parse_args()

    link = Link(args.rig, args.port, args.psk)
    state = link.handshake()
    if state == "BUSY":
        print("the Rig already has a session - stop Remote Keyer on the other device first", file=sys.stderr)
        return 2
    if state != "OK":
        print(f"no answer from the Rig at {args.rig}", file=sys.stderr)
        return 2
    print(f"session {link.session:#010x}, HELLO_ACK flags {link.ack_flags:#04x}\n", flush=True)

    try:
        check("capability: the rig advertises remote configuration",
              bool(link.ack_flags & ACK_CFG_CAPABLE), f"flags={link.ack_flags:#04x}")

        values, flags = link.cfg_req()
        check("fetch: the rig answers CFG_REQ with its settings", values is not None, "no CFG_VAL came back")
        if values is None:
            return 1
        note(show(values))
        check("fetch: the values are flagged as stored", bool(flags & CFG_STORED), f"flags={flags:#04x}")

        if args.expect:                                   # --expect: the NVS round trip, after a reboot
            want = [int(x) for x in args.expect.split(",")]
            check("NVS: the values survived the rig's reboot", values == want, f"got {values}, want {want}")
            return 0 if all(results) else 1

        # Change one value the operator would plausibly change, and check the REPLY rather than assuming.
        target = 1                                        # Rig Limit Kyr, seconds
        newval = 7 if values[target] != 7 else 9
        wanted = list(values)
        wanted[target] = newval
        got, flags = link.cfg_set(wanted)
        check("set: the rig answers a CFG_SET", got is not None, "no CFG_VAL came back")
        if got is not None:
            note(show(got))
            check("set: the changed value took", got[target] == newval, f"{FIELDS[target][0]}={got[target]}")
            check("set: nothing else moved",
                  all(got[i] == values[i] for i in range(6) if i != target), f"{show(got)}")
            check("set: the reply says the values are stored", bool(flags & CFG_STORED), f"flags={flags:#04x}")

        # Out of range, deliberately: the rig must clamp rather than accept. A transmitter's key-down limit is
        # not something a stale or hostile Keyer may set to anything it likes.
        hostile = list(got or values)
        hostile[target] = 250                             # far above Rig Limit Kyr's maximum of 30
        got2, _ = link.cfg_set(hostile)
        check("clamp: an out-of-range value is answered", got2 is not None, "no CFG_VAL came back")
        if got2 is not None:
            lo, hi = FIELDS[target][1], FIELDS[target][2]
            check("clamp: the rig clamped it to the parameter's own maximum",
                  got2[target] == hi, f"{FIELDS[target][0]}={got2[target]}, maximum is {hi}")
            note(f"asked for 250, rig stored {got2[target]} (range {lo}..{hi})")
            print(f"\nto check the NVS round trip: restart the rig, then run with "
                  f"--expect {','.join(str(v) for v in got2)}", flush=True)
    finally:
        link.bye()

    print(f"\n{sum(results)}/{len(results)} passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())

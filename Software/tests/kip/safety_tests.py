#!/usr/bin/env python3
"""M32KIP spec §13.4 safety tests, driven from a laptop against a real Remote Rig.

    /usr/bin/python3 safety_tests.py --rig 192.168.1.237 --psk "..."

Apple's python on macOS: a third-party interpreter is blocked from the local network (see
devdocs/m32kip/TEST_REPORT.md), and this script needs nothing outside the standard library.

The Rig must be listening with NO session (spec §8 allows one): stop the Remote Keyer on the other device first.
The script says so rather than failing obscurely if a session is already held.

Three tests, each read from the Rig's own STATS:

  forged    a KEY packet whose MAC is wrong must be ignored - the session survives, keying continues, and the
            watchdog is not disturbed (spec §9: a bad MAC is dropped in silence, D12c).
  keydown   a mark held past max_keydown_ms must be cut: the Rig lifts the key at the limit and sets bit 3 of
            rig_state. The stream is kept alive throughout, so this is the LIMIT acting, not the watchdog.
  watchdog  when the Keyer goes silent mid-mark, TX must drop within keepalive_timeout (spec §8).
"""

import argparse, hashlib, hmac, os, socket, struct, sys, time

MAGIC, VERSION = 0x4B, 0x01
PKT_HELLO, PKT_HELLO_ACK, PKT_KEY, PKT_STATS, PKT_BYE, PKT_NACK = 1, 2, 3, 4, 5, 6
TICKS_PER_MS = 10
RIG_KEY_DOWN, RIG_KEYDOWN_LIMIT = 0x01, 0x08


def header(ptype, session, seq):
    return struct.pack("<BBBBIHH", MAGIC, VERSION, ptype, 0, session, seq, 0)


def seal(body, key):
    return body + hmac.new(key, body, hashlib.sha256).digest()[:8]


def opened(data, key):
    if len(data) < 20 or data[0] != MAGIC:
        return None
    want = hmac.new(key, data[:-8], hashlib.sha256).digest()[:8]
    if not hmac.compare_digest(want, data[-8:]):
        return None
    return data[2], data[12:-8]


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
        self.max_keydown = 0

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
                self.session, self.max_keydown = struct.unpack_from("<IH", payload, 16)[0:2]
                self.ksess = hmac.new(self.base, nonce_c + payload[8:16], hashlib.sha256).digest()
                return "OK"
        return "SILENT"

    def key(self, edges, t_now=None):
        """One KEY packet carrying `edges` as (tick, state), newest last."""
        self.seq += 1
        body = (header(PKT_KEY, self.session, self.seq & 0xFFFF)
                + struct.pack("<IBBBB", self.ticks() if t_now is None else t_now, 25, len(edges), 0, 0)
                + b"".join(struct.pack("<IB", t, st) for t, st in edges))
        return seal(body, self.ksess)

    def send(self, packet):
        self.s.sendto(packet, self.addr)

    def stats(self, timeout=2.0):
        """The next STATS the Rig sends, as a dict; None if it says nothing."""
        end = time.time() + timeout
        while time.time() < end:
            try:
                data, _ = self.s.recvfrom(256)
            except socket.timeout:
                continue
            got = opened(data, self.ksess)
            if not got or got[0] != PKT_STATS:
                continue
            (playout, jitter, loss, late, under, rig_state, mk, dit, techo, trig) = \
                struct.unpack("<HHBBBBHHII", got[1])
            return dict(playout=playout, late=late, under=under, state=rig_state, max_keydown=mk, dit=dit)
        return None

    def bye(self):
        for i in range(3):
            self.seq += 1
            self.send(seal(header(PKT_BYE, self.session, self.seq & 0xFFFF), self.ksess))
            time.sleep(0.1)


results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (f"   {detail}" if detail else ""), flush=True)


def test_forged(link):
    """A packet with a broken MAC must change nothing."""
    t = link.ticks()
    link.send(link.key([(t, 1)]))                       # a real key-down, so there is a session state to disturb
    time.sleep(0.2)
    good = link.key([(link.ticks(), 0)])                # ... and its key-up, which we corrupt
    forged = good[:-1] + bytes([good[-1] ^ 0xFF])
    for _ in range(5):
        link.send(forged)
        time.sleep(0.05)
    st = link.stats(3)
    check("forged MAC: the session survives", st is not None, "no STATS came back" if st is None else "")
    if st:
        # The key-up was only ever sent forged, so the mark is still on the air: the Rig ignored the bad packet
        # rather than acting on it.
        check("forged MAC: it was ignored, not acted on", bool(st["state"] & RIG_KEY_DOWN),
              f"rig_state={st['state']:#04x}")
    link.send(link.key([(link.ticks(), 0)]))            # release the key properly
    time.sleep(0.3)


def test_keydown_limit(link):
    """A mark longer than max_keydown_ms is cut at the limit, with the stream kept alive throughout."""
    limit_ms = link.max_keydown or 3000
    down_t = link.ticks()
    link.send(link.key([(down_t, 1)]))
    deadline = time.time() + limit_ms / 1000.0 + 2.0
    hit, lifted_after = None, None
    start = time.time()
    while time.time() < deadline:
        link.send(link.key([(down_t, 1)]))              # keepalives repeating the same edge: no new edges
        st = link.stats(0.3)
        if st:
            if st["state"] & RIG_KEYDOWN_LIMIT and hit is None:
                hit = time.time() - start
            if hit is not None and not (st["state"] & RIG_KEY_DOWN) and lifted_after is None:
                lifted_after = time.time() - start
                break
        time.sleep(0.2)
    check(f"key-down limit ({limit_ms} ms): the Rig flagged it", hit is not None,
          f"after {hit:.1f}s" if hit else "bit 3 never set")
    check("key-down limit: the key went up", lifted_after is not None,
          f"after {lifted_after:.1f}s" if lifted_after else "still down")
    if lifted_after:
        margin = abs(lifted_after - limit_ms / 1000.0)
        check("key-down limit: cut at the limit, not late", margin < 1.0, f"{lifted_after:.1f}s vs {limit_ms/1000:.1f}s")
    link.send(link.key([(link.ticks(), 0)]))
    time.sleep(0.3)


def test_watchdog(link):
    """Silence mid-mark must drop the key within the keepalive timeout."""
    t = link.ticks()
    link.send(link.key([(t, 1)]))
    time.sleep(0.5)
    st = link.stats(2)
    if not (st and st["state"] & RIG_KEY_DOWN):
        check("watchdog: a mark was on the air to begin with", False, "the key never went down")
        return
    start = time.time()
    dropped = None
    while time.time() - start < 6:                       # say nothing at all: no keys, no keepalives
        st = link.stats(0.5)
        if st and not (st["state"] & RIG_KEY_DOWN):
            dropped = time.time() - start
            break
    check("watchdog: the key dropped on silence", dropped is not None,
          f"after {dropped:.1f}s" if dropped else "still down after 6 s")
    if dropped is not None:
        check("watchdog: within the keepalive timeout", dropped < 2.0, f"{dropped:.1f}s")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rig", required=True)
    ap.add_argument("--port", type=int, default=7374)
    ap.add_argument("--psk", required=True)
    args = ap.parse_args()

    link = Link(args.rig, args.port, args.psk)
    state = link.handshake()
    if state == "BUSY":
        print("the Rig already has a session - stop Remote Keyer on the other device first", file=sys.stderr)
        return 2
    if state != "OK":
        print(f"no answer from the Rig at {args.rig}", file=sys.stderr)
        return 2
    print(f"session {link.session:#010x}, max key-down {link.max_keydown} ms\n", flush=True)

    try:
        test_forged(link)
        test_keydown_limit(link)
        test_watchdog(link)
    finally:
        link.bye()

    print(f"\n{sum(results)}/{len(results)} passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())

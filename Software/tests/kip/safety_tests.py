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
    """Detail explains a FAILURE. Printing it on success too put 'the key never went down' next to a PASS."""
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (f"   {detail}" if detail and not ok else ""), flush=True)


def note(text):
    """A measured fact worth showing whatever the verdict."""
    print(f"     {text}", flush=True)


def wait_key_down(link, edge, timeout=6.0):
    """Wait until the Rig reports the key actually down, keeping the stream alive, and return that STATS.

    Two lessons are baked in here, both learned by blaming the firmware first:

    - The edge has to clear the playout delay (150 ms) and then be caught by a 1 Hz STATS, so the first report
      after a key-down routinely still reads 0x00. Asserting on it is simply reading too early.
    - **A waiting harness must still send.** The Rig's keepalive timeout is 1 s; a poll loop that transmits nothing
      lets the watchdog trip, which lifts the key and aborts the mark (`rig_state` bit 2). Two tests then failed
      against a key that the harness itself had caused to be released. A real Keyer sends every 250 ms; so do we.
    """
    end = time.time() + timeout
    while time.time() < end:
        link.send(link.key([edge]))                     # keepalive: same edge, so no new edge enters the stream
        st = link.stats(0.25)
        if st and st["state"] & RIG_KEY_DOWN:
            return st
    return None


def test_forged_and_limit(link):
    """Both spec §8 behaviours, read from the SERIES of STATS rather than from one sampled reading.

    A single reading cannot express either property. The mark must *survive* the forged packets - which is a
    statement about a stretch of time - and it must then be cut *at* the limit, which is a statement about when a
    flag appears. Sampling one STATS and asserting on it failed both tests repeatedly while the firmware was doing
    exactly the right thing; the trace that settled it simply printed the series.
    """
    limit_ms = link.max_keydown or 3000
    down_t = link.ticks()
    for _ in range(4):
        link.send(link.key([(down_t, 1)]))
        time.sleep(0.03)

    down_at = None          # when the Rig first reported the key down
    forged = None           # built once the key is down, then sent alongside valid keepalives
    survived_ms = 0         # how long the mark lasted while forged packets were arriving
    limit_at = None         # when bit 3 appeared
    released_at = None      # when the key went back up

    start = time.time()
    last_send = 0.0
    max_gap = 0.0                                       # the harness's own worst send interval, measured not assumed
    while time.time() - start < (limit_ms / 1000.0) + 5.0:
        now = time.time()
        if now - last_send >= 0.2:                      # send on a timer, not whenever the reads happen to allow it:
            if last_send:                               # a cadence dictated by blocking reads once starved the
                max_gap = max(max_gap, now - last_send) # stream and let the watchdog lift the key mid-test
            link.send(link.key([(down_t, 1)]))          # valid keepalive: the same edge, so no new edge is added
            if forged:
                link.send(forged)                       # ... and the bad packet, which must change nothing
            last_send = now
        st = link.stats(0.1)
        if not st:
            continue
        now = time.time()
        if st["state"] & RIG_KEY_DOWN:
            if down_at is None:
                down_at = now
                good = link.key([(link.ticks(), 0)])    # a valid key-up, corrupted: the only key-up we ever send
                forged = good[:-1] + bytes([good[-1] ^ 0xFF])
            elif forged:
                survived_ms = int((now - down_at) * 1000)
        elif down_at is not None and released_at is None:
            released_at = now
        if (st["state"] & RIG_KEYDOWN_LIMIT) and limit_at is None:
            limit_at = now
        if released_at and limit_at:
            break

    note(f"harness cadence: worst gap between sends {max_gap*1000:.0f} ms (the Rig drops the key after 1000 ms)")
    if down_at:
        note(f"mark survived {survived_ms} ms of forged key-ups"
             + (f", limit cut it {limit_at - down_at:.1f}s after key-down" if limit_at else ", limit never fired"))
    check("forged MAC: a mark was on the air", down_at is not None, "the key never went down")
    check("forged MAC: ignored - the mark outlived the forged key-ups", survived_ms >= 1000,
          f"mark held {survived_ms} ms while forged packets arrived")
    check(f"key-down limit ({limit_ms} ms): the Rig flagged it", limit_at is not None, "bit 3 never set")
    if down_at and limit_at:
        cut_after = limit_at - down_at
        check("key-down limit: cut at the limit, not late", abs(cut_after - limit_ms / 1000.0) < 1.5,
              f"{cut_after:.1f}s vs {limit_ms/1000:.1f}s")
    check("key-down limit: the key went up", released_at is not None, "still down")

    link.send(link.key([(link.ticks(), 0)]))            # release properly
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
        test_forged_and_limit(link)
        test_watchdog(link)
    finally:
        link.bye()

    print(f"\n{sum(results)}/{len(results)} passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())

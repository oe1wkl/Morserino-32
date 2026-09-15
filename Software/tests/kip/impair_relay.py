#!/usr/bin/env python3
"""M32KIP impairment relay — the netem box of spec §13.2, as a UDP relay on a laptop.

Point the Remote Keyer's TRX Peer at the machine running this, and this at the Remote Rig:

    /usr/bin/python3 impair_relay.py --rig 192.168.1.237 --psk "..." --profile b --stats b.csv

Every datagram is forwarded in both directions with the chosen impairment. The relay also holds the pre-shared
key, so it derives each session key from the HELLO / HELLO_ACK pair it forwards and logs every STATS packet the
Rig sends as a CSV row - which makes it the STATS logger for the drift test (§13.3) as well.

Standard library only, on purpose: on macOS a third-party Python is blocked from the local network by the
Local Network privacy setting, and Apple's /usr/bin/python3 is not.

Profiles (spec §13.2):
    a  50 ms delay, 10 ms jitter
    b  120 ms delay, 40 ms jitter
    c  b + 2 % loss
    d  b + 5 % loss, with reordering
    e  bursts of 3 consecutive lost packets (Keyer -> Rig), every 5 s, on a clean link
    clean  no impairment (the relay's own cost, as a baseline)

The impairment is added ON TOP of the real WiFi path, so "50 ms" means at least 50 ms. Jitter is uniform in
+/- the given value, as netem's default. Without reordering a packet is never released before the one sent
ahead of it (netem's behaviour when rate control is off is to reorder; profile d asks for it explicitly, so the
others keep order).
"""

import argparse, csv, hashlib, heapq, hmac, random, select, socket, struct, sys, time

MAGIC, VERSION = 0x4B, 0x01
PKT_HELLO, PKT_HELLO_ACK, PKT_KEY, PKT_STATS, PKT_BYE, PKT_NACK = 1, 2, 3, 4, 5, 6
NAMES = {1: "HELLO", 2: "HELLO_ACK", 3: "KEY", 4: "STATS", 5: "BYE", 6: "NACK"}

PROFILES = {
    "clean": dict(delay=0,   jitter=0,  loss=0.0, reorder=False, burst=0, burst_every=0),
    "a":     dict(delay=50,  jitter=10, loss=0.0, reorder=False, burst=0, burst_every=0),
    "b":     dict(delay=120, jitter=40, loss=0.0, reorder=False, burst=0, burst_every=0),
    "c":     dict(delay=120, jitter=40, loss=2.0, reorder=False, burst=0, burst_every=0),
    "d":     dict(delay=120, jitter=40, loss=5.0, reorder=True,  burst=0, burst_every=0),
    "e":     dict(delay=0,   jitter=0,  loss=0.0, reorder=False, burst=3, burst_every=5.0),
}


def open_packet(key, data):
    if len(data) < 20 or data[0] != MAGIC or data[1] != VERSION:
        return None
    want = hmac.new(key, data[:-8], hashlib.sha256).digest()[:8]
    if not hmac.compare_digest(want, data[-8:]):
        return None
    _, _, ptype, flags, session, seq, _ = struct.unpack_from("<BBBBIHH", data, 0)
    return {"type": ptype, "session": session, "seq": seq, "payload": data[12:-8]}


class Direction:
    """One direction of the path, with its own impairment state."""

    def __init__(self, name, delay, jitter, loss, reorder, burst=0, burst_every=0.0):
        self.name, self.delay, self.jitter, self.loss, self.reorder = name, delay, jitter, loss, reorder
        self.burst, self.burst_every = burst, burst_every
        self.next_burst = time.monotonic() + burst_every if burst else None
        self.burst_left = 0
        self.last_due = 0.0
        self.forwarded = self.dropped = 0

    def schedule(self, now):
        """Return the release time for a packet arriving now, or None to drop it."""
        if self.burst and now >= self.next_burst and self.burst_left == 0:
            self.burst_left = self.burst
            self.next_burst = now + self.burst_every
        if self.burst_left:
            self.burst_left -= 1
            self.dropped += 1
            return None
        if self.loss and random.random() * 100.0 < self.loss:
            self.dropped += 1
            return None
        due = now + (self.delay + random.uniform(-self.jitter, self.jitter)) / 1000.0
        if due < now:
            due = now
        if not self.reorder and due < self.last_due:
            due = self.last_due                       # keep order: never overtake the packet ahead
        self.last_due = max(self.last_due, due)
        self.forwarded += 1
        return due


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rig", required=True, help="the Remote Rig's address")
    ap.add_argument("--port", type=int, default=7374, help="the Rig's UDP port")
    ap.add_argument("--listen", type=int, default=7374, help="where the Keyer sends to (its TRX Peer points here)")
    ap.add_argument("--psk", required=True)
    ap.add_argument("--profile", choices=sorted(PROFILES), default="clean")
    ap.add_argument("--delay", type=float, help="override: ms each way")
    ap.add_argument("--jitter", type=float, help="override: +/- ms")
    ap.add_argument("--loss", type=float, help="override: percent")
    ap.add_argument("--reorder", action="store_true", help="override: let jitter reorder packets")
    ap.add_argument("--stats", help="CSV file for the Rig's STATS")
    ap.add_argument("--duration", type=float, default=0, help="stop after this many seconds (0 = until Ctrl-C)")
    ap.add_argument("--seed", type=int, help="random seed, for a repeatable run")
    args = ap.parse_args()
    if args.seed is not None:
        random.seed(args.seed)

    p = dict(PROFILES[args.profile])
    for k in ("delay", "jitter", "loss"):
        if getattr(args, k) is not None:
            p[k] = getattr(args, k)
    if args.reorder:
        p["reorder"] = True
    to_rig = Direction("keyer->rig", p["delay"], p["jitter"], p["loss"], p["reorder"], p["burst"], p["burst_every"])
    to_keyer = Direction("rig->keyer", p["delay"], p["jitter"], p["loss"], p["reorder"])   # bursts hit KEY traffic

    base = hashlib.sha256(args.psk.encode()).digest()
    rig = (socket.gethostbyname(args.rig), args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", args.listen))
    sock.setblocking(False)

    keyer = None
    hellos = {}                 # nonce_c -> time seen
    ksess = {}                  # session id -> session key
    pending = []                # heap of (due, n, data, dest)
    n = 0
    stats_file = open(args.stats, "w", newline="") if args.stats else None
    writer = csv.writer(stats_file) if stats_file else None
    if writer:
        writer.writerow(["t_s", "session", "playout_ms", "jitter_ms", "loss_pct", "late_edges", "underruns",
                         "rig_state", "max_keydown_ms", "dit_ms"])
    t0 = time.monotonic()
    last_report = t0
    counts = {}

    print(f"relay :{args.listen} <-> {rig[0]}:{rig[1]}  profile {args.profile}: {p}", flush=True)
    try:
        while True:
            now = time.monotonic()
            if args.duration and now - t0 >= args.duration:
                break
            timeout = 0.05
            if pending:
                timeout = max(0.0, min(timeout, pending[0][0] - now))
            r, _, _ = select.select([sock], [], [], timeout)
            now = time.monotonic()
            if r:
                while True:
                    try:
                        data, addr = sock.recvfrom(512)
                    except BlockingIOError:
                        break
                    if addr[0] == rig[0]:
                        if keyer is None:
                            continue                      # nobody to forward to yet
                        direction, dest = to_keyer, keyer
                    else:
                        keyer = addr
                        direction, dest = to_rig, rig
                    ptype = data[2] if len(data) > 2 else 0
                    counts[(direction.name, ptype)] = counts.get((direction.name, ptype), 0) + 1

                    # Watch the handshake to learn session keys; log STATS.
                    if ptype == PKT_HELLO and direction is to_rig:
                        pkt = open_packet(base, data)
                        if pkt:
                            hellos[pkt["payload"][:8]] = now
                    elif ptype == PKT_HELLO_ACK:
                        pkt = open_packet(base, data)
                        if pkt:
                            nonce_c, nonce_s = pkt["payload"][:8], pkt["payload"][8:16]
                            session = struct.unpack_from("<I", pkt["payload"], 16)[0]
                            ksess[session] = hmac.new(base, nonce_c + nonce_s, hashlib.sha256).digest()
                            print(f"{now - t0:8.1f}s  session {session:#010x} established", flush=True)
                    elif ptype == PKT_STATS and writer:
                        session = struct.unpack_from("<I", data, 4)[0]
                        key = ksess.get(session)
                        pkt = open_packet(key, data) if key else None
                        if pkt:
                            (playout, jitter, loss, late, under, rig_state, mk, dit, techo, trig) = \
                                struct.unpack("<HHBBBBHHII", pkt["payload"])
                            writer.writerow([f"{now - t0:.2f}", f"{session:#010x}", playout, jitter / 10.0, loss,
                                             late, under, rig_state, mk, dit / 10.0])
                            stats_file.flush()

                    due = direction.schedule(now)
                    if due is not None:
                        heapq.heappush(pending, (due, n, data, dest))
                        n += 1

            while pending and pending[0][0] <= time.monotonic():
                _, _, data, dest = heapq.heappop(pending)
                sock.sendto(data, dest)

            if now - last_report >= 10.0:
                last_report = now
                kr = counts.get(("keyer->rig", PKT_KEY), 0)
                st = counts.get(("rig->keyer", PKT_STATS), 0)
                print(f"{now - t0:8.1f}s  KEY in {kr}, dropped to rig {to_rig.dropped}, "
                      f"STATS {st}, dropped to keyer {to_keyer.dropped}", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        if stats_file:
            stats_file.close()
    print("\npackets seen:", {f"{d} {NAMES.get(t, t)}": c for (d, t), c in sorted(counts.items())})
    print(f"keyer->rig forwarded {to_rig.forwarded}, dropped {to_rig.dropped}; "
          f"rig->keyer forwarded {to_keyer.forwarded}, dropped {to_keyer.dropped}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

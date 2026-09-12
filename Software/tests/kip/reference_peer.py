#!/usr/bin/env python3
"""M32KIP reference peer — a working Keyer unit and Rig unit in Python.

Used to test one end of the link against something that is not the other end of the same firmware:

    # play a CW message at a real Morserino running in Rig mode
    python3 reference_peer.py keyer --host 192.168.1.23 --psk "..." --text "CQ CQ DE OE1WKL K" --wpm 25

    # act as the Rig for a Morserino running in Keyer mode, and print the key line it asks for
    python3 reference_peer.py rig --psk "..."

The wire format is implemented here independently of the C++ core (see check_vectors.py, which pins
the two against each other), so a disagreement between firmware and this script is evidence rather
than a coincidence.

The Rig role prints the reconstructed key line rather than driving hardware; --trace adds a line per
edge with its scheduled and actual time, which is what you want when a timing complaint comes in.
"""

import argparse, hashlib, hmac, random, socket, struct, sys, time

MAGIC, VERSION = 0x4B, 0x01
PKT_HELLO, PKT_HELLO_ACK, PKT_KEY, PKT_STATS, PKT_BYE, PKT_NACK = 1, 2, 3, 4, 5, 6
TICKS_PER_MS = 10
MAX_EDGES = 8

MORSE = {
    "A": ".-", "B": "-...", "C": "-.-.", "D": "-..", "E": ".", "F": "..-.", "G": "--.", "H": "....",
    "I": "..", "J": ".---", "K": "-.-", "L": ".-..", "M": "--", "N": "-.", "O": "---", "P": ".--.",
    "Q": "--.-", "R": ".-.", "S": "...", "T": "-", "U": "..-", "V": "...-", "W": ".--", "X": "-..-",
    "Y": "-.--", "Z": "--..", "0": "-----", "1": ".----", "2": "..---", "3": "...--", "4": "....-",
    "5": ".....", "6": "-....", "7": "--...", "8": "---..", "9": "----.", "/": "-..-.", "?": "..--..",
    "=": "-...-", ".": ".-.-.-", ",": "--..--", "+": ".-.-.",
}


def now_ticks(t0):
    return int((time.monotonic() - t0) * 10000) & 0xFFFFFFFF


def tick_diff(a, b):
    d = (a - b) & 0xFFFFFFFF
    return d - (1 << 32) if d >= (1 << 31) else d


class Codec:
    def __init__(self, key):
        self.key = key

    def seal(self, body):
        return body + hmac.new(self.key, body, hashlib.sha256).digest()[:8]

    def open(self, data):
        if len(data) < 20 or data[0] != MAGIC or data[1] != VERSION:
            return None
        want = hmac.new(self.key, data[:-8], hashlib.sha256).digest()[:8]
        if not hmac.compare_digest(want, data[-8:]):
            return None
        _, _, ptype, flags, session, seq, _ = struct.unpack_from("<BBBBIHH", data, 0)
        return {"type": ptype, "flags": flags, "session": session, "seq": seq, "payload": data[12:-8]}

    @staticmethod
    def header(ptype, session, seq, flags=0):
        return struct.pack("<BBBBIHH", MAGIC, VERSION, ptype, flags, session, seq, 0)


def cw_edges(text, wpm, start_tick):
    """The element stream of `text` as (tick, state) edges, exactly as an M32 keyer would emit it."""
    dit = 12000 // wpm
    t, edges = start_tick, []
    for word in text.upper().split():
        for ci, ch in enumerate(word):
            pattern = MORSE.get(ch)
            if not pattern:
                continue
            for ei, sym in enumerate(pattern):
                length = dit * (3 if sym == "-" else 1)
                edges.append((t, 1))
                t += length
                edges.append((t, 0))
                t += dit if ei + 1 < len(pattern) else 0
            t += 2 * dit          # to a full inter-character gap of 3 dits
        t += 4 * dit              # to a full word gap of 7 dits
    return edges


def run_keyer(args):
    base = hashlib.sha256(args.psk.encode()).digest()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.5)
    peer = (args.host, args.port)
    t0 = time.monotonic()

    nonce_c = bytes(random.getrandbits(8) for _ in range(8))
    hello = Codec(base)
    session, ksess, ack = 0, None, None
    for attempt in range(10):
        body = (Codec.header(PKT_HELLO, 0, attempt + 1) + nonce_c
                + struct.pack("<IBBBB", now_ticks(t0), args.wpm, args.redundancy, 0, 0))
        sock.sendto(hello.seal(body), peer)
        try:
            data, _ = sock.recvfrom(256)
        except socket.timeout:
            continue
        pkt = hello.open(data)
        if not pkt or pkt["type"] != PKT_HELLO_ACK:
            continue
        echoed, nonce_s = pkt["payload"][:8], pkt["payload"][8:16]
        if echoed != nonce_c:
            continue                                     # not an answer to our HELLO
        session, max_keydown, ptt_lead, flags = struct.unpack_from("<IHBB", pkt["payload"], 16)
        ksess = hmac.new(base, nonce_c + nonce_s, hashlib.sha256).digest()
        ack = (max_keydown, ptt_lead, flags)
        break
    if ksess is None:
        print("no HELLO_ACK — wrong host, wrong PSK, or the Rig unit is not listening", file=sys.stderr)
        return 1
    print(f"session {session:#010x}, max key-down {ack[0]} ms, PTT lead {ack[1]} ms")

    codec = Codec(ksess)
    edges = cw_edges(args.text, args.wpm, now_ticks(t0) + TICKS_PER_MS * 500)
    history, seq = [], 0
    sent = 0
    next_keepalive = time.monotonic()

    while edges or time.monotonic() < next_keepalive + 1.0:
        tick = now_ticks(t0)
        while edges and tick_diff(tick, edges[0][0]) >= 0:
            history.append(edges.pop(0))
            del history[:-MAX_EDGES]
            seq += 1
            take = history[-args.redundancy:]
            body = (Codec.header(PKT_KEY, session, seq & 0xFFFF)
                    + struct.pack("<IBBBB", tick, args.wpm, len(take), 0, 0)
                    + b"".join(struct.pack("<IB", et, st) for et, st in take))
            sock.sendto(codec.seal(body), peer)
            sent += 1
        if time.monotonic() >= next_keepalive:
            next_keepalive = time.monotonic() + 0.25
            take = history[-(args.redundancy - 1):] if args.redundancy > 1 else []
            seq += 1
            body = (Codec.header(PKT_KEY, session, seq & 0xFFFF)
                    + struct.pack("<IBBBB", now_ticks(t0), args.wpm, len(take), 0, 0)
                    + b"".join(struct.pack("<IB", et, st) for et, st in take))
            sock.sendto(codec.seal(body), peer)
        try:
            data, _ = sock.recvfrom(256)
            pkt = codec.open(data)
            if pkt and pkt["type"] == PKT_STATS:
                (playout, jitter, loss, late, under, rig_state, mk, dit, techo, trig) = \
                    struct.unpack("<HHBBBBHHII", pkt["payload"])
                print(f"  STATS D={playout} ms jitter={jitter/10:.1f} ms loss={loss}% late={late} "
                      f"underruns={under} rig={rig_state:#04x} dit={dit/10:.1f} ms")
        except socket.timeout:
            pass
        time.sleep(0.001)

    for _ in range(3):
        seq += 1
        sock.sendto(codec.seal(Codec.header(PKT_BYE, session, seq & 0xFFFF)), peer)
        time.sleep(0.1)
    print(f"sent {sent} KEY packets for {len(cw_edges(args.text, args.wpm, 0))} edges")
    return 0


def run_rig(args):
    base = hashlib.sha256(args.psk.encode()).digest()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", args.port))
    sock.settimeout(0.05)
    t0 = time.monotonic()
    print(f"listening on UDP {args.port}")

    session, ksess, codec, peer = 0, None, None, None
    queue, off, D = [], 0, TICKS_PER_MS * args.playout
    key_state, started = 0, False
    last_stats = time.monotonic()

    while True:
        try:
            data, addr = sock.recvfrom(256)
        except socket.timeout:
            data = None
        if data:
            pkt = Codec(base).open(data)
            if pkt and pkt["type"] == PKT_HELLO:
                nonce_c = pkt["payload"][:8]
                nonce_s = bytes(random.getrandbits(8) for _ in range(8))
                session = random.getrandbits(31) | 1
                ksess = hmac.new(base, nonce_c + nonce_s, hashlib.sha256).digest()
                codec, peer, started = Codec(ksess), addr, False
                queue.clear()
                src = pkt["payload"][14]
                max_keydown = 3000 if src == 0 else 10000
                body = (Codec.header(PKT_HELLO_ACK, session, 1) + nonce_c + nonce_s
                        + struct.pack("<IHBB", session, max_keydown, 0, 0))
                sock.sendto(Codec(base).seal(body), addr)
                print(f"session {session:#010x} opened by {addr[0]}, source {src}, max key-down {max_keydown} ms")
            elif codec:
                pkt = codec.open(data)
                if not pkt:
                    pass
                elif pkt["type"] == PKT_BYE:
                    print("BYE — key up, session closed")
                    key_state, codec, queue = 0, None, []
                elif pkt["type"] == PKT_KEY:
                    t_now, wpm, n, src, _ = struct.unpack_from("<IBBBB", pkt["payload"], 0)
                    rig_now = now_ticks(t0)
                    if not started:
                        off, started = (rig_now - t_now) & 0xFFFFFFFF, True
                    for i in range(n):
                        et, st = struct.unpack_from("<IB", pkt["payload"], 8 + 5 * i)
                        if not any(q[0] == et for q in queue):
                            queue.append((et, st))
                    queue.sort(key=lambda q: q[0])

        rig_now = now_ticks(t0)
        while queue and tick_diff(rig_now, (queue[0][0] + off + D) & 0xFFFFFFFF) >= 0:
            et, st = queue.pop(0)
            if st != key_state:
                key_state = st
                if args.trace:
                    print(f"  {'DOWN' if st else 'UP  '} at {rig_now/10:.1f} ms "
                          f"(scheduled {((et + off + D) & 0xFFFFFFFF)/10:.1f})")
                else:
                    sys.stdout.write("=" if st else " ")
                    sys.stdout.flush()

        if codec and peer and time.monotonic() - last_stats >= 1.0:
            last_stats = time.monotonic()
            body = (Codec.header(PKT_STATS, session, 0)
                    + struct.pack("<HHBBBBHHII", D // TICKS_PER_MS, 0, 0, 0, 0,
                                  1 if key_state else 0, 3000, 0, 0, now_ticks(t0)))
            sock.sendto(codec.seal(body), peer)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("role", choices=["keyer", "rig"])
    ap.add_argument("--host", default="127.0.0.1", help="the Rig unit (keyer role only)")
    ap.add_argument("--port", type=int, default=7374)
    ap.add_argument("--psk", required=True)
    ap.add_argument("--text", default="CQ CQ DE OE1WKL K")
    ap.add_argument("--wpm", type=int, default=25)
    ap.add_argument("--redundancy", type=int, default=4)
    ap.add_argument("--playout", type=int, default=150, help="playout delay in ms (rig role)")
    ap.add_argument("--trace", action="store_true", help="one line per reproduced edge (rig role)")
    args = ap.parse_args()
    try:
        return run_keyer(args) if args.role == "keyer" else run_rig(args)
    except KeyboardInterrupt:
        print()
        return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Ask a Rig unit whether it is free, and show exactly what it answers.

Sends one authenticated HELLO (up to five attempts) and prints the reply by name:

    NACK reason 1 (busy)   another Keyer holds the session - the normal proof that a
                           Remote Keyer is linked, without touching either unit (spec §8)
    NACK reason 2/3        version / authentication refused
    HELLO_ACK              the Rig was FREE and has just admitted *this Mac*
    (no reply)             wrong address or port, Rig not in Remote Rig, a pass phrase
                           mismatch (the Rig drops what it cannot authenticate), or the
                           Mac's Local Network permission (see below)

reference_peer.py cannot tell these apart: it skips every reply that is not a HELLO_ACK
and reports "no HELLO_ACK" for a correct busy refusal too.

A won session is released at once with three BYEs. That matters: an earlier probe that
kept its session (or lost its one BYE) blocked the real Keyer, which then reported
"No answer" while the Rig showed a live session (TEST_REPORT.md). Still, the probe is
only read-only when the Rig is busy - don't fire it while a Keyer is starting.

    python3 hello_probe.py <rig address> --psk "<pass phrase>"     (or M32KIP_PSK=...)

Run it from Terminal: from a sandboxed tool shell, macOS Local Network privacy turns every
UDP send to the LAN into "No route to host" while ping still works.
"""

import argparse, hashlib, hmac, os, random, socket, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from reference_peer import Codec, now_ticks, PKT_HELLO, PKT_HELLO_ACK, PKT_BYE, PKT_NACK

NAMES = {1: "HELLO", 2: "HELLO_ACK", 3: "KEY", 4: "STATS", 5: "BYE", 6: "NACK"}
REASONS = {1: "busy", 2: "version", 3: "auth"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=7374)
    ap.add_argument("--psk", default=os.environ.get("M32KIP_PSK"))
    args = ap.parse_args()
    if not args.psk:
        ap.error("pass phrase needed: --psk or M32KIP_PSK")

    base = hashlib.sha256(args.psk.encode()).digest()
    hello = Codec(base)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    peer = (args.host, args.port)
    t0 = time.monotonic()
    nonce_c = bytes(random.getrandbits(8) for _ in range(8))

    for attempt in range(1, 6):
        body = (Codec.header(PKT_HELLO, 0, attempt) + nonce_c
                + struct.pack("<IBBBB", now_ticks(t0), 25, 4, 0, 0))
        try:
            sock.sendto(hello.seal(body), peer)
        except OSError as e:
            print(f"send failed: {e}  (from a tool shell this is usually macOS Local Network privacy)")
            return 2
        try:
            data, src = sock.recvfrom(256)
        except socket.timeout:
            print(f"attempt {attempt}: no reply")
            continue
        pkt = hello.open(data)
        if not pkt:
            print(f"attempt {attempt}: {len(data)} bytes from {src[0]}, not authenticated by this pass phrase")
            continue
        kind = NAMES.get(pkt["type"], pkt["type"])
        if pkt["type"] == PKT_NACK:
            reason = pkt["payload"][0] if pkt["payload"] else None
            print(f"NACK from {src[0]}: reason {reason} ({REASONS.get(reason, 'unknown')})")
            return 0 if reason == 1 else 1
        if pkt["type"] == PKT_HELLO_ACK and pkt["payload"][:8] == nonce_c:
            session = struct.unpack_from("<I", pkt["payload"], 16)[0]
            ksess = hmac.new(base, nonce_c + pkt["payload"][8:16], hashlib.sha256).digest()
            bye = Codec(ksess)
            for seq in range(3):                      # one lost BYE would leave this Mac holding the Rig
                sock.sendto(bye.seal(Codec.header(PKT_BYE, session, seq)), peer)
                time.sleep(0.02)
            print(f"HELLO_ACK from {src[0]}: the Rig was FREE - no Keyer linked. Session {session:#010x} released (3x BYE).")
            return 3
        print(f"attempt {attempt}: unexpected {kind} from {src[0]}")
    print("no reply: wrong address/port, Rig not in Remote Rig, or a pass phrase mismatch")
    return 4


if __name__ == "__main__":
    sys.exit(main())

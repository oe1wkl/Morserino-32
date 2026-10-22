#!/usr/bin/env python3
"""Cross-check the M32KIP wire format against an independent implementation.

`test_core --vectors` prints packets built by the firmware's own C++ codec. This script rebuilds
the same packets from the field values using Python's struct, hashlib and hmac, and compares byte
for byte. Two implementations agreeing is what makes the layout and the MAC derivation trustworthy;
the C++ tests on their own would only prove the codec is self-consistent.

    python3 check_vectors.py            # builds test_core via make if needed
"""

import hashlib, hmac, struct, subprocess, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
MAGIC, VERSION = 0x4B, 0x01
PKT_HELLO, PKT_HELLO_ACK, PKT_KEY, PKT_STATS, PKT_BYE, PKT_NACK = 1, 2, 3, 4, 5, 6


def header(ptype, session, seq, flags=0):
    return struct.pack("<BBBBIHH", MAGIC, VERSION, ptype, flags, session, seq, 0)


def seal(body, key):
    return body + hmac.new(key, body, hashlib.sha256).digest()[:8]


def build_expected():
    psk = b"morserino keying over ip"
    kbase = hashlib.sha256(psk).digest()

    nonce_c = bytes(range(0x31, 0x39))
    nonce_s = bytes(range(0x91, 0x99))
    ksess = hmac.new(kbase, nonce_c + nonce_s, hashlib.sha256).digest()

    out = {"psk": psk.decode(), "kbase": kbase.hex(), "ksess": ksess.hex()}

    # HELLO — authenticated with the base key, session 0, seq 1
    body = header(PKT_HELLO, 0, 1) + nonce_c + struct.pack("<IBBBB", 0x01020304, 25, 4, 0, 0)
    out["hello"] = seal(body, kbase).hex()

    # everything after the handshake uses the session key
    sess_id, seq = 0x0A0B0C0D, 4242

    body = (header(PKT_HELLO_ACK, sess_id, 1) + nonce_c + nonce_s
            + struct.pack("<IHBB", sess_id, 3000, 0, 0))
    out["helloack"] = seal(body, ksess).hex()

    edges = b"".join(struct.pack("<IB", 0x000FFF00 + i * 480, 1 - (i & 1)) for i in range(4))
    body = header(PKT_KEY, sess_id, seq) + struct.pack("<IBBBB", 0x00100000, 25, 4, 0, 0) + edges
    out["key"] = seal(body, ksess).hex()

    body = (header(PKT_STATS, sess_id, seq)
            + struct.pack("<HHBBBBHHII", 150, 88, 1, 0, 0, 0x01, 3000, 480, 0x00100000, 0x00200000))
    out["stats"] = seal(body, ksess).hex()

    out["bye"] = seal(header(PKT_BYE, sess_id, seq), ksess).hex()
    return out


def main():
    subprocess.run(["make", "test_core"], cwd=HERE, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    proc = subprocess.run([os.path.join(HERE, "test_core"), "--vectors"],
                          cwd=HERE, check=True, capture_output=True, text=True)
    got = {}
    for line in proc.stdout.splitlines():
        if not line.strip():
            continue
        name, _, value = line.partition(" ")
        got[name] = value

    want = build_expected()
    failures = 0
    for name in ("psk", "kbase", "ksess", "hello", "helloack", "key", "stats", "bye"):
        if name not in got:
            print(f"  FAIL  {name}: the C++ side printed nothing")
            failures += 1
        elif got[name] != want[name]:
            print(f"  FAIL  {name}\n        C++    {got[name]}\n        Python {want[name]}")
            failures += 1

    # the same length rules the C++ tests assert, checked here against the spec's own tables
    lengths = {"hello": 12 + 16 + 8, "helloack": 12 + 24 + 8, "key": 12 + 8 + 4 * 5 + 8,
               "stats": 12 + 20 + 8, "bye": 12 + 8}
    for name, expect in lengths.items():
        if name in got and len(got[name]) // 2 != expect:
            print(f"  FAIL  {name} is {len(got[name])//2} bytes, the spec says {expect}")
            failures += 1

    print(f"\n{len(want)} vectors checked against Python, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

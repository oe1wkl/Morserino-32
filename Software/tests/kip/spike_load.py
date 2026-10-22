#!/usr/bin/env python3
"""WiFi load generator for the M32KIP Phase 0 spike (M32KipSpike.cpp).

Sends UDP packets to the device at a fixed rate; the spike echoes each one back.
Reports loss and round-trip statistics, which double as a first look at the LAN's
delay and jitter for the playout-delay estimator (spec §7.2/7.3).

    python3 spike_load.py <device-ip> [--pps 200] [--seconds 60] [--size 60]
"""
import argparse, socket, struct, time, statistics, sys

ap = argparse.ArgumentParser()
ap.add_argument("ip")
ap.add_argument("--port", type=int, default=7374)
ap.add_argument("--pps", type=int, default=200)
ap.add_argument("--seconds", type=float, default=60)
ap.add_argument("--size", type=int, default=60, help="payload bytes (>= 12)")
a = ap.parse_args()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setblocking(False)
interval = 1.0 / a.pps
sent = {}
rtts = []
t_end = time.monotonic() + a.seconds
seq = 0
next_tx = time.monotonic()
last_report = time.monotonic()
padding = b"K" * max(0, a.size - 12)

def drain():
    while True:
        try:
            data, _ = sock.recvfrom(2048)
        except BlockingIOError:
            return
        if len(data) < 12:
            continue
        s, t_tx = struct.unpack_from("<Id", data, 0)
        if s in sent:
            rtts.append((time.monotonic() - sent.pop(s)) * 1000.0)

while time.monotonic() < t_end:
    now = time.monotonic()
    if now >= next_tx:
        sent[seq] = now
        sock.sendto(struct.pack("<Id", seq, now) + padding, (a.ip, a.port))
        seq += 1
        next_tx += interval
    drain()
    if now - last_report >= 5:
        last_report = now
        print(f"sent {seq:6d}  echoed {len(rtts):6d}  outstanding {len(sent):4d}", file=sys.stderr)
    time.sleep(0.0005)

time.sleep(0.5)
drain()
lost = len(sent)
print(f"\nsent {seq}, echoed {len(rtts)}, lost {lost} ({100.0*lost/max(1,seq):.2f} %)")
if rtts:
    rtts.sort()
    pct = lambda p: rtts[min(len(rtts)-1, int(p * len(rtts)))]
    print(f"RTT ms: min {rtts[0]:.2f}  median {pct(0.5):.2f}  p90 {pct(0.9):.2f}  p99 {pct(0.99):.2f}  max {rtts[-1]:.2f}")
    print(f"jitter (p99 - min): {pct(0.99)-rtts[0]:.2f} ms")

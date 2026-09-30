#!/usr/bin/env python3
"""Connect + enable notifications on EVERY device advertising the NUS service,
one by one, and report each by address. No handshake, so no FN press needed.

The quickest way to tell a stuck Morserino from a stuck Mac: on 2026-09-30 the
Pocket connected in 1.5 s while the classic stalled 31 s and dropped, from the
same Mac at the same time. Both advertise as 'Morserino-32' - go by address.
Run from macOS Terminal (Bluetooth permission, see ble_bench_common.py).
"""
import asyncio, time
from bleak import BleakScanner, BleakClient
NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

async def main():
    found = await BleakScanner.discover(timeout=12, return_adv=True)
    cands = [(d, a) for d, a in found.values() if NUS in [u.lower() for u in a.service_uuids]]
    print("devices offering the NUS service:", [(d.name, d.address, a.rssi) for d, a in cands], flush=True)
    for d, a in cands:
        t = time.time()
        try:
            c = BleakClient(d); await c.connect(timeout=12)
            await c.start_notify(TX, lambda h, x: None)
            print(f"  {d.address}: connect + notify OK in {time.time()-t:.1f}s", flush=True)
            await c.disconnect()
        except Exception as e:
            print(f"  {d.address}: FAILED after {time.time()-t:.1f}s: {type(e).__name__}: {e}", flush=True)

asyncio.run(main())

#!/usr/bin/env python3
"""BLE Serial across repeated WiFi suspend/resume (matrix step 10).

Each cycle: settle at the top menu, start 'Disp MAC Addr' over USB (brings the
WiFi radio up, so BLE Serial suspends), record the start ack, the BLE suspend
notice and the link drop, then reconnect and re-handshake over BLE. Someone
must be at the device: a reconnect outside the 60 s consent grace window asks
for FN again (the Mac says so).

The settle matters: PUT menu/start is silently ignored while the device is not
yet back at the menu (TODO B3), which earlier made notices look "lost".

Usage:  M32_PORT=/dev/cu.usbserial-0001 python3 wifi_cycles.py [cycles]
Found 2026-09-30 (classic V2, firmware 10.0 beta): notice in 0.13 s every
cycle; after ~16 cycles in one power-on the classic accepted connections but
stalled them (31 s, then dropped) until hard reset - the per-cycle BLE heap
leak, see DESIGN.md.
"""
from ble_bench_common import *

CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 10

async def main():
    usb = M32Usb(port=os.environ.get("M32_PORT", "/dev/cu.usbserial-0001"))
    time.sleep(0.3); usb.pump(0.3); usb.objects.clear(); usb.raw = ""
    ble = Ble()
    await ble.connect()
    dev = await ble_hs(ble, "Press F N")
    check("H1: handshake after FN", dev is not None and "device" in dev)
    usb.send("PUT device/protocol/on"); usb.wait_for("device", 4)
    usb.send("GET menus"); menus_u = usb.wait_for("menus", 8)
    mac_no = next(m["menu number"] for m in menus_u["menus"] if "MAC" in m["content"])
    results = []
    for cyc in range(CYCLES):
        await asyncio.sleep(5.0)                       # settle at the top menu
        usb.objects.clear(); ble.stream.objects.clear(); ble.disconnected.clear()
        t0 = time.time()
        usb.send(f"PUT menu/start now/{mac_no}")
        ack = None
        for _ in range(30):
            usb.pump(0.1)
            if usb.objects: ack = list(usb.objects[0].keys()); break
        notice = None; tn = None
        while time.time() - t0 < 8 and notice is None:
            for i, o in enumerate(ble.stream.objects):
                if "message" in o: notice = ble.stream.objects.pop(i); tn = time.time() - t0; break
            await asyncio.sleep(0.02)
        try: await asyncio.wait_for(ble.disconnected.wait(), 8)
        except asyncio.TimeoutError: pass
        dropped = ble.disconnected.is_set()
        print(f"   cycle {cyc+1}: start-ack={ack} notice={'%.2fs %s' % (tn, json.dumps(notice)[:60]) if notice else 'NO'} dropped={dropped}", flush=True)
        results.append((bool(notice), dropped))
        if not dropped:
            print("   BLE did not drop - aborting"); break
        usb.wait_for("message", 6); await asyncio.sleep(2.0)
        try:
            await ble.connect(timeout=25)
            rr = await ble_hs(ble, f"Cycle {cyc+1}")
            if rr is None or "device" not in rr: print("   handshake failed:", rr); break
        except Exception as e:
            print(f"   reconnect failed: {type(e).__name__}: {e}"); break
    n = len(results)
    check("E1: every cycle suspended and reconnected", n == CYCLES and all(r[1] for r in results), f"{sum(r[1] for r in results)}/{CYCLES}")
    check("E2: suspend notice received every cycle", n == CYCLES and all(r[0] for r in results), f"{sum(r[0] for r in results)}/{CYCLES}")
    try:
        await ble.send("put device/protocol/off"); await ble.wait_for("end m32protocol", 3)
        await ble.disconnect()
    except Exception: pass
    usb.send("PUT device/protocol/off"); usb.wait_for("end m32protocol", 3); usb.close()
    sys.exit(summary())

asyncio.run(main())

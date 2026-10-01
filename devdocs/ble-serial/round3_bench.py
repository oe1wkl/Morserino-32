#!/usr/bin/env python3
"""BLE Serial hardware regression, round 3 (2026-09-30): round2_regression.py
brought up to date with ACCESS_CONTROL.md and the snapshot-blob rework.

USB and BLE clients attached simultaneously. Someone must stand at the device:
the Mac says when to press FN (about ten times), and T0 needs NO press.
  T0  fail closed: an unanswered request is declined after ~20 s
  A   session-control isolation across transports (both directions)
  B   multi-chunk GET menus, GET capabilities, paginated GET configs/details,
      remote keyer start, cw/play echo, batched write, DEVICE BUSY + recovery
  C   Bluetooth Use via PUT config stops/restarts BLE; snapshot recall leaves
      the selector and the link alone (Bluetooth Use is not in a snapshot)
  D   congestion: no loop stall, backoff clears
  E   WiFi suspend/resume x5 via remotely executed 'Disp MAC Addr'

Usage:  M32_PORT=/dev/cu.usbserial-0001 python3 round3_bench.py
Leaves Bluetooth Use = 5. Known harness limit: E starts the next cycle after a
fixed pause; PUT menu/start is silently ignored if the device is not yet back
at the menu (TODO B3), so prefer wifi_cycles.py for counting notices.
"""
from ble_bench_common import *

async def main():
    usb = M32Usb(port=os.environ.get("M32_PORT", "/dev/cu.usbserial-0001"))
    time.sleep(0.3); usb.pump(0.3); usb.objects.clear(); usb.raw = ""
    ble = Ble()

    # ---------- setup: BLE session first, then USB handshake under BLE watch —
    await ble.connect()
    # T0: fail closed - an UNANSWERED request must be declined after the window
    ble.stream.objects.clear()
    await ble.send("PUT device/protocol/on")
    cue("Test zero. Do not press anything for about twenty five seconds")
    t0 = time.time(); got = None; confirm = False
    while time.time() - t0 < 45 and got is None:
        for i, o in enumerate(ble.stream.objects):
            if "message" in o and "CONFIRM" in json.dumps(o): confirm = True; ble.stream.objects.pop(i); break
            if "error" in o or "device" in o: got = ble.stream.objects.pop(i); break
        await asyncio.sleep(0.05)
    dt = time.time() - t0
    check("T0a: client is told CONFIRM ON DEVICE", confirm)
    check("T0b: unanswered request declined (fail closed)", got is not None and "DECLINED" in json.dumps(got), f"{json.dumps(got)[:60] if got else 'none'} after {dt:.1f}s")
    dev = await ble_hs(ble, "Setup")
    check("setup: BLE handshake after FN", dev is not None and "device" in dev, json.dumps(dev)[:80] if dev else "no device object")

    usb.send("PUT device/protocol/on")
    dev_u = usb.wait_for("device", 4)
    ble_leak = not await ble.silent_for("device", 2.0)
    check("A1: USB handshake reply reaches USB", dev_u is not None)
    check("A2: USB handshake reply does NOT leak to BLE (fix 5)", not ble_leak)

    usb.send("PUT menu/stop")          # leave whatever mode the scripted suite left us in
    usb.wait_for("ok", 3); usb.objects.clear(); ble.stream.objects.clear()

    # ---------- A: BLE session end must stay on BLE ----------
    await ble.send("put device/protocol/off")
    bye_b = await ble.wait_for("end m32protocol")
    usb_leak = not usb_silent_for(usb, "end m32protocol", 2.0)
    check("A3: BLE goodbye on BLE", bye_b is not None)
    check("A4: BLE goodbye does NOT leak to USB (fix 5)", not usb_leak)
    usb.send("GET control/speed")
    check("A5: USB session alive after BLE off", usb.wait_for("control", 3) is not None)

    # mixed-case re-handshake + already-on re-ack + bogus value, all BLE-only
    a6 = await ble_hs(ble, "A six", line="PUT Device/Protocol/ON")
    check("A6: mixed-case BLE re-handshake (FN)", a6 is not None and "device" in a6)
    check("A7: BLE handshake reply does NOT leak to USB", usb_silent_for(usb, "device", 1.5))
    await ble.send("put device/protocol/on")
    check("A8: already-on re-ack over BLE (new intercept)", (await ble.wait_for("device")) is not None)
    await ble.send("put device/protocol/banana")
    err = await ble.wait_for("error")
    check("A9: bogus protocol value answered on BLE", err is not None and "INVALID Value" in json.dumps(err), json.dumps(err) if err else "none")
    check("A10: bogus-value error does NOT leak to USB", usb_silent_for(usb, "error", 1.5))

    # USB off (lowercase) must not end/notify the BLE session
    usb.send("PUT device/protocol/off")
    check("A11: USB goodbye on USB", usb.wait_for("end m32protocol", 3) is not None)
    check("A12: USB goodbye does NOT leak to BLE (fix 5)", await ble.silent_for("end m32protocol", 2.0))
    await ble.send("GET control/speed")
    check("A13: BLE session alive after USB off", (await ble.wait_for("control")) is not None)
    usb.send("PUT device/protocol/on"); usb.wait_for("device", 3)

    # ---------- B: basics over the confirmed BLE session ----------
    ble.stream.objects.clear()
    await ble.send("GET menus"); menus = await ble.wait_for("menus", 10)
    check("B1: multi-chunk GET menus reassembles", menus is not None and len(menus.get("menus", [])) > 20, f"{len(menus['menus']) if menus else 0} menus")
    keyer = next((m["menu number"] for m in (menus or {}).get("menus", []) if m["content"].strip().lower().endswith("cw keyer")), None)
    await ble.send("GET capabilities"); caps = await ble.wait_for("capabilities", 4)
    check("B2: GET capabilities over BLE", caps is not None and "kip" in json.dumps(caps), json.dumps(caps)[:90] if caps else "none")
    pages = 0; total = None; frm = 0; more = True; ok_pages = True
    while more and pages < 12:
        await ble.send("GET configs/details" + (f"/{frm}" if frm else ""))
        pg = await ble.wait_for("configdetails", 10)
        if pg is None: ok_pages = False; break
        d = pg["configdetails"]; pages += 1; total = d.get("total"); frm = d["from"] + d["count"]; more = d.get("more", False)
    check("B3: paginated GET configs/details over BLE", ok_pages and total and frm == total, f"{pages} pages, {frm}/{total} parameters")
    ble.stream.echo = ""
    await ble.send(f"PUT menu/start now/{keyer}")
    act = None
    for _ in range(80):
        for i, o in enumerate(ble.stream.objects):
            if "activate" in o or "menu" in o: act = ble.stream.objects.pop(i); break
        if act: break
        await asyncio.sleep(0.05)
    check("B4: PUT menu/start now/<keyer> answered over BLE", act is not None)
    await ble.send("PUT cw/play/CQ CQ CQ DE W2ASM"); await asyncio.sleep(7.0)
    check("B5: cw/play echo arrives over BLE", "CQ" in ble.stream.echo.upper(), repr(ble.stream.echo[:40]))
    ble.stream.echo = ""; ble.stream.objects.clear()
    await ble.client.write_gatt_char(NUS_RX, b"PUT cw/play/HI\nGET control/speed\n", response=True)
    check("B6: batched single write executes both lines", (await ble.wait_for("control", 8)) is not None)
    await asyncio.sleep(2.0); await ble.send("PUT cw/stop"); await asyncio.sleep(0.5)
    # busy refusal: end the session while a mode is running, then ask again
    ble.stream.objects.clear()
    await ble.send("put device/protocol/off"); await ble.wait_for("end m32protocol", 3)
    await ble.send("PUT device/protocol/on"); busy = await ble.wait_for("error", 4)
    check("B7: handshake while a mode runs is refused at once (DEVICE BUSY)", busy is not None and "BUSY" in json.dumps(busy).upper(), json.dumps(busy) if busy else "none")
    usb.send("PUT menu/stop"); usb.wait_for("ok", 3); usb.objects.clear()
    await asyncio.sleep(1.0)
    r = await ble_hs(ble, "B eight")
    check("B8: handshake at the top menu succeeds after the busy refusal (FN)", r is not None and "device" in r)

    # ---------- C: fix 2 — snapshot recall runs the change-switch ----------
    usb.objects.clear()
    usb.send("GET snapshots")
    snaps = usb.wait_for("snapshots", 4)
    print("   current snapshots:", json.dumps(snaps))
    existing = set()
    if snaps:
        # tolerate either a list of numbers or a list of objects
        for e in (snaps.get("snapshots") if isinstance(snaps.get("snapshots"), list) else []):
            if isinstance(e, int): existing.add(e)
            elif isinstance(e, dict) and "number" in e: existing.add(e["number"])
    slot = next(n for n in range(1, 9) if n not in existing)
    print(f"   using free snapshot slot {slot} (existing: {sorted(existing) or 'none'})")

    usb.send("PUT config/Bluetooth Use/0")
    ok0 = usb.wait_for("ok", 4)
    gotmsg = await ble.wait_for("message", 4)          # "BLE serial off", BleOnly
    try:
        await asyncio.wait_for(ble.disconnected.wait(), 6)
        dropped = True
    except asyncio.TimeoutError:
        dropped = ble.disconnected.is_set()
    check("C1: selector->0 via PUT config stops BLE (notice + link drop)",
          ok0 is not None and dropped, f"notice={json.dumps(gotmsg) if gotmsg else 'MISSING'}")

    usb.send(f"PUT snapshot/store/{slot}")
    ok_store = usb.wait_for("ok", 8)
    if ok_store is None:
        print("   C2 diag: usb.raw tail:", repr(usb.raw[-200:]), "depth:", usb.depth,
              "torn:", usb.resync(), "pending:", usb.objects[:3])
    check("C2: snapshot stored with selector=0", ok_store is not None)

    usb.send("PUT config/Bluetooth Use/5")
    usb.wait_for("ok", 4)
    await asyncio.sleep(1.5)                            # top-menu backstop restarts BLE
    try:
        await ble.connect(timeout=20)
        r3 = await ble_hs(ble, "C three")
        re_ok = r3 is not None and "device" in r3
    except RuntimeError:
        re_ok = False
    check("C3: selector->5 restarts BLE (re-advertise + handshake)", re_ok)

    usb.objects.clear(); ble.stream.objects.clear(); ble.disconnected.clear()
    usb.send(f"PUT snapshot/recall/{slot}")
    ok_r = usb.wait_for("ok", 4)
    await asyncio.sleep(3.0)
    check("C4: PUT snapshot/recall leaves BLE up (selector is not part of a snapshot)", ok_r is not None and not ble.disconnected.is_set())
    usb.send("GET configs"); cfgs = usb.wait_for("configs", 5)
    val = next((c["value"] for c in cfgs["configs"] if c["name"] == "Bluetooth Use"), None) if cfgs else None
    check("C5: selector still 5 after recall", val == 5, f"value={val}")
    usb.send(f"PUT snapshot/clear/{slot}"); ok_c = usb.wait_for("ok", 4)
    await ble.send("GET control/speed")
    check("C6: cleanup - slot cleared, BLE session alive", ok_c is not None and (await ble.wait_for("control", 4)) is not None)

    # ---------- D: congestion — no stall, backoff clears ----------
    usb.raw = ""
    ble.stream.objects.clear()
    for _ in range(8):
        await ble.send("GET configs")                   # ~2.9 KB each, back to back
    t0 = time.time()
    usb.send("GET control/speed")
    ctl = usb.wait_for("control", 2.0)
    usb_latency = time.time() - t0
    check("D1: USB responsive during BLE TX flood (no loop stall)",
          ctl is not None and usb_latency < 1.0, f"latency={usb_latency*1000:.0f} ms")
    await asyncio.sleep(6)                              # let the flood settle / ring drain
    intact = sum(1 for o in ble.stream.objects if "configs" in o)
    torn = sum(1 for o in ble.stream.objects if "_torn" in o)
    dangling = ble.stream.resync()                      # a dropped tail leaves unbalanced braces: client
    if dangling: torn += 1                              # must resync by timeout (or reconnect)
    ble.stream.objects.clear()
    await ble.send("GET control/speed")
    after = await ble.wait_for("control", 4)
    recovery = "in-place"
    if after is None:                                   # documented client fallback: reconnect = fresh session
        await ble.disconnect()
        await asyncio.sleep(1.0)
        await ble.connect(timeout=15)
        await ble_hs(ble, "D recovery")
        await ble.send("GET control/speed")
        after = await ble.wait_for("control", 4)
        recovery = "after reconnect"
    check("D2: BLE usable after congestion episode (backoff cleared)", after is not None,
          f"flood result: {intact} intact configs, {torn} torn/dangling (drop-don't-stall is the documented policy); recovered {recovery}")
    usb.pump(1.0)                                       # actually collect pending USB bytes incl. DEBUG lines
    usb.resync()
    backoff_seen = "backoff" in usb.raw
    print(f"   device DEBUG output during flood: {'BLE TX backoff reported' if backoff_seen else 'no backoff line captured'}")
    usb.objects.clear()

    # ---------- E: WiFi suspend/resume x5 via Disp MAC Addr ----------
    menus = None
    for attempt in range(2):
        usb.resync()
        usb.send("GET menus")
        menus = usb.wait_for("menus", 6)
        if menus: break
    mac_no = mac_exec = None
    if menus:
        for m in menus["menus"]:
            if "MAC" in m["content"]:
                mac_no, mac_exec = m["menu number"], m.get("executable")
    else:
        print("   E0 diag: no menus reply; usb.raw tail:", repr(usb.raw[-200:]))
    check("E0: found remotely executable 'Disp MAC Addr'", mac_no is not None and mac_exec,
          f"menu #{mac_no}")
    cycles_ok = 0
    notice_on_ble = 0
    notice_leaked_to_usb = 0
    for cyc in range(5):
        usb.objects.clear()
        usb.send(f"PUT menu/start now/{mac_no}")
        msg_b = await ble.wait_for("message", 5)        # "BLE serial suspended: wireless mode", BleOnly
        if msg_b and "suspended" in json.dumps(msg_b): notice_on_ble += 1
        try:
            await asyncio.wait_for(ble.disconnected.wait(), 8)
        except asyncio.TimeoutError:
            pass
        if not ble.disconnected.is_set():
            print(f"   cycle {cyc+1}: BLE did not drop"); break
        # USB gets the MAC message; the BLE suspend notice must NOT appear on USB
        mac_msg = usb.wait_for("message", 6)
        for o in usb.objects + ([mac_msg] if mac_msg else []):
            if o and "suspended" in json.dumps(o): notice_leaked_to_usb += 1
        await asyncio.sleep(2.0)                        # back at top menu; backstop restarts BLE
        try:
            await ble.connect(timeout=25)
            rr = await ble_hs(ble, f"Cycle {cyc+1} of 5")
            if rr is None or "device" not in rr: break
        except RuntimeError:
            print(f"   cycle {cyc+1}: no re-advertise"); break
        cycles_ok += 1
    check("E1: 5x WiFi suspend -> re-advertise -> reconnect cycles", cycles_ok == 5, f"{cycles_ok}/5 cycles")
    check("E2: suspend notice arrived on BLE each cycle", notice_on_ble == 5, f"{notice_on_ble}/5")
    check("E3: suspend notice never leaked to USB (fix 5)", notice_leaked_to_usb == 0, f"leaks={notice_leaked_to_usb}")

    # heap deltas from the TEMP HEAPMARK instrumentation (DEBUG lines on raw USB)
    import re
    usb.pump(1.0)
    inits = [int(x) for x in re.findall(r"HEAPMARK init done: (\d+)", usb.raw)]
    stops = [int(x) for x in re.findall(r"HEAPMARK stop done: (\d+)", usb.raw)]
    print(f"   HEAPMARK free heap at init-done per cycle: {inits}")
    print(f"   HEAPMARK free heap at stop-done per cycle: {stops}")
    if len(inits) >= 2:
        deltas = [a - b for a, b in zip(inits, inits[1:])]
        print(f"   per-suspend/resume-cycle heap loss (init-to-init): {deltas} bytes")

    # ---------- teardown ----------
    await ble.send("put device/protocol/off"); await ble.wait_for("end m32protocol", 3)
    await ble.disconnect()
    usb.send("GET configs")
    cfgs = usb.wait_for("configs", 5)
    val = next((c["value"] for c in cfgs["configs"] if c["name"] == "Bluetooth Use"), None) if cfgs else None
    check("Z1: final state — selector restored to 5 (BLE Serial)", val == 5, f"value={val}")
    usb.send("PUT device/protocol/off"); usb.wait_for("end m32protocol", 3)
    usb.close()

    print()
    failed = [r for r in RESULTS if not r[1]]
    print(f"===== {len(RESULTS)-len(failed)}/{len(RESULTS)} checks passed =====")
    if failed:
        for n, _, d in failed: print("FAILED:", n, d)
        sys.exit(1)

asyncio.run(main())

asyncio.run(main())

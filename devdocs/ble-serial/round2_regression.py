#!/usr/bin/env python3
"""Hardware regression run for PR #194 review round 2 (commit 6c84e76).

Exercises, with USB and BLE clients attached simultaneously:
  A. fix 5  — session-control isolation across transports (both directions,
              handshake replies, goodbyes, re-ack, bogus value)
  B. fix d  — USB protocol value case-insensitivity (run separately, earlier)
  C. fix 2  — PUT snapshot/recall runs the Bluetooth-Use change-switch
  D. fix 3/4— congestion: no loop stall while the BLE TX ring overflows;
              backoff clears after drain
  E. WiFi suspend/resume x5 via remotely executed 'Disp MAC Addr'
              (suspend notice must be BLE-only; BLE must re-advertise and
              reconnect each cycle)
"""
import asyncio, json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from usb_m32 import M32Usb
from bleak import BleakClient, BleakScanner

NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

RESULTS = []
def check(name, ok, detail=""):
    RESULTS.append((name, ok, detail))
    print(("PASS  " if ok else "FAIL  ") + name + ("  -- " + detail if detail else ""))

class BleStream:
    def __init__(self):
        self.buf = ""; self.depth = 0; self.in_string = False; self.escape = False
        self.objects = []; self.echo = ""
    def feed(self, data: bytes):
        for ch in data.decode("utf-8", errors="replace"):
            if self.depth == 0:
                if ch == "{": self.depth = 1; self.buf = ch
                else: self.echo += ch
                continue
            self.buf += ch
            if self.in_string:
                if self.escape: self.escape = False
                elif ch == "\\": self.escape = True
                elif ch == '"': self.in_string = False
            elif ch == '"': self.in_string = True
            elif ch == "{": self.depth += 1
            elif ch == "}":
                self.depth -= 1
                if self.depth == 0:
                    try: self.objects.append(json.loads(self.buf))
                    except json.JSONDecodeError: self.objects.append({"_torn": self.buf[:60]})
                    self.buf = ""
    def resync(self):
        torn = None
        if self.depth > 0:
            torn = self.buf[:80]
            self.depth = 0; self.in_string = False; self.escape = False; self.buf = ""
        return torn

class Ble:
    def __init__(self):
        self.client = None
        self.stream = BleStream()
        self.disconnected = asyncio.Event()
    async def connect(self, timeout=20):
        dev = None
        end = time.time() + timeout
        while dev is None and time.time() < end:
            dev = await BleakScanner.find_device_by_filter(
                lambda d, adv: NUS_SERVICE.lower() in [u.lower() for u in adv.service_uuids],
                timeout=5.0)
        if dev is None:
            raise RuntimeError("device not advertising")
        self.disconnected.clear()
        self.stream = BleStream()
        self.client = BleakClient(dev, disconnected_callback=lambda c: self.disconnected.set())
        await self.client.connect()
        await self.client.start_notify(NUS_TX, lambda h, d: self.stream.feed(bytes(d)))
    async def send(self, line):
        await self.client.write_gatt_char(NUS_RX, (line + "\n").encode(), response=False)
    async def wait_for(self, key, timeout=4.0):
        end = time.time() + timeout
        while time.time() < end:
            for i, o in enumerate(self.stream.objects):
                if key in o: return self.stream.objects.pop(i)
            await asyncio.sleep(0.05)
        return None
    async def silent_for(self, key, seconds=2.0):
        """True if no object with `key` arrives within `seconds`."""
        return (await self.wait_for(key, seconds)) is None
    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()

def usb_silent_for(usb, key, seconds=2.0):
    return usb.wait_for(key, seconds) is None

async def main():
    usb = M32Usb(port=os.environ.get("M32_PORT", "/dev/cu.usbmodem101"))
    time.sleep(0.3); usb.pump(0.3); usb.objects.clear(); usb.raw = ""
    ble = Ble()

    # ---------- setup: BLE session first, then USB handshake under BLE watch —
    await ble.connect()
    await ble.send("PUT device/protocol/on")
    dev = await ble.wait_for("device")
    check("setup: BLE handshake", dev is not None, json.dumps(dev)[:80] if dev else "no device object")

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
    await ble.send("PUT Device/Protocol/ON")
    check("A6: mixed-case BLE re-handshake", (await ble.wait_for("device")) is not None)
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
        await ble.send("PUT device/protocol/on")
        re_ok = (await ble.wait_for("device")) is not None
    except RuntimeError:
        re_ok = False
    check("C3: selector->5 restarts BLE (re-advertise + handshake)", re_ok)

    usb.objects.clear()
    usb.send(f"PUT snapshot/recall/{slot}")
    ok_r = usb.wait_for("ok", 4)
    gotmsg2 = await ble.wait_for("message", 4)          # fix 2: "BLE serial off" must arrive on BLE
    try:
        await asyncio.wait_for(ble.disconnected.wait(), 6)
        dropped2 = True
    except asyncio.TimeoutError:
        dropped2 = ble.disconnected.is_set()
    check("C4: PUT snapshot/recall (selector 0) stops BLE (fix 2)",
          ok_r is not None and dropped2, f"notice={json.dumps(gotmsg2) if gotmsg2 else 'MISSING'}")
    usb.send("GET configs")
    cfgs = usb.wait_for("configs", 5)
    val = next((c["value"] for c in cfgs["configs"] if c["name"] == "Bluetooth Use"), None) if cfgs else None
    check("C5: selector reads 0 after recall", val == 0, f"value={val}")

    # cleanup: clear slot, restore selector 5, BLE back up
    usb.send(f"PUT snapshot/clear/{slot}"); usb.wait_for("ok", 4)
    usb.send("PUT config/Bluetooth Use/5"); usb.wait_for("ok", 4)
    await asyncio.sleep(1.5)
    await ble.connect(timeout=20)
    await ble.send("PUT device/protocol/on")
    check("C6: cleanup — slot cleared, selector=5, BLE back", (await ble.wait_for("device")) is not None)

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
        await ble.send("PUT device/protocol/on")
        await ble.wait_for("device", 4)
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
            await ble.send("PUT device/protocol/on")
            if (await ble.wait_for("device")) is None: break
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

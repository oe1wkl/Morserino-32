#!/usr/bin/env python3
"""End-to-end test for the Morserino-32 "BLE Serial" feature (M32 protocol over BLE).

Runs from any Mac/Linux/Windows machine with Bluetooth:
    pip install bleak
    python3 ble_m32_test.py            # full scripted sequence
    python3 ble_m32_test.py --repl     # interactive: type protocol lines yourself

The device must run firmware with CONFIG_BLE_SERIAL and have the
"BLE Serial" preference set to On (double-click the black knob ->
turn to "BLE Serial" -> set On -> leave preferences; it starts at the
top menu from then on).

What the scripted sequence exercises (PLAN section 8, steps 1-8):
  1. scan by NUS service UUID, device name in scan response
  2. handshake PUT device/protocol/on -> device object (also: immediately
     after connect - early-write preservation, D18)
  3. GET menus -> multi-chunk JSON reassembly
  4. PUT menu/start now/<CW Keyer> (menu number discovered dynamically)
  5. PUT cw/play/... -> ok + (with Serial Output=All) raw echo over BLE
  6. batched write: two commands in one GATT write -> both execute
  7. mixed-case Put device/protocol/off -> ends ONLY the BLE session
     (rev-4 regression case), then re-handshake works
  8. GET snapshots (multi-KB) -> flow-control/chunking soak
"""

import argparse
import asyncio
import json
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    sys.exit("pip install bleak")

NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   # we write commands here
NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   # device notifies output here


class M32Stream:
    """Reassembles the notification byte stream into JSON objects + raw echo,
    exactly the way a USB client frames the M32 protocol (balanced braces,
    string/escape aware; bytes outside objects are the raw CW echo)."""

    def __init__(self):
        self.buf = ""
        self.depth = 0
        self.in_string = False
        self.escape = False
        self.objects = []
        self.echo = ""

    def feed(self, data: bytes):
        for ch in data.decode("utf-8", errors="replace"):
            if self.depth == 0:
                if ch == "{":
                    self.depth = 1
                    self.buf = ch
                else:
                    self.echo += ch
                continue
            self.buf += ch
            if self.in_string:
                if self.escape:
                    self.escape = False
                elif ch == "\\":
                    self.escape = True
                elif ch == '"':
                    self.in_string = False
            elif ch == '"':
                self.in_string = True
            elif ch == "{":
                self.depth += 1
            elif ch == "}":
                self.depth -= 1
                if self.depth == 0:
                    try:
                        self.objects.append(json.loads(self.buf))
                    except json.JSONDecodeError:
                        print(f"!! torn/unparseable JSON ({len(self.buf)} chars)")
                    self.buf = ""


async def wait_for(stream, pred, timeout=6.0, what="response"):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for obj in stream.objects:
            if pred(obj):
                stream.objects.remove(obj)
                return obj
        await asyncio.sleep(0.05)
    raise TimeoutError(f"no {what} within {timeout}s")


async def send_line(client, line: str):
    print(f">> {line}")
    await client.write_gatt_char(NUS_RX, (line + "\n").encode(), response=True)


async def find_device(timeout=15.0):
    print(f"scanning for NUS service {NUS_SERVICE} ...")
    dev = await BleakScanner.find_device_by_filter(
        lambda d, ad: NUS_SERVICE.lower() in [u.lower() for u in (ad.service_uuids or [])],
        timeout=timeout,
    )
    if dev is None:
        sys.exit("no Morserino advertising BLE Serial found (pref on? at top menu? not in a WiFi mode?)")
    print(f"found: name={dev.name!r} addr={dev.address}  (expect name 'Morserino-32' via scan response)")
    return dev


async def scripted(client, stream):
    ok = lambda o: "ok" in o
    passed = []

    # 2 - handshake immediately after connect (D18 early-write preservation)
    await send_line(client, "PUT device/protocol/on")
    dev = await wait_for(stream, lambda o: "device" in o, what="device object")
    print(f"<< device: {dev['device']}")
    passed.append("handshake+early-write")

    # 3 - menu list, multi-chunk reassembly; find the CW Keyer number dynamically
    await send_line(client, "GET menus")
    menus = await wait_for(stream, lambda o: "menus" in o, what="menus")
    keyer = next(m["menu number"] for m in menus["menus"] if m["content"].strip().lower().endswith("cw keyer"))
    print(f"<< {len(menus['menus'])} menus, CW Keyer = #{keyer}")
    passed.append("menus-reassembly")

    # 4 - start the keyer remotely
    await send_line(client, f"PUT menu/start now/{keyer}")
    await wait_for(stream, lambda o: "activate" in o or "menu" in o, what="activate")
    passed.append("start-keyer")

    # 5 - key text (listen to the sidetone!)
    await send_line(client, "PUT cw/play/CQ CQ CQ DE W2ASM")
    await asyncio.sleep(1.0)
    print(f"   echo so far: {stream.echo!r}  (needs pref 'Serial Output' = All/Generated)")
    passed.append("cw-play")
    await asyncio.sleep(6.0)
    await send_line(client, "PUT cw/stop")

    # 6 - batched write: both lines must execute (one per poll)
    stream.echo = ""
    batch = f"PUT cw/play/HI\nGET control\n".encode()
    print(f">> [single GATT write] {batch!r}")
    await client.write_gatt_char(NUS_RX, batch, response=True)
    await wait_for(stream, lambda o: "control" in o or "controls" in o, what="control after batched cw/play")
    passed.append("batched-write")
    await asyncio.sleep(2.0)

    # 7 - mixed-case protocol/off (rev-4 regression case): must end ONLY BLE
    await send_line(client, "Put device/protocol/off")
    await wait_for(stream, lambda o: "end m32protocol" in o, what="goodbye")
    print("<< goodbye received (BLE session ended; check any USB session is UNAFFECTED)")
    # device must now be silent to us until we re-handshake
    await send_line(client, "GET control")
    try:
        await wait_for(stream, lambda o: "control" in o, timeout=2.0)
        print("!! FAIL: got a response while un-handshaken")
    except TimeoutError:
        passed.append("mixed-case-off+silence")
    await send_line(client, "PUT device/protocol/on")
    await wait_for(stream, lambda o: "device" in o, what="re-handshake")
    passed.append("re-handshake")

    # 8 - multi-KB response: flow control / chunking soak
    await send_line(client, "GET snapshots")
    snaps = await wait_for(stream, lambda o: "snapshots" in o, timeout=15.0, what="snapshots")
    print(f"<< snapshots object OK ({len(json.dumps(snaps))} chars, reassembled untorn)")
    passed.append("flow-control-soak")

    await send_line(client, "PUT device/protocol/off")
    print("\nPASSED:", ", ".join(passed))


async def repl(client, stream):
    print("type M32 protocol lines (e.g. 'PUT device/protocol/on'); Ctrl-D to quit")
    loop = asyncio.get_event_loop()
    while True:
        try:
            line = await loop.run_in_executor(None, input, "m32> ")
        except EOFError:
            break
        if line.strip():
            await send_line(client, line.strip())
        await asyncio.sleep(0.3)
        for obj in stream.objects:
            print(f"<< {json.dumps(obj)}")
        stream.objects.clear()
        if stream.echo:
            print(f"<~ echo: {stream.echo!r}")
            stream.echo = ""


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repl", action="store_true", help="interactive mode")
    args = ap.parse_args()

    dev = await find_device()
    stream = M32Stream()
    async with BleakClient(dev) as client:
        print(f"connected, MTU {client.mtu_size}")
        await client.start_notify(NUS_TX, lambda _c, d: stream.feed(d))
        if args.repl:
            await repl(client, stream)
        else:
            await scripted(client, stream)


if __name__ == "__main__":
    asyncio.run(main())

"""Shared helpers for the BLE Serial bench scripts (round 3, 2026-09-30).

BLE Serial requires on-device consent since ACCESS_CONTROL.md was implemented:
every handshake from a new client raises "Allow connection?" on the Morserino
(FN = allow, encoder click = deny, 20 s timeout = deny) and is refused with
DEVICE BUSY while a mode runs. A client that returns within 60 s of a grant is
admitted without asking (decision D3). ble_hs() below handles the two-stage
reply and speaks a cue through the Mac's speaker when a press is needed.

Run the scripts from macOS Terminal, not from a sandboxed tool shell: a process
whose host app lacks Bluetooth permission is killed by macOS on first
CoreBluetooth use (exit code 134, no output).
"""
import asyncio, json, os, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from usb_m32 import M32Usb
from bleak import BleakClient, BleakScanner

NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

RESULTS = []
def cue(msg):
    print("   >>> " + msg, flush=True)
    try: subprocess.Popen(["say", msg])
    except Exception: pass

async def ble_hs(ble, label, line="PUT device/protocol/on", press=True, wait=40):
    """Handshake under ACCESS_CONTROL: device answers CONFIRM ON DEVICE, then device/error after FN."""
    ble.stream.objects.clear()
    await ble.send(line)
    if press: cue(f"{label}. Press F N on the classic now")
    t0 = time.time()
    while time.time() - t0 < wait:
        for i, o in enumerate(ble.stream.objects):
            if "message" in o and "CONFIRM" in json.dumps(o):
                ble.stream.objects.pop(i); break
            if "device" in o or "error" in o:
                return ble.stream.objects.pop(i)
        await asyncio.sleep(0.05)
    return None
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


ESPTOOL = os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")
def hard_reset(port, chip="esp32"):
    """A real reset. Opening the classic's CP2102 port does NOT reliably reset it."""
    subprocess.run([sys.executable, ESPTOOL, "--chip", chip, "--port", port,
                    "--before", "default_reset", "--after", "hard_reset", "chip_id"],
                   capture_output=True, timeout=60)

def summary(label="checks passed"):
    print()
    failed = [r for r in RESULTS if not r[1]]
    print(f"===== {len(RESULTS)-len(failed)}/{len(RESULTS)} {label} =====")
    for n, _, d in failed: print("FAILED:", n, d)
    return 1 if failed else 0

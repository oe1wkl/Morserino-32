"""Minimal USB-side M32 protocol client (mirror of the BLE test's M32Stream)."""
import json, time
import serial

class M32Usb:
    def __init__(self, port="/dev/cu.usbmodem101", baud=115200):
        self.ser = serial.Serial(port, baud, timeout=0.05)
        self.buf = ""
        self.depth = 0
        self.in_string = False
        self.escape = False
        self.objects = []
        self.raw = ""          # everything outside JSON objects (echo + DEBUG lines)

    def close(self):
        self.ser.close()

    def send(self, line):
        self.ser.write((line + "\n").encode())

    def _feed(self, data: bytes):
        for ch in data.decode("utf-8", errors="replace"):
            if self.depth == 0:
                if ch == "{":
                    self.depth = 1
                    self.buf = ch
                else:
                    self.raw += ch
                continue
            self.buf += ch
            if self.in_string:
                if self.escape: self.escape = False
                elif ch == "\\": self.escape = True
                elif ch == '"': self.in_string = False
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
                        self.objects.append({"_unparseable": self.buf})
                    self.buf = ""

    def pump(self, duration=0.5):
        end = time.time() + duration
        while time.time() < end:
            data = self.ser.read(4096)
            if data:
                self._feed(data)

    def wait_for(self, key, timeout=3.0):
        """Pump until an object with top-level `key` arrives; return it or None."""
        end = time.time() + timeout
        while time.time() < end:
            for i, o in enumerate(self.objects):
                if key in o:
                    return self.objects.pop(i)
            data = self.ser.read(4096)
            if data:
                self._feed(data)
        return None

    def drain(self):
        self.pump(0.3)
        objs, self.objects = self.objects, []
        return objs

    def resync(self):
        """Recover from a torn object (client-side timeout resync); returns the torn prefix or None."""
        torn = None
        if self.depth > 0:
            torn = self.buf[:80]
            self.depth = 0; self.in_string = False; self.escape = False; self.buf = ""
        return torn

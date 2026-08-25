#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
m32_link.py -- talk the M32 serial protocol to a connected Morserino.

The transport half of the protocol tests: port discovery, the
`PUT device/protocol/on` handshake, brace-framed reply reading, and the
close/reopen cycle that reboots an ESP32-S3 so persistence can be checked.

Kept separate from the checks themselves so other bench scripts can import it:

    from m32_link import M32Link, find_port
    with M32Link(find_port()) as m32:
        print(m32.command("GET kochlesson"))

Requires pyserial (`pip install pyserial`).
"""

import glob
import json
import time

try:
    import serial
except ImportError:                                     # pragma: no cover
    raise SystemExit("pyserial is missing -- install it with:  pip install pyserial")


class ProtocolError(RuntimeError):
    """The device answered, but with {"error": ...}."""

    def __init__(self, message, command=None):
        super().__init__(message)
        self.command = command


def find_port():
    """The Morserino's serial port, or None.

    An M32 Pocket is an ESP32-S3 on native USB and shows up as
    /dev/cu.usbmodem*; a classic M32 goes through a CP210x and shows up as
    /dev/cu.SLAB_USBtoUART or /dev/cu.usbserial-*.
    """
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.SLAB_USBtoUART*",
                    "/dev/cu.usbserial-*", "/dev/ttyUSB*", "/dev/ttyACM*"):
        found = sorted(glob.glob(pattern))
        if found:
            return found[0]
    return None


def extract_json_object(buffer):
    """Pull the first complete JSON object out of `buffer`.

    Returns (object_text, rest_of_buffer), or (None, buffer) if no object has
    arrived in full yet.

    Replies are NOT newline-delimited and a single read can straddle two of
    them, so framing is by brace counting -- string- and escape-aware, because
    a CW memory or an operator name may legitimately contain a brace. This is
    the same framing the Configuration Tool settled on after curly braces in
    user text were found to truncate one reply and garble the next.
    """
    depth = 0
    start = -1
    in_string = False
    escaped = False

    for i, c in enumerate(buffer):
        if start < 0:
            if c == "{":
                start = i
                depth = 1
            continue
        if escaped:
            escaped = False
            continue
        if in_string:
            if c == "\\":
                escaped = True
            elif c == '"':
                in_string = False
            continue
        if c == '"':
            in_string = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return buffer[start:i + 1], buffer[i + 1:]
    return None, buffer


class M32Link:
    """One protocol session over one serial port."""

    def __init__(self, port, baud=115200, verbose=False):
        self.port = port
        self.baud = baud
        self.verbose = verbose
        self._serial = None
        self._buffer = ""
        self.device = {}
        # Commands whose reply had to be shaken loose by a later one; a
        # protocol fault worth reporting, not a slow device. See command().
        self.delayed_replies = []

    # -- connection ---------------------------------------------------------

    def open(self, handshake=True, boot_wait=1.5):
        """Open the port and (by default) handshake the protocol on.

        Opening resets an ESP32-S3 board -- the first thing on the wire is the
        ROM banner, not an answer -- so `boot_wait` lets most of the boot pass
        before the handshake is attempted, and handshake() retries besides.

        pyserial rejects dtr=/rts= as constructor keywords, so the object is
        built unopened, the lines are set, and only then is it opened -- with
        both lines de-asserted, which is what this board answers to.
        """
        s = serial.Serial()
        s.port = self.port
        s.baudrate = self.baud
        s.timeout = 0.1
        s.dtr = False
        s.rts = False
        s.open()
        self._serial = s
        self._buffer = ""

        if boot_wait:
            time.sleep(boot_wait)
        s.reset_input_buffer()

        if handshake:
            self.handshake()
        return self

    def close(self):
        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None

    def reboot(self, boot_wait=3.0, attempts=4):
        """Power-cycle by proxy: close the port, reopen it, handshake again.

        Opening the port asserts a chip reset on the ESP32-S3, so this is a
        real reboot -- which is exactly what a persistence check needs, since
        anything held only in RAM does not survive it.
        """
        self.close()
        time.sleep(0.5)
        last = None
        for attempt in range(attempts):
            try:
                return self.open(handshake=True, boot_wait=boot_wait)
            except Exception as exc:                     # port not back yet
                last = exc
                self.close()
                time.sleep(1.0)
        raise RuntimeError("device did not come back after reboot: %s" % last)

    def __enter__(self):
        return self.open()

    def __exit__(self, *exc_info):
        self.close()
        return False

    # -- raw I/O ------------------------------------------------------------

    def _write_line(self, text):
        if self.verbose:
            print("    >> %s" % text)
        self._serial.write((text + "\n").encode("utf-8", "replace"))
        self._serial.flush()

    def _read_object(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            obj, self._buffer = extract_json_object(self._buffer)
            if obj is not None:
                if self.verbose:
                    print("    << %s" % obj)
                return obj
            chunk = self._serial.read(4096)
            if chunk:
                self._buffer += chunk.decode("utf-8", "replace")
            else:
                time.sleep(0.01)
        partial = self._buffer[:120]
        self._buffer = ""
        raise TimeoutError("no reply within %.1fs (partial: %r)" % (timeout, partial))

    # -- protocol -----------------------------------------------------------

    def handshake(self, timeout=2.0, attempts=8):
        """Open a protocol session. Nothing else is recognised before this.

        Retried, because opening the port reset the board: the first attempts
        land while the ESP32-S3 is still in ROM boot and go unanswered, and how
        long that takes varies (a cold battery boot is slower than a warm one).
        The device answers as soon as it is up, so this usually costs nothing.
        """
        last = None
        for _ in range(attempts):
            self._buffer = ""
            try:
                self._serial.reset_input_buffer()
            except Exception:                            # port still settling
                pass
            self._write_line("PUT device/protocol/on")
            try:
                reply = json.loads(self._read_object(timeout))
            except (OSError, ValueError) as exc:          # no reply, or a torn one
                last = exc
                time.sleep(0.7)
                continue
            if isinstance(reply, dict) and "device" in reply:
                self.device = reply["device"]
                return self.device
            last = "unexpected reply %s" % reply
            time.sleep(0.7)
        raise ProtocolError("no protocol session after %d attempts (last: %s)"
                            % (attempts, last))

    # Most commands answer in about 100 ms. The exceptions are the ones that
    # persist something: writePreferences() can land on an NVS page compaction
    # and block for seconds, once in a while and unpredictably. A short timeout
    # then loses the reply, and -- worse -- the straggler arrives while the NEXT
    # command is waiting and is read as ITS answer, so every later reply is off
    # by one. Hence a generous default here, a longer one still on the writing
    # helpers below, and a flush before every send so a straggler can never be
    # mistaken for the next reply.
    DEFAULT_TIMEOUT = 5.0
    WRITE_TIMEOUT = 2.5

    # A harmless read-only command, used to poke a withheld reply loose.
    NUDGE = "GET capabilities"

    def command(self, text, timeout=None, allow_error=False, nudge=True):
        """Send one command and return the parsed reply.

        An {"error": ...} reply raises ProtocolError rather than being handed
        back looking like data -- unless `allow_error`, for the cases where an
        error IS the expected answer (rejecting an out-of-range lesson).

        If nothing comes back in time, the reply may not be lost but merely
        withheld: some commands compose their answer and then do not put it on
        the wire until more input arrives (`PUT config/Koch Sequence/3` does
        this reliably). Rather than fail, send a harmless read, collect the
        late answer, and record the command in `delayed_replies` so the caller
        can report it -- a reply that needs a later command to shake it loose
        is a protocol fault, not a slow device.
        """
        timeout = self.DEFAULT_TIMEOUT if timeout is None else timeout
        self._buffer = ""
        try:
            self._serial.reset_input_buffer()            # drop any straggler
        except Exception:
            pass
        self._write_line(text)
        try:
            raw = self._read_object(timeout)
        except OSError:
            if not nudge:
                raise
            # Collect everything the nudge shakes loose. TWO objects means the
            # original answer was sitting there unsent and the nudge released
            # it (withheld). ONE means only the nudge itself was answered and
            # the original reply never existed (lost). Timing cannot tell these
            # apart -- both arrive at the normal ~100 ms round trip -- so count
            # objects instead.
            self._write_line(self.NUDGE)
            objects = []
            while len(objects) < 2:
                try:
                    objects.append(self._read_object(timeout if not objects else 1.5))
                except OSError:
                    break
            if not objects:
                raise
            self.delayed_replies.append((text, "withheld" if len(objects) > 1
                                               else "lost"))
            # With two, the first is the original answer; with one, all we have
            # is the nudge's own reply, which is not an answer to `text` at all.
            raw = objects[0]
        reply = json.loads(raw)
        if isinstance(reply, dict) and "error" in reply:
            if allow_error:
                return reply
            err = reply["error"]
            if isinstance(err, dict):
                # the firmware puts the text in "content"; read "name" too, for tolerance
                message = err.get("content") or err.get("name") or "unknown error"
            else:
                message = str(err)
            raise ProtocolError("%s   (command: %s)" % (message, text), command=text)
        return reply

    # -- convenience --------------------------------------------------------

    def get_config(self, name):
        """Current value of one preference, by its on-screen name."""
        reply = self.command("GET config/%s" % name)
        return reply["config"]["value"]

    def set_config(self, name, value):
        # writes NVS -- see WRITE_TIMEOUT
        self.command("PUT config/%s/%s" % (name, value), timeout=self.WRITE_TIMEOUT)

    def get_kochlesson(self):
        return self.command("GET kochlesson")["kochlesson"]

    def set_kochlesson(self, value, allow_error=False):
        # writes NVS -- see WRITE_TIMEOUT
        return self.command("PUT kochlesson/%s" % value,
                            timeout=self.WRITE_TIMEOUT, allow_error=allow_error)

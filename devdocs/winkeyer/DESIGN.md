# WinKeyer protocol emulation — feasibility and effort estimate

Status: **feasibility study, 2026-10-15. No code written.** Nothing here is decided;
the open decisions are listed at the end and need Willi's sign-off before work starts.

The question: can the M32 behave like a K1EL WinKeyer (WK2/WK3) so that logging and
contest programs (N1MM+, Logger32, Win-Test, DXLog, RUMlog, fldigi, …) can send CW
through it? How would that interfere with the existing USB use, and could it also run
over BLE Serial?

**Short answer:** feasible. A useful WinKeyer *mode* that covers the WK2 command subset
loggers actually use is about **1,000–1,500 lines** in a new `MorseWinKeyer.cpp/.h`,
roughly **4–6 working sessions** plus bench time with 2–3 loggers on both variants.
Full WK3 compatibility is not worth it: the hardware has no PTT output and only one
key line. The real risks are **sharing the single USB port**, the **1200-baud switch on
the classic**, and **DTR/RTS resets**. BLE works as a byte transport, but almost no host
software can reach a WinKeyer over BLE.

---

## 1. What WinKeyer needs, and what the firmware already has

WinKeyer protocol in brief (from the K1EL WK2/WK3 datasheets — **verify every command
code against the datasheet before implementing**):

- Serial **1200 baud, 8N2** (WK3 can be switched to 9600 by an admin command).
- Session opens with admin command **Host Open** (`0x00 0x02`), answered with the
  firmware revision byte; closes with `0x00 0x03`.
- Bytes `0x00`–`0x1F` are commands, `0x20`–`0x7F` are text to send.
  - **Immediate** commands: sidetone, speed, weighting, PTT lead/tail, speed-pot setup,
    pause, backspace, pin config, clear buffer, key immediate, HSCW, Farnsworth,
    WinKeyer mode register (paddle mode, echo flags, autospace, contest spacing, swap),
    keying compensation, paddle switchpoint, software paddle, request status, …
  - **Buffered** commands (sit in the text buffer, take effect in sequence): PTT on/off,
    key down, wait, merge letters (prosigns), speed change, cancel speed change, NOP.
- The device → host stream mixes three kinds of byte:
  - **status** (`0xC0 | flags`: XOFF, break-in, busy, wait, …) — sent unsolicited on change;
  - **speed pot** (`0x80 | value`) — sent when the pot moves;
  - **echo characters** — paddle-sent characters decoded and echoed (if enabled), and
    serial echo of buffered text as it is sent.
- Flow control is in-band: the host stops sending when XOFF is set (buffer ≳ 2/3 full).
- Paddle activity during buffered sending is **break-in**: the buffer is cleared, the
  break-in status bit is set, the paddle wins.

Mapping onto existing firmware (`Software/src/Version 6 and newer/`):

| WinKeyer function | M32 today | Notes |
|---|---|---|
| Key the transmitter | `keyOut()` (`m32_v6.ino:4219`), `keyerPin` | one key line only |
| Sidetone | `MorseOutput::pwmTone` via `keyOut()` | hard rule 1 |
| Text → CW | `encodeProSigns()` → `generateCWword()` | WinKeyer's merge-letters and its prosign characters must map onto M32 prosign codes (hard rule 4) |
| Paddle keyer, break-in | `doPaddleIambic()` | modes `IAMBICA/IAMBICB/ULTIMATIC/NONSQUEEZE` (`morsedefs.h:293`); WinKeyer "bug" mode has no equivalent |
| Paddle echo to host | `MorseDecoder` | decoded chars → WinKeyer echo bytes |
| Speed pot | encoder in speed mode | report as `0x80 | (wpm − min)` |
| Standalone messages | `memoryKeyer()` (`m32_v6.ino:4328`), 8 memories | later, optional |
| **PTT output** | **none** | accept and ignore PTT/lead/tail; fine for QSK/VOX rigs or loggers switching PTT over CAT |
| **Second key output, pin config** | **none** | accept and ignore |

Engine: a non-blocking buffered sender, driven from `loop()` like the other modes,
with the WinKeyer input buffer, XOFF accounting and buffered-command handling. It
must not block (see the "blocking loops" class of bugs), so that serial input, paddle
break-in and `resetTOT()` (hard rule 2) all keep running while it sends.

The mode keys a real transmitter, so it runs under its own TX-capable `morseState`
(not `morseGame`/`morseQsoBot`, which are deliberately excluded from every TX gate —
hard rule 8). A new `morseWinKeyer` enum value (`morsedefs.h:330`) keeps that explicit.

## 2. Interference with existing USB use

1. **One port, two protocols.** The Pocket's USB-Serial-JTAG (HWCDC) exposes a single
   CDC port; the classic has one CP2102 UART bridge. The M32 protocol is line-based text
   (`serialEvent()`, `m32_v6.ino:4681`, handshake `PUT device/protocol/on`); WinKeyer is
   binary and starts with `0x00 0x02`. The two can't be confused — `0x00` never occurs in
   M32 protocol text — but they **cannot run at the same time** on one port.
   - Recommended: a **dedicated WinKeyer menu mode** that owns the port while active and
     hands it back on exit, like the transceiver modes own their radio.
   - Rejected: a USB composite device with two CDC ports. On the S3 that means switching
     from HWCDC (`ARDUINO_USB_MODE=1`) to TinyUSB, throwing away the HWCDC TX/RX buffer
     fixes (`m32_v6.ino:573`) and the bootloader-reset path. Too much risk for this feature.
   - Auto-detecting Host Open in the top menu would be possible on the Pocket, but not on
     the classic (baud mismatch, below), so a mode is the one design that works on both.
2. **Baud rate on the classic.** On the Pocket the baud rate of a virtual CDC port is
   irrelevant. On the classic the CP2102 turns the host's 1200-baud setting into the real
   UART speed, so the mode must `Serial.begin(1200, SERIAL_8N2)` on entry and restore
   115200 on exit.
3. **Stray output corrupts the binary stream.** Must be gated while the mode is active:
   - `DEBUG()` prints to raw `Serial` whenever **Serial Output is off**
     (`m32_v6.ino:542`) — i.e. in normal use, not only in debug builds;
   - about 15 raw `Serial.println` sites (NVS-full warnings in the games and prefs,
     voice-store messages, codec error, …);
   - the `m32out.echo()` character echo (`M32ProtocolOut.cpp:70`), Serial Output pref.
   A host that hasn't sent Host Open yet ignores garbage, but once a session is open any
   stray byte is misread as status/echo/pot.
4. **DTR/RTS resets — the biggest bench risk.**
   - Classic: the CP2102 auto-reset circuit pulls EN when only one of DTR/RTS is
     asserted; both asserted or both released is safe.
   - Pocket: USB-Serial-JTAG reboots on certain DTR/RTS sequences — the same port-open
     reboots seen with pyserial's DTR-before-RTS order.
   - Loggers handle these lines differently (several let the user configure them; old
     serial WinKeyers drew power from them, so "both on" is common — the safe state).
     The manual will need a per-logger note on DTR/RTS settings.
   - `pocketwroom` sets `board_upload.use_1200bps_touch = true` (`platformio.ini:299`),
     and 1200 is WinKeyer's baud rate. That setting is upload-side and HWCDC firmware
     should ignore a 1200-baud line coding, but **verify on the bench** before anything else.
5. **Serial input drain at boot** (`m32_v6.ino:978`) is harmless; no change needed.

## 3. BLE Serial

- **Device side is cheap** (≈1–2 sessions): the BLE RX ring (`BleByteRing.h`) is
  byte-oriented, but `bleSerialEvent()` (`m32_v6.ino:4856`) assembles text lines with a
  400-byte cap and dispatches at `\n`. WinKeyer needs a raw byte branch there, and TX via
  `MorseBleSerial::txEnqueue`/`txEnqueueEcho`.
- **Consent:** BLE consent is asked only at the top menu
  (`devdocs/ble-serial/ACCESS_CONTROL.md`, decision D2), so the user connects before
  entering the WinKeyer mode. That fits; no change to the access model.
- **Latency is not a problem:** keying runs locally from the buffer; only status, echo and
  pot bytes reach the host a connection interval later.
- **Host side is the obstacle:** Windows and macOS loggers open a COM port. A Nordic-UART
  BLE service is not one, and neither OS provides a BLE-UART-to-COM bridge. Without a
  helper (a pty bridge on macOS/Linux, com0com plus a bridge on Windows) nothing would use
  it. The M32 iOS app (`devdocs/ios-app/DESIGN.md`) could, if it ever wanted a logger role.
- Classic Bluetooth SPP *would* appear as a real Windows COM port on the classic M32, but
  the Pocket's ESP32-S3 has no Classic Bluetooth, and Bluedroid Classic is heavy on the V2
  alongside WiFi/LoRa. Not recommended.

**Recommendation:** leave BLE out of the first version; add the raw byte path only when
there is a client that wants it.

## 4. Effort

| Piece | Estimate |
|---|---|
| Command parser: Host Open/Close, revision, speed, sidetone, weighting, mode register, clear buffer, buffered speed/wait/merge, status + XOFF and speed-pot reports | 2 sessions |
| Non-blocking buffered sender, paddle break-in (clears buffer, sets status bit), paddle echo | 1–2 sessions |
| Mode entry/exit, classic baud switch, output gating, `morseWinKeyer` state | 1 session |
| EN + DE manuals (incl. per-logger setup notes), voice clips for the new menu entry, `UX_CONVENTIONS.md` check | 0.5–1 session |
| BLE raw byte path (optional, see §3) | 1–2 sessions |
| Willi's bench: N1MM+, one Mac logger (RUMlog), fldigi — on classic and Pocket | 2–4 evenings |

A host-side test script in `Software/tests/` (pyserial at 1200 8N2, Host Open, send text,
check status/XOFF/echo bytes) would make the bench repeatable, in the style of the
protocol harness (`Software/tests/protocol/`). Count it inside the parser estimate.

## 5. Duties from CLAUDE.md that apply

- Build both variants (§1); `CONFIG_WINKEYER` flag in `platformio.ini` `build_flags` if
  it should be excludable from any env (e.g. the accessibility build, if flash is tight).
- UX: mode entry/exit, speed and volume must follow `devdocs/UX_CONVENTIONS.md` (§6).
  Open question: what the encoder does while a host is connected (speed pot vs. M32 speed).
- A11y (§8): the new menu entry is picked up by the extractor. **Open point:** on-screen
  messages are voiced via the M32 protocol text stream, which is *off* while WinKeyer owns
  the port — check how mode-internal messages ("Host connected", "Host closed") get voiced.
- Manuals EN + DE (§7).

## 6. Decisions for Willi

- **D1 — Mode or selector?** A dedicated WinKeyer menu mode (recommended: works on both
  variants, port ownership is obvious) vs. a "Serial Port Use" selector preference
  (M32 protocol / WinKeyer) that makes the port WinKeyer-only until changed.
- **D2 — Scope:** WK2 command subset as above (recommended) vs. WK3 extensions.
- **D3 — PTT:** accept and ignore (recommended; no PTT hardware) vs. repurpose something.
- **D4 — BLE:** not in the first version (recommended).
- **D5 — Branch:** own branch off `master` (CLAUDE.md branch rule); V9.x or V10 line.

# WinKeyer protocol emulation — feasibility and effort estimate

Status: **feasibility study, 2026-10-15. No code written.** Willi decided D1–D5 the
same day (§6). §7 walks through every WK2 and WK3 command against the K1EL datasheets;
it raised three more decisions (D6–D8), decided the same day after the logger research
in §8. **All decisions are taken; nothing blocks implementation.**

Sources: K1EL *WinKeyer2 IC Interface & Operation Manual* v23 (10/5/2010), *WinKeyer 3.1
IC Interface & Operation Manual* rev 1.3 (3/19/2019), *WK2 Application Interface Guide*
v1.1 — all from <https://k1elsystems.com/DocsC.html>.

The question: can the M32 behave like a K1EL WinKeyer (WK2/WK3) so that logging and
contest programs (N1MM+, Logger32, Win-Test, DXLog, RUMlog, fldigi, …) can send CW
through it? How would that interfere with the existing USB use, and could it also run
over BLE Serial?

**Short answer:** feasible. A useful WinKeyer *mode* that covers the WK2 command subset
loggers actually use is about **1,000–1,500 lines** in a new `MorseWinKeyer.cpp/.h`,
roughly **5–7 working sessions** (after the §7 walk-through) plus bench time with 2–3
loggers on both variants.
Full WK3 compatibility is not worth it: the hardware has no PTT output and only one
key line. The real risks are **sharing the single USB port**, the **1200-baud switch on
the classic**, and **DTR/RTS resets**. BLE works as a byte transport, but almost no host
software can reach a WinKeyer over BLE.

---

## 1. What WinKeyer needs, and what the firmware already has

WinKeyer protocol in brief (full command list in §7):

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
- Flow control is in-band: the host stops sending when XOFF is set (buffer more than 2/3
  full; the buffer holds 128 characters on WK2, 160 on WK3).
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
   - **Decided (D1):** a **dedicated WinKeyer menu mode** that owns the port while active
     and hands it back on exit, like the transceiver modes own their radio.
   - **Decided (D1a):** on the **Pocket only**, auto-detect is a second, always-on way in.
     At the top menu, `serialEvent()` handles WinKeyer bytes before it assembles lines:
     - K1EL's recommended host start-up is four `0x13` nulls, an Echo Test
       (`0x00 0x04 c` → `c`), then Host Open. So the top menu must **answer the Echo Test
       while closed** and must keep `0x13`/`0x00` bytes out of the M32 line buffer.
     - Host Open (`0x00 0x02`) enters the WinKeyer mode and replies with the revision
       byte; Host Close (`0x00 0x03`) in an auto-entered session returns to the menu.
     - **Reply first, repaint later:** CQRLOG waits only 50 ms for the revision byte
       (and 300 ms for the Echo Test reply), so the reply must not wait for the screen.
     - Only at the top menu, like BLE consent, so a logger can never take over a running
       mode. The M32 protocol never sends `0x00`, so it can't trigger by mistake.
     - Not possible on the classic: at 115200 baud, 1200-baud bytes arrive as framing
       garbage, and the ESP32 only measures baud inside `begin()`. The manual says so.
   - Rejected: a USB composite device with two CDC ports. On the S3 that means switching
     from HWCDC (`ARDUINO_USB_MODE=1`) to TinyUSB, throwing away the HWCDC TX/RX buffer
     fixes (`m32_v6.ino:573`) and the bootloader-reset path. Too much risk for this feature.
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
     K1EL's own interface guide tells host authors to wait 400 ms "after asserting the
     RTS and DTR lines". But both open-source hosts we read do something else:
     **CQRLOG and flwkey set DTR on, RTS off.** On the classic that does not reset (EN
     stays high) but holds GPIO0 low, so any later reset — brown-out, watchdog — would
     boot into the download mode. On the Pocket DTR-only should be harmless too. What the
     bench has to show is the *transition* while the port opens, logger by logger.
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

**Decided (D4):** BLE is not in the first version; add the raw byte path only when there
is a client that wants it.


## 4. Effort

| Piece | Estimate |
|---|---|
| Command parser for every WK2/WK3 command (§7 — even ignored ones must be parsed), status + XOFF, speed-pot and echo bytes | 2 sessions |
| Non-blocking buffered sender with the sending-side settings of §7 (weighting, ratio, compensation, 1st extension, Farnsworth, letterspace, contest spacing, merge, ½-dit pad), paddle break-in, paddle echo | 1.5–2 sessions |
| Mode entry/exit, classic baud switch (1200, and 9600 on request), output gating, `morseWinKeyer` state | 1 session |
| Pocket auto-detect at the top menu (D1a), incl. the closed-state Echo Test | 0.5 session |
| Optional extras if D8 says so (messages → keyer memories, pointer commands, software paddle) | 0.5–1 session |
| EN + DE manuals (incl. per-logger setup notes), voice clips for the new menu entry, `UX_CONVENTIONS.md` check | 0.5–1 session |
| Willi's bench: N1MM+, one Mac logger (RUMlog), fldigi — on classic and Pocket | 2–4 evenings |

Total: about **5–7 sessions** plus the bench. BLE (D4: not now) would add 1–2 sessions later.

A host-side test script in `Software/tests/` (pyserial at 1200 8N2, Host Open, send text,
check status/XOFF/echo bytes) would make the bench repeatable, in the style of the
protocol harness (`Software/tests/protocol/`). Count it inside the parser estimate.

## 5. Duties from CLAUDE.md that apply

- Build both variants (§1); `CONFIG_WINKEYER` flag in `platformio.ini` `build_flags` if
  it should be excludable from any env (e.g. the accessibility build, if flash is tight).
- UX: mode entry/exit, speed and volume must follow `devdocs/UX_CONVENTIONS.md` (§6).
  The encoder role is decided (E1 below); check it against the conventions when designing
  the screen.
- A11y (§8): the new menu entry is picked up by the extractor. **Open point:** on-screen
  messages are voiced via the M32 protocol text stream, which is *off* while WinKeyer owns
  the port — check how mode-internal messages ("Host connected", "Host closed") get voiced.
- Manuals EN + DE (§7).

## 6. Decisions

Decided by Willi on 2026-10-15:

- **D1 — Menu mode.** A dedicated WinKeyer menu mode owns the serial port while active.
- **D1a — Pocket auto-detect, always on.** Host Open at the top menu also enters the mode
  (Pocket only; details in §2.1). No preference for it.
- **D2 — A subset, chosen command by command.** Walk through all WK2 and WK3 commands for
  feasibility first — that is §7.
- **D3 — PTT: accept and ignore.** No PTT hardware; the manual says so.
- **D4 — USB only** in the first version.
- **D5 — Own branch `winkeyer` off `master`**, merged back when done.
- **E1 — Encoder = speed, reported as the speed pot.** As in CW Keyer the encoder sets the
  speed; it is reported to the host as speed-pot bytes. WinKeyer rules apply: a host speed
  command wins until the host sets speed 0 (pot mode). Click and long press stay as in
  `UX_CONVENTIONS.md`.

- **D8 — Optional extras: messages and Vcc.** Admin "Send Message 1–6" plays M32 keyer
  memories 1–6; Read Back Vcc reports the measured battery voltage. Pointer commands and
  the software paddle are **not** in the first version.

Decided after the logger research in §8:

- **D6 — WK2 now, WK3 tried on the bench.** Implement the WK3-mode handling (Set WK3 Mode,
  WK3 sidetone formula, X1/X2MODE layouts, the changed Load Defaults bytes 9 and 15), but
  reply **23** to Host Open from a single constant. On the bench, try **31** with N1MM and
  one more logger, then settle. **Settled 2026-10-17: 23.** With 31, RUMlog switched to WK3
  mode and worked; N1MM only asked two WK3 questions (FW minor revision, RTTY registers) and
  otherwise stayed with WK2 commands - no WK3 mode, no baud change. 31 gains nothing, 23 is
  the revision K3NG's emulator has served the major loggers with (BENCH.md §5).
- **D7 — The M32 owns paddle and pitch.** Keyer mode, paddle swap, ACS/autospace, latency
  and the sidetone pitch stay with the M32 preferences; the host's values for these are
  parsed and ignored. Sending-side settings come from the host (speed, weighting, ratio,
  compensation, 1st extension, Farnsworth, letterspace, contest spacing, echo flags,
  paddle-only sidetone, key-output enable). Nothing the host sets is written to NVS;
  everything reverts on Host Close or mode exit. Consequences: no bug mode is needed; the
  manual must say that a logger's paddle-reverse, keying-mode and sidetone-frequency
  settings have no effect on the M32 (set them on the M32 instead).

## 7. Command walk-through (WK2 v23 and WK3.1)

Verdicts: **✅** implement — maps onto existing code, or cheap in the new sender;
**🟡** partial or approximate; **⚪** accept and ignore; **❌** not supported (send a
neutral reply where the host waits for one).

**Parser rule:** every command — including ⚪/❌ ones — must consume exactly its parameter
bytes, or the byte stream desynchronises and text is read as commands. Load EEPROM
(256 data bytes) and Load Defaults (15) are the long ones.

### 7.1 Admin commands (`0x00` + sub-command)

Admin commands are accepted while the host interface is closed (only Host Close is meant
for the open state). **The sub-command byte is the list number in the manuals, in
decimal** — `0x0A` Set WK1 Mode … `0x0E` Send Message, `0x0F` Load X1MODE, `0x14` Set WK3
Mode, `0x19` Sidetone Volume. The manuals' own examples are inconsistent (Send Message is
printed as `<00><14>`), but both flwkey (W1HKJ) and the K3NG keyer define the constants
this way, so that is what hosts send. One exception is still open: the two baud commands
(`11`/`12`, below).

| Byte | Command | Params → reply | WK | Verdict | Notes |
|---|---|---|---|---|---|
| `00` | Calibrate | 1 (`FF`) | 1 | ⚪ | WK1 only, ignored by WK2/3 too |
| `01` | Reset | – | 2/3 | 🟡 | Reset the WinKeyer session state (as Host Close). **Never** reboot the ESP32 |
| `02` | Host Open | → revision | 2/3 | ✅ | Revision per D6. Starts the session; on the Pocket also the auto-detect trigger |
| `03` | Host Close | – | 2/3 | ✅ | Restore the M32 settings; restore 1200 baud on the classic; auto-entered session → back to menu |
| `04` | Echo Test | 1 → same byte | 2/3 | ✅ | Must work while closed — loggers probe with it before Host Open (also at the top menu, D1a) |
| `05` | Paddle A2D | → 0 | 1 | ❌ | Historical; WK3 also returns 0 |
| `06` | Speed A2D | → 0 | 1 | ❌ | Historical; returns 0 |
| `07` | Get Values | → 15 bytes (WK2) / 0 (WK3) | 2/3 | ✅ | WK2 returns all setup values in Load Defaults order (WK3: 0). Cheap — report the session's values |
| `08` | Reserved (K1EL debug) | – | – | ⚪ | |
| `09` | Get Cal (WK2) / Get FW Major Rev (WK3) | → byte | 2/3 | ✅ | WK2: historical, returns a fixed byte (K3NG sends `0x18`); WK3: same value as the Host Open reply |
| `0A` | Set WK1 Mode | – | 2/3 | ✅ | Status-byte format only (bit 3 = KEYDOWN) |
| `0B` | Set WK2 Mode | – | 2/3 | 🟡 | Status-byte format switches; we never send pushbutton status bytes — the M32's buttons belong to the M32 UX |
| `0C` | Dump EEPROM | → 256 bytes | 2/3 | 🟡 | Used by K1EL's WK3tools (flwkey 1.2.4 has no "read parameters" control and shows its own saved values - bench, 2026-10-16). Reply with an image whose first 16 bytes hold the M32's current settings (WK2 layout), the rest zero — see D7 in §8 |
| `0D` | Load EEPROM | 256 | 2/3 | ❌ | Consume and discard all 256 bytes |
| `0E` | Send Message 1–6 | 1 | 2/3 | ✅ | D8: → M32 keyer memories 1–6 (`memoryKeyer()`, 8 slots); an empty slot sends nothing |
| `0F` | Load X1MODE (WK2: letterspace) | 1 | 2/3 | ✅ | Letterspace +0…30 % in the sender. Paddle-status and standalone-cut bits ignored |
| `10` | Firmware Update | protected | 3 | ❌ | Never; ignore |
| `11` | Set Low Baud (1200) | – | 2/3 | ✅ | Classic: `Serial.begin(1200, SERIAL_8N2)`; Pocket: no-op. **Unclear:** K3NG treats `0x11` as *high* and `0x12` as *low* — the opposite of the WK3 manual. Check with a logger that uses 9600 (UcxLog has such an option) |
| `12` | Set High Baud (9600) | – | 2/3 | ✅ | Classic: switch UART to 9600 8N2 (reverts on Close); Pocket: no-op. See `11` |
| `13` | Set RTTY Mode | 2 | 3.1 | ❌ | FSK via the key line would be possible, but out of scope |
| `14` | Set WK3 Mode | – | 3 | 🟡 | Switches the sidetone formula, the X1MODE layout and the meaning of Load Defaults bytes 9 and 15 (§8). Only relevant if we claim WK3 (D6) |
| `15` | Read Back Vcc | → byte | 3 | ✅ | D8: battery voltage, `byte = 26214 / (V × 100)` |
| `16` | Load X2MODE | 1 | 3 | 🟡 | Paddle-only sidetone ✅, cut 0→T / 9→N ✅ (trivial in the sender), paddle mute ✅, SO2R ⚪, fast PB response ⚪, paddle status ⚪ |
| `17` | Get FW Minor Rev | → byte | 3 | ✅ | e.g. 0 |
| `18` | Get IC Type | → byte | 3 | ✅ | 0 (DIP) |
| `19` | Set Sidetone Volume | 1 | 3 | ⚪ | Volume is the user's (encoder), not the host's |

### 7.2 Immediate commands (bypass the buffer)

| Code | Command | Params | Verdict | Notes |
|---|---|---|---|---|
| `01` | Sidetone frequency | 1 | 🟡 | Honour the **paddle-only sidetone** bit (MSB in WK2) — mutes host-sent CW when the rig provides its own sidetone. The frequency is ignored (D7: the M32 pitch stays). For reference: WK2 codes 1–10 (datasheet: 4000/N Hz nominal; real WK2 chips ≈ 3759/N — N1MM's default 469 Hz is N = 8), WK3 `62500/f`. `pwmTone` takes any frequency, so no need to snap to the M32's note table (233–932 Hz) |
| `02` | Set WPM speed | 1 (5–99; 0 = pot) | ✅ | 0 → encoder speed (E1). Clamp to the M32's speed range |
| `03` | Weighting | 1 (10–90, 50 = none) | ✅ | New in the sender (the M32 has no weighting today); host CW only |
| `04` | PTT lead/tail | 2 | ⚪ | D3. Lead-in is *not* applied to the key line either |
| `05` | Setup speed pot | 3 (min, range, 0) | ✅ | Defines the reported window: pot byte = `0x80 \| clamp(wpm − min, 0, range)` (max 31) |
| `06` | Pause | 1 | ✅ | Current character completes first; Clear Buffer cancels |
| `07` | Get speed pot | – | ✅ | Queue a pot byte |
| `08` | Backspace | – | ✅ | Drop the last unsent buffer byte |
| `09` | Pin config | 1 | 🟡 | Key-output enable bits ✅ (both off = sidetone only, no TX keying); sidetone-enable bit ✅; PTT bit ⚪; Ultimatic priority ⚪ (the M32's Ultimatic has its own memory model, `devdocs/ultimatic/`); hang time ⚪ (PTT only) |
| `0A` | Clear buffer | – | ✅ | Also ends pause and tune; cuts the current character |
| `0B` | Key immediate (tune) | 1 | ✅ | Key down until `0B 00`, Clear Buffer, paddle, an M32 button/exit, or the 100 s watchdog |
| `0C` | HSCW | 1 | ❌ | 1000–8000 lpm (≈ 200–1600 WpM); meteor scatter only. Ignore |
| `0D` | Farnsworth WPM | 1 | ✅ | Character speed vs spacing speed in the sender; off when speed ≥ Farnsworth |
| `0E` | WinKeyer mode register | 1 | 🟡 | Echo flags (paddle, serial) ✅, contest spacing ✅, paddle watchdog ✅. Key mode, swap and autospace are ignored (D7) — so bug mode is never needed |
| `0F` | Load defaults | 15 | ✅ | Parse all 15; apply the sending-side ones (D7). Byte 9 = 1st extension in WK2 mode but X2MODE in WK3 mode; byte 15 = unused (WK2) or X1MODE (WK3) |
| `10` | 1st extension | 1 (0–250 ms) | ✅ | Lengthen the first element after the key has been idle — real value for slow-QSK rigs |
| `11` | Key compensation | 1 (0–250 ms) | ✅ | Host CW in the sender. Applying it to the paddle path means touching `doPaddleIambic()` — not in the first version |
| `12` | Paddle switchpoint | 1 | ⚪ | The M32's *Latency* preference plays this role and stays the user's |
| `13` | Null | – | ✅ | Ignore; K1EL start-up sends four of them |
| `14` | Software paddle | 1 | 🟡 | Feed the keyer's paddle state if D8 says so; else ignore |
| `15` | Request status | – | ✅ | Queue a status byte |
| `16` | Pointer command | 1 (+1 for sub-cmd 3) | 🟡 | Overwrite/append position in the buffer, add nulls. Per D8 |
| `17` | Dit/dah ratio | 1 (33–66, 50 = 1:3) | ✅ | New in the sender |

### 7.3 Buffered commands (take effect in sequence)

| Code | Command | Params | Verdict | Notes |
|---|---|---|---|---|
| `18` | PTT on/off | 1 | ⚪ | D3 |
| `19` | Key buffered | 1 (0–99 s) | ✅ | Timed key-down; aborted by paddle or Clear Buffer |
| `1A` | Wait | 1 (0–99 s) | ✅ | Sets the WAIT status bit |
| `1B` | Merge letters | 2 chars | ✅ | Send the two characters' elements with no letter space between them — works for every pair, no prosign table needed |
| `1C` | Buffered speed change | 1 | ✅ | Remember the speed in force; restore on `1E` (or the immediate changes listed in the manual) |
| `1D` | HSCW speed change | 1 | ❌ | As `0C` |
| `1E` | Cancel buffered speed | – | ✅ | |
| `1F` | Buffered NOP | – | ✅ | |

### 7.4 Text, echo and status

- **Characters** `A–Z`, `0–9`, space, `. , ? /` and the K1EL prosign mappings: `"` RR,
  `$` SX, `'` WG, `(` KN, `)` KK, `+` AR, `-` DU, `/` DN, `:` KN, `;` AA, `<` AR, `=` BT,
  `>` SK, `@` AC, `[` AS, `\` DN, `]` KN. Unmapped codes (`! # % & *`) are ignored, and so
  is everything above `0x5D` except `|` (½-dit pad, ✅). We accept lower-case letters as
  upper case (WK3 ignores them; being lenient costs nothing). Element patterns come from
  the M32's own Morse table; prosign output follows hard rule 4.
- **Serial echo** (mode bit 2): each character is echoed after it has been sent; buffered
  commands are not echoed. ✅
- **Paddle echo** (mode bit 6): paddle characters from `MorseDecoder`, sent while the
  break-in status bit is high so the host can tell the two apart. ✅ Hard rule 3: the
  decoder's trailing space must not become an echoed space byte.
- **Break-in:** a paddle press stops host CW at once (WK2 v23 behaviour, not at the end of
  the letter), clears the buffer, and sets BREAKIN; host text that arrives meanwhile is
  dropped (immediate commands still apply); host sending resumes after one word space. ✅
- **Status byte** `0xC0 |` WAIT, KEYDOWN (WK1 format) or 0 (WK2 format), BUSY, BREAKIN,
  XOFF — all derivable from the sender state, sent unsolicited on every change. ✅
- **Pushbutton status bytes** (WK2 mode): never sent. ❌ by design.
- **Standalone mode, standalone settings, EEPROM image:** not applicable — when no host
  is connected, the M32 *is* the standalone keyer, with its own preferences. ❌

### 7.5 Summary

Everything a logger needs to send CW, follow its progress, break in from the paddle, and
read the speed control is ✅. What we do not do is what the M32 lacks the hardware for (PTT,
second key output, pushbuttons), what belongs to K1EL's own tools (EEPROM image, firmware
update, standalone settings), and two rarely used modes (HSCW, RTTY/FSK). The sending-side
extras that WinKeyer has and the M32 lacks today — weighting, dit/dah ratio, keying
compensation, 1st extension — are cheap because they only touch the new sender.

## 8. Facts for D6 and D7 (collected 2026-10-15)

Sources: the source code of CQRLOG (`src/uCWKeying.pas`), flwkey (`src/misc/support.cxx`,
`src/wkey_io.cxx`) and the K3NG keyer (`k3ng_keyer.ino`, `keyer_settings.h`, the most
widely used open-source WinKeyer emulation), plus N1MM Logger+'s documentation of its
Winkey settings (<https://n1mmwp.hamdocs.com/setup/the-configurer/>). Win-Test, DXLog,
WriteLog, Logger32 and UcxLog are closed source; what we know about them comes from
K3NG's change log.

| Host | Start-up | What it sends | Consequence |
|---|---|---|---|
| **CQRLOG** (open source) | DTR on, RTS off; 3 × `13`; Echo Test, waits 300 ms; Host Open, waits **50 ms**, ignores the revision | Only speed, speed-pot setup (**2 of its 3 bytes** — a real WinKeyer then eats the next byte as the third), backspace, tune, clear buffer | Never sends a mode register or sidetone: the M32's paddle settings and pitch stay whatever D7 says |
| **flwkey** (W1HKJ, open source) | DTR on, RTS off; Null, Echo Test (2 s), Host Open (2 s); revision ≥ 20 → sends Set WK2 Mode | A full Load Defaults block from its own settings window (WK2 layout); ~~"Read parameters" fills that window from the 256-byte EEPROM dump~~ - not in flwkey 1.2.4: on the bench its window showed its own saved values after a fresh connect, and only its Done button (apply and close) sends them | flwkey always imposes its own Load Defaults; the M32 keeps paddle and pitch anyway (D7) |
| **N1MM Logger+** (closed; documented) | – | Its Winkey tab: keying mode (Iambic A/B, Ultimatic, **Semi-Automatic = bug**), **Reverse Paddles**, **Sidetone Frequency (default 469 Hz)**, paddle-only sidetone, autospace, lead/tail, 1st extension, key compensation, hang time, three speed-pot behaviours; weight and contest spacing on other tabs. WK3.1 is needed **only** for its RTTY-via-Winkey option | An N1MM user sets paddles and pitch in N1MM, as with a real WinKeyer |
| **K3NG keyer** (open-source emulator) | – | – | Reports **WK2 v23** (or WK1 v10), **never WK3**, and has been used for years with N1MM, Win-Test, Logger32, WriteLog and UcxLog. Applies the host's mode register **in full** (key mode, swap, autospace) and even saves it |

K3NG's change log also lists logger quirks that we should design for from the start:
N1MM needs the **break-in status byte** (stopping a repeating CQ with the paddle depends on
it); Win-Test locked up on a prosign case; Logger32 tripped over Cancel Buffered Speed;
N1MM's pot settings needed a specific pot behaviour; **WriteLog and Win-Test saw random
pauses while the keyer wrote its EEPROM during sending** — for us: never touch NVS during a
session (which the D7 options below all respect).

### What this means for D6 (WK2 or WK3)

- No host was found that *requires* WK3. N1MM uses WK3.1 only for RTTY, which we don't do.
  The most-used emulator has served all the major loggers as WK2 v23.
- WK3 behaviour only starts when the host sends Set WK3 Mode (`00 14`) — which a host that
  sees revision 31 may well do. From then on the sidetone value means `62500/f`, X1MODE has
  a different layout, and **Load Defaults bytes 9 and 15 change meaning** (1st extension →
  X2MODE, unused → X1MODE). Getting that wrong silently scrambles two settings.
- What WK3 would buy us is small: exact sidetone frequency, X2MODE's cut-number and
  paddle-only-sidetone bits (WK2 has the latter in the sidetone byte), and 9600 baud
  (also in WK2).
- **Decided (D6):** implement the WK3-mode handling anyway (it is cheap), reply **23**
  by default from a single constant, and try **31** with N1MM and one more logger on the
  bench.

### What this means for D7 (who owns paddle and pitch)

- A real WinKeyer keeps no settings of its own in host mode: Host Open suspends its
  standalone settings, the host loads its own, and Host Close brings the standalone ones
  back. WinKeyer users are used to setting paddle mode, paddle reverse and pitch in their
  logger. K3NG copies that.
- What actually arrives depends on the host: CQRLOG sends none of these settings,
  flwkey sends its own (or ours, if it read them first), N1MM sends its own — by default
  Iambic B, not reversed, 469 Hz.
- The three ways to resolve it, with their surprises:
  1. **Host owns everything** (WinKeyer/K3NG semantics). Surprise: a left-hander with
     reversed paddles on the M32 finds them un-reversed in WinKeyer mode until they tick
     Reverse Paddles in N1MM; the pitch jumps to N1MM's 469 Hz. Also needs an M32 **bug
     (semi-automatic) mode** for N1MM's fourth keying mode — a small keyer addition, or
     fall back to Iambic B.
  2. **M32 owns paddle and pitch** (the split recommended earlier). Surprise: N1MM's
     Reverse Paddles, keying-mode and sidetone settings silently do nothing — an
     experienced WinKeyer user changes them in N1MM and nothing happens.
  3. **Host owns everything, but starts from the M32's values:** Get Values and Dump
     EEPROM report the M32's own settings (keyer mode, polarity, ACS, pitch), and when the
     session ends everything reverts. Behaves like option 1 with N1MM; with flwkey and
     CQRLOG the M32's settings simply carry on. Costs about the same as option 1.
- In all three, nothing the host sets is written to NVS.
- **Decided (D7): option 2**, the M32 owns paddle and pitch (see §6).


## 9. Future: Remote WinKeyer (with M32KIP)

Willi, 2026-10-15, after the first bench session: WinKeyer should also play nicely with M32KIP
(branch `m32kip`, the V10 line, `devdocs/m32kip/`). The likely shape is a further Transceiver leaf,
**Remote WinKeyer**, next to Remote Keyer / Remote Rig: a logger drives this M32 over USB exactly as
in WinKeyer mode, and the resulting keying goes to a Remote Rig over M32KIP instead of (or as well
as) the local key line. Nothing is designed yet. Questions to settle when both branches meet:

- **Where the keying is captured.** Remote Keyer takes its edges at the key line in `keyOut()` and
  runs in the global loop; the WinKeyer sender also keys through `keyOut()`, so the capture point
  may already be shared - check that both paddle and host CW reach the wire, and that the sidetone
  stays local.
- **Latency vs. the host's view.** The link delay (Remote Rig holds ~150 ms) is longer than the
  WinKeyer status/echo timing a logger expects; echo is sent when the *local* sender finishes a
  letter, so the logger stays in step with the M32, not with the rig. Probably fine; to confirm.
- **Break-in and tune.** Paddle break-in, Clear Buffer and the 100 s tune watchdog must also stop
  the remote key line promptly (Remote Keyer's fail-safe, D14, applies).
- **Port and network together.** The mode owns USB (WinKeyer) and WiFi (M32KIP) at once; the
  classic's 1200-baud switch and the Pocket's auto-detect are unaffected, but auto-detect at the
  top menu would then have to choose between WinKeyer and Remote WinKeyer (or stay local only).
- **Menu label** "Remote WinKeyer" is 15 characters - longer than the 12-character labels the OLED
  menu is hand-fitted to (`"Update Firmw"`); needs a shorter form or a check on both displays.

**Willi, 2026-10-17 (direction, not yet a decision):**

- V10 gets the menu entry **Remote WinKeyer** (WinKeyer keying a remote rig over M32KIP).
- The Pocket's auto-detect must know which of the two to start when a host opens a session at the
  top menu. Two ways: ask on the device at that moment, or a **preference** - Willi leans towards
  the preference. What each costs:
  - *Preference* (e.g. "WinKeyer → local / remote"): a new `prefPos` (enum + `pliste[]` +
    `prefName[]` in step, CLAUDE.md rule 10), one NVS entry, voice clips for its name and values
    (CLAUDE.md §8). No timing risk: the revision byte goes out at once as today.
  - *Asking on the device*: the Host Open reply must still go out within CQRLOG's 50 ms, so the
    question can only come after the reply, while the host already sends its settings; an
    unanswered question needs a timeout and a default, and a blind operator needs it spoken.
    More moving parts for a choice that rarely changes.

### 9.1 Remote WinKeyer — decision list (2026-10-17, for Willi)

**Decided by Willi, 2026-10-17:** all recommendations accepted, with one change to R2. R1: the preference
(**WinKeyer Tx** = Local / Remote, default Local, no `Off`) exists **only on the Pocket** — the classic has no
auto-detect and so no use for it. R2: the menu label is **`Remote WinK`** on the classic (12-character limit) and the
full **`Remote WinKeyer`** on the Pocket. R3–R8 as recommended below (R3a: source 1 while Tune / Key Buffered;
R4 (B); R5 hold BUSY; R6 serve the parser during start-up, stay in the mode on auto-detect errors; R7; R8 yes).
**Implemented 2026-10-17** on branch `remote-winkeyer` (state `morseRemoteWinKeyer`, menu `_remoteWinKeyer`,
preference `posWkTarget`, hooks in `MorseWinKeyer.cpp` / `MorseKipKeyer.cpp` / `keyOut()`); bench checklist in
`BENCH_REMOTE.md`, which also lists five interpretations made while implementing.

Branch `remote-winkeyer` off `m32kip` (b9075fb). Checked against `MorseWinKeyer.cpp`,
`MorseKipKeyer.cpp`, the `keyOut()` hook and the Transceiver chain in `MorseMenu.cpp`. Each item gives the options,
what they cost, and a recommendation (**→**). Items marked *for the record* need no decision.

**R1 — Which mode the Pocket's auto-detect starts: a preference.**
- **→ A preference**, Pocket only (the classic has no auto-detect, D1a), default **Local** so V9 behaviour is kept.
  - Name and values (12-character budget, like every `parName`): **`WinKeyer Tx`** = `Local` / `Remote`, spoken
    "WinKeyer transmitter", "local", "remote". Alternatives: `Host Opens` = `WinKeyer` / `Remote WK`;
    `WK Auto Mode` = `Local` / `Remote`.
  - It decides **only** what a Host Open at the top menu starts. The two menu entries always do what they say.
  - Where it sits: in the "All" view right after **Key ext TX** (both say where keying goes); inside the two WinKeyer
    modes **first** (mode-specific settings first, B16), so it can be changed where it is needed.
  - Cost: one `prefPos` with `pliste[]` + `prefName[]` in step (rule 10, under `CONFIG_M32KIP && CONFIG_WINKEYER`,
    Pocket only); **1 NVS entry** (a `uint8_t` in the existing `morserino` namespace, key ≤ 15 chars, e.g.
    `wkTarget`); not in snapshots (not a training setting); appears in `GET configs` by itself (an added config, no
    protocol version change); **3 clips** via the extractor (name + two values, a few KB of the ~164 KB headroom).
- Optional third value **`Off`** (auto-detect disabled). Free once the preference exists, but it reopens D1a
  ("always on, no preference"). → not unless you want it.
- Rejected alternatives: *ask on the device* (costed above: reply-first timing, timeout + default, spoken prompt);
  *last used* (remember which of the two was last entered from the menu: no menu line, but the same NVS entry and
  an invisible rule that surprises).

**R2 — Menu label and position.**
- "Remote WinKeyer" is 15 characters; menu labels are fitted to 12 (`"Remote Keyer"` is exactly 12).
  **→ `Rmt WinKeyer`**, spoken "Remote WinKeyer" (a pronunciation override, so the Accessibility Edition says it in
  full). Alternatives: `WinKeyer Rmt`, `Remote WinKy`, `Remote WK`. The start screen spells it out in full
  ("Remote WinKeyer" fits a scroll line on both displays, as "Waiting for host" does).
- **→ Position:** iCW/Ext Trx → WinKeyer → **Rmt WinKeyer** → Remote Keyer → Remote Rig → QSO Bot: next to
  WinKeyer, and leading into the two M32KIP entries. A new `menuNo` appended at the end of the enum (menuPtr is
  persisted raw); `_afterWk` / `_beforeKip` updated and the `menu_ring` check re-run; not remotely executable (it
  takes the USB port, like WinKeyer).

**R3 — How the WinKeyer sender feeds Remote Keyer's edge path.** *For the record*, plus one decision (R3a).
- The sender (`keyDown()`/`keyUp()` in `MorseWinKeyer.cpp`) and the paddle keyer both key through `keyOut(…,
  fromHere=true)`, and `MorseKipKeyer::noteEdge()` already sits in `keyOut()` where the local TX line would move.
  So host CW and paddle CW both reach the wire with **no change to the edge path**; the send task, repeat schedule
  (D13), reconnect (D14) and glitch filter are reused as they are.
- **Local TX forced off** in this mode whatever Key ext TX says (Remote Keyer, D12d). The sidetone stays local and
  still follows the host's pin config and paddle-only-sidetone bit.
- The host's "key outputs off" pin config, the paddle watchdog and X2MODE paddle mute stop the **remote** line the
  way they stop the local one today (`txWanted()` gates the edges; a key-up always passes, so a gate changing
  mid-mark can never leave the rig keyed).
- Host CW is machine-made: **source 0**, no glitch filter (it would only delay every edge). A straight key on the
  paddle jack is source 1 with the Glitch Filter, as in Remote Keyer.
- Implementation: its own `morseState` (`morseRemoteWinKeyer`), so "never keys the local jack" is visible in
  `keyOut()` and in no Key-ext-TX case; the ~10 `morseWinKeyer` checks go through one helper. About 1 session.
- **R3a — Tune and long marks.** The rig cuts a source-0 mark at **Rig Limit Kyr (3 s default)**, so a logger's
  Tune (Key Immediate, WinKeyer watchdog 100 s) or Key Buffered stops on the air after 3 s.
  **→ Report source 1 while Tune or Key Buffered holds the key**, so **Rig Limit SK (10 s default)** applies — what
  spec §8 *Tuning* already says for an internal keyer (a mid-session source change the protocol allows). The rig's
  limit always wins over the 100 s watchdog; when STATS reports the limit tripped, the M32 ends Tune itself, so the
  host's status matches the air. Alternative: stay source 0, and the operator raises Rig Limit Kyr from the
  operating position (D17). The manual says either way.

**R4 — The link drops while the host is sending (D14).** The rig has already lifted the key (keepalive timeout);
the question is what the logger is told and what happens to its text.
- (A) **As Remote Keyer:** keep sending locally, sidetone only; nothing reaches the air and the logger never knows.
  Simplest, but a contest auto-CQ carries on into the void.
- (B) **→ Treat the loss like a paddle break-in:** at the moment of loss, abort the letter in progress, clear the
  buffer and raise BREAKIN (as the paddle does — N1MM stops a repeating CQ on exactly that bit, §8). While the link
  is down, host text is taken and **dropped** (no XOFF build-up, BUSY never sticks), the sidetone stays silent, and
  screen and voice say "Link lost" (existing clip). Reconnect is automatic (D14); the next message goes out whole,
  because nothing stale was kept. The paddle behaves as in Remote Keyer (sidetone only while down).
- (C) Hold: pause the sender, keep the buffer, XOFF, resume on reconnect. Rejected: a message — or half of one —
  goes on the air seconds or minutes late.

**R5 — BUSY versus the playout delay.** Echo and BUSY follow the *local* sender, which runs one playout delay
(~150 ms, more on a poor link) ahead of the rig. A logger that switches PTT over CAT (D3: we have no PTT) drops it
when BUSY clears, which would clip the end of every message on the remote rig.
- **→ Hold BUSY for the rig's current playout delay (from STATS) + 20 ms** after the last host key-up. Echo
  characters keep local timing (they only feed the logger's display). Hosts do not wait for BUSY before sending, so
  nothing slows down. Cost: a few lines.
- Alternative: leave BUSY local, and tell PTT-over-CAT users to set a PTT tail ≥ the playout delay in the logger.

**R6 — Start-up, above all from auto-detect.** Remote Keyer's start blocks: WiFi connect up to 11 s (+3.5 s on an
error screen), handshake up to 5 s — and nothing serves the serial port meanwhile. From auto-detect the host's
session is already open (the revision byte still goes out at once), and loggers ask questions straight after Host
Open (status, FW minor revision, Vcc — N1MM asks two, BENCH.md §5).
- **→ Serve the WinKeyer parser inside those waits** (a hook in `wifiConnect()`'s and the handshake's wait loops):
  replies go out, the sender is held, host text buffers and XOFF works. About 0.5 session.
- **Errors, menu entry:** as Remote Keyer — message, back to the menu (no host session exists yet).
- **Errors, auto-detect:** the host session is open and cannot be refused, so **→ stay in the mode**: show the
  error, keep calling the rig where the configuration allows (D14), host text handled as in R4. Never fall back to
  local WinKeyer: that would key the local jack, which this user did not choose. The long press leaves as always.

**R7 — What the display and the voice show.**
- **→ WinKeyer's screen plus Remote Keyer's link logo:** top bar "Host" while a session is open (WinKeyer's slot)
  and the WiFi logo while the rig answers (Remote Keyer's slot); scroll area as in WinKeyer (host text as it is
  sent, paddle text decoded). Start screen: "Remote WinKeyer" / rig address / "Host connected" or
  "Waiting for host".
- Voice: every status phrase exists already ("Connecting", "Calling rig", "Linked to rig", "Link lost",
  "Link restored", "Host connected", "Waiting for host", "Host closed", the rig errors); new clips only for the menu
  label and R1's preference. The protocol text stream is off in this mode (the host owns USB), as in WinKeyer.

**R8 — Preferences inside the mode: are the Remote Keyer (rig) settings offered?**
- **→ Yes:** the same list switch as Remote Keyer (D17) — with the link up and a rig that answers configuration
  requests, the **rig items first** (Rig Delay, Rig Limit Kyr, Rig Limit SK, Rig 1st Ext, Rig Hang), then
  **WinKeyer Tx** (R1), the keyer block (Keyer Mode … Latency), Glitch Filter, Pass Phrase, the general block
  without Serial Output, paddles. **No Key ext TX** (local TX is forced off). WinKeyer's D7 rules apply unchanged
  (the M32 owns paddle and pitch; nothing the host sets reaches NVS).
- Cost: two option lists (with and without rig items) and the existing D17 branch extended to the new state.

**Effort, once decided:** about 2–3 sessions — mode and state (1), R4–R6 (1), preferences, menu, voice, EN + DE V10
manuals and the README 10.0 change log (0.5–1) — plus a two-Morserino bench with a logger (checklist comes with the
implementation).


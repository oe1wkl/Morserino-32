# WinKeyer — bench checklist

Branch `winkeyer` (implements `DESIGN.md`). Everything below is **not yet tested on hardware**.

**Safety first:** the send and tune steps key the transmitter whenever **Key ext TX** is not
Never. Use a dummy load, or leave the key jack empty and watch the keyer LED.

**Flashing:** classic = `heltec_wifi_lora_32_V2`; Pocket = `pocketwroom` (standard) — ask which
edition before flashing a Pocket (the Accessibility Edition needs `pocketwroom-accessibility` plus
`uploadfs`: five new voice clips, pack stamp `4bcfcad5`).

The script: `Software/tests/winkeyer/wk_bench.py PORT [--lines dtr|both|none] [--only step,...]`
(1200 8N2; DTR on / RTS off by default, as CQRLOG and flwkey do). Steps: `echo values open send
prosigns xoff status potspeed buffered defaults tune breakin close`.

## Results so far

**2026-10-15, M32 Pocket (standard edition), macOS, `wk_bench.py` with DTR on / RTS off:**
every step passes - echo (21-25 ms), values, Vcc, Dump EEPROM, Host Open at the main menu
(auto-detect, revision 23 in 22-25 ms), PARIS at 20 WpM in 3.03 s, prosign + merge, XOFF and
Clear Buffer, buffered speed/wait (WAIT seen), Load Defaults in sync, encoder reported as
speed pot, paddle break-in (BREAKIN before the echo, host text dropped, paddle echo "TEST"),
tune, Host Close back to the menu with the M32 protocol answering again. No reboot on port
open. One firmware bug found and fixed (33701a8: closed-state Get Values reported zeros).
Pot mode (E1, `potspeed` step): with no speed command from the host, 7 encoder clicks took the
sending speed from 19.9 to 26.8-27.6 WpM within one word (pot 15 -> 22, pot min 5, i.e. 20 -> 27);
with a host speed set, the encoder only reports the pot, as decided.
**2026-10-15, classic M32 2nd edition (CP2102, macOS's own AppleUSBSLCOM driver), `--lines both`,
mode started from the menu:** all 16 automatic checks pass (echo 73 ms, Host Open 47 ms - at
1200 baud the two request bytes and the reply alone are ~27 ms on the wire, so CQRLOG's 50 ms is
tight but met; PARIS at 20 WpM in 3.07 s). Two practical notes: the CP2102 port refused every
`tcsetattr` (even `stty 115200`: "Invalid argument") for a few seconds after replugging, then
worked; and a host that talks to the classic before the mode is started gets nothing sensible
(the port is still at 115200) - the manual says to start the mode first.

Last-executed mode (2947854): on the classic, WinKeyer chosen from the menu, power cycled - the menu
came back on Transceiver > WinKeyer. Flashing the classic through the 20-in-1 USB-C dock failed
(write timeout, then "serial TX path seems to be down"); plugged straight into the Mac it worked.

**flwkey 1.2.4 on macOS with the Pocket (2026-10-16):** flwkey's port scan globs `/dev/cu.*` and
`/dev/tty.*` but skips every name containing "modem" (an old Apple-modem rule, inherited from the
W1HKJ serial code) - so the Pocket's `/dev/cu.usbmodem…` never appears in Configure → Ports, even
after a restart. Workaround: with flwkey fully quit (closing its window does not quit it, and it
rewrites `~/.flwkey/flwkey.prefs` on quit), set `serial_port_name:/dev/cu.usbmodem12301` in that
file. flwkey then opened the port at start-up and connected (auto-detect at the main menu).
Documented in the manual (Pocket variants). Worth reporting upstream to W1HKJ. Test results with
flwkey: see section 4.

### FINDING (classic): DTR on / RTS off holds the FN button down

On the classic, **FN is GPIO0** (`volButtonPin = 0`), the pin the CP2102 auto-reset circuit pulls
low when DTR is asserted and RTS is not. DESIGN.md §2.4 only saw the download-mode risk of that
state; it is worse: **"DTR on, RTS off" - what CQRLOG and flwkey do, and wk_bench.py's default -
holds FN pressed for as long as the port is open.** Observed on the bench: the classic, at the
main menu, started *Audio In Adj.* (FN long press) - which keys the transmitter.

Consequences with such a logger:
- in WinKeyer mode, the held FN is a long press: the encoder switches to scroll mode and stops
  being the speed control (and FN clicks no longer register);
- if the mode is left while the logger keeps the port open, the menu sees FN held and starts
  *Audio In Adj.*, keying the TX.

"Both on" and "both off" are safe (as is the Pocket, whose FN is not on a strap pin). Options
as written down at the time:
1. Manual only: classic users must set DTR and RTS both on (or both off) in the logger. CQRLOG
   and flwkey are hard-wired to DTR on / RTS off, so they would not be usable on the classic.
2. Firmware, classic WinKeyer mode: ignore FN while it has been held continuously since the mode
   started or for longer than a few seconds (a line, not a finger), until it is released.
3. Firmware, menu: do not start *Audio In Adj.* when FN was already low on arriving at the menu
   (needs a release first) - protects the TX whatever the cause.
2 and 3 together would make the classic safe with every logger; 1 alone is the cheap answer.

**Decided by Willi: 2 and 3, implemented.** `MorseWinKeyer::filterFn()` (only where FN is pin 0):
an FN already down at mode start, or down for more than 3 s, is a line and is ignored until
released; a long press, which ClickButton reports after 1 s while still held, is held back and
delivered at the release. `menu_()`: an FN down on arrival does nothing until released (all
variants). **Verified on the classic with DTR on / RTS off held for 35 s:** the encoder stayed the
speed control throughout (pot bytes 3.8-9.7 s), leaving the mode with the port still held did not
start Audio In Adj. (the menu answered the protocol at 115200 afterwards - Audio In Adj. never
polls serial), and the port was back at 115200.
**Remaining gap:** a logger that opens the port while the classic is still at the *menu* (before
the mode is started) holds FN from then on, and the menu's FN long press starts Audio In Adj. -
the manual says to start the mode first. Cosmetic: leaving the mode sends one stray byte (`FE`)
to the logger as the UART is re-initialised at 115200.

Still open below: classic M32 (hands-on steps, leave the mode -> 115200), DTR/RTS variants, timing on the key line, the loggers, rev 31.

## 1. Before anything else (DESIGN.md §2.4)

- [x] **Pocket, 1200-baud touch:** `pocketwroom` has `board_upload.use_1200bps_touch`. With the
      Pocket at the main menu, run `wk_bench.py PORT --only echo`. It must answer the Echo Test,
      not drop into the bootloader.
- [ ] **DTR/RTS on open and close, each variant**, with `--lines dtr`, `both` and `none`: does
      the M32 reboot when the port opens or closes? (Classic with DTR on / RTS off holds GPIO0
      low; harmless unless it resets meanwhile.) Note the results per variant — the manual's
      DTR/RTS paragraph should be made concrete from them.

## 2. Script, both variants

- [x] Classic: start Transceiver → WinKeyer, run the full script (automatic steps, `--lines both`). Then leave the mode and check
      that the Configuration Tool works again at 115200.
- [ ] Pocket, menu-entered: same.
- [x] Pocket, auto-detect: leave it at the main menu, run the full script. Host Open must answer
      within 50 ms (CQRLOG's limit — the script prints the time) and enter the mode; `close`
      must return to the menu.
- [ ] Pocket: a logger knocking while a training mode runs must NOT take over.
- [ ] Classic 9600: send `00 12` after open, reopen the script at 9600; Host Close must bring 1200
      back. (Check whether a 9600-capable logger — UcxLog — sends `0x12` or `0x11`; K3NG has them
      swapped, DESIGN.md §7.1.)

## 3. Keying timing (logic analyser or scope on the key line)

- [ ] PARIS at 20 and 40 WpM: dit/dah/gaps exact (the sender cancels keyOut()'s 6 ms edges like
      generateCW()).
- [ ] Weighting 60, ratio 60, key comp 10 ms, 1st extension 20 ms (with a tail of 0): each as
      specified, overall speed unchanged for weighting/comp.
- [ ] Farnsworth 25 at 15 WpM; contest spacing (6-dit word gap).
- [ ] Paddle-only sidetone: host CW silent, key line unchanged in timing; paddle has sidetone.
- [ ] Pin config both key outputs off: sidetone only, key line idle.

## 4. Loggers

- [ ] **N1MM Logger+, Windows PC, Pocket standard edition (2026-10-17) - connected, sends:**
      - With N1MM's port DTR/RTS both **Always Off** nothing reached the Pocket, not even in
        WinKeyer mode: the Pocket's native USB (HWCDC) only exchanges data with a host that asserts
        DTR. **DTR Always On / RTS Always Off** works. N1MM applies the lines only when it opens the
        port, i.e. at start-up - changing them needs an N1MM restart. On the first start with the
        new lines the Pocket rebooted once (the switch-over), not on later starts.
      - The Configurer's OK button can be off-screen on a small display; closing the window any
        other way discards the settings.
      - N1MM's default **Paddle only sidetone** made the Pocket key N1MM's CW silently; unticking
        it (and restarting N1MM - it sends its Winkey settings only on connect) brought the tone.
      - Auto-detect from the main menu worked (revision 23); text from N1MM is sent.
      - All of the above is now in the manual (EN/DE, "Example: setting up N1MM Logger+").
      - Still to check with N1MM: Esc, break-in on a repeating CQ, the speed pot, revision 31; and
        N1MM with the classic (the manual's "both Always Off" for the classic is derived from the
        CP2102 behaviour, not yet tried with N1MM).
- [x] **N1MM on the Pocket (2026-10-17, later):** Esc, break-in and the speed pot work.
- [ ] **N1MM on the classic (2026-10-17):** with DTR/RTS both Always Off, the encoder went into
      scroll mode the moment N1MM connected: while opening the port N1MM passes through "DTR on,
      RTS off" for 1-3 s - FN (GPIO0) held briefly, released like a finger, so `HeldLineFilter`'s
      3-s rule delivered a long press. Reproduced on the Mac (script: hold 2 s during the start-up,
      then both off). Fixed: `HeldLineFilter::markLine()` - a press under way while a host
      connects counts as a line however short. WinKeyer mode marks within 2 s of an Echo Test or
      Host Open (not on ordinary text, so FN stays usable while the logger sends); the classic's
      main menu marks on USB input that cannot be protocol text - which at 115200 against a
      1200-baud logger arrives only as **UART framing errors** (the driver drops those bytes;
      first attempt failed on that), now caught with `Serial.onReceiveError`. Verified with the
      imitation in both places, and a finger's 1.5-s press still works in both. Host tests added.
- [x] **N1MM on the classic, re-test (2026-10-17):** with both lines Always Off N1MM's commands
      arrived (a temporary build showed them on screen: A02 0E 05 01 09 04 10 11 03 07 02 02 02 02 -
      no baud change) but **the classic's answers never reached N1MM**, so it sent no text and no
      paddle echo came back - the same "DTR must be on" as the Pocket on that Windows PC. With
      **DTR and RTS both Always On** it works - but N1MM still holds FN (it drives RTS off itself),
      and a twitch of the line during the connect counted as an FN click: the encoder went to
      volume and FN could not bring it back. Fixed: a host opening the session puts the encoder
      back to speed (E1), and a press the filter classed as a line yields no click
      (`HeldLineFilter`, host test added). Re-test passed: Host, encoder = speed, sending, Esc,
      break-in, no scroll mode or Audio In Adj.; FN works again after N1MM quits. Manual: classic =
      DTR/RTS both Always On, FN unusable while N1MM is connected (set the volume first).
- [ ] **N1MM Logger+** (Windows): F1 CQ, Esc stops it, a paddle touch stops a *repeating* CQ
      (needs BREAKIN), paddle echo appears in N1MM, the encoder moves N1MM's speed (pot), N1MM's
      speed field wins over the encoder. Confirm "Reverse paddles", "Keying mode", "Sidetone
      frequency" do nothing (by design, D7).
- [x] **RUMlogNG** (macOS, Pocket, 2026-10-16): CW Interface set to WinKeyer → RUMlog opened the
      port and the Pocket entered WinKeyer mode by itself (auto-detect); text from RUMlog's CW window
      is sent. With RUMlog's "CW side tone" popup at Off the M32 keyed silently (pin config /
      paddle-only bit, host-owned by design); any frequency there brought the tone back - now in the
      manual. Esc stops sending, the paddle stops Auto CQ, speed follows in both directions. (Macros
      such as <his call> are expanded by RUMlog - plain text to the M32, nothing extra to test.)
- [x] **RUMlogNG on the classic 2nd edition** (2026-10-16, plugged straight into the Mac, mode
      started first): no reboot when RUMlog opened the port; sending, Esc, paddle break-in on Auto CQ,
      speed both ways all pass. A short FN press toggled speed/volume normally - so RUMlog does **not**
      hold GPIO0 low (it does not use DTR on / RTS off). Leaving the mode with RUMlog still connected
      landed in the menu, no Audio In Adj. This also covers the classic paddle break-in.
- [ ] **RUMlog** (macOS) or **CQRLOG** (Linux). CQRLOG sends only 2 of the 3 Setup Speed Pot
      bytes, so the next byte is eaten as the third (as on a real WinKeyer) — check that CQRLOG
      still works.
- [x] **flwkey 1.2.4, macOS, Pocket (2026-10-16):** connects (with the prefs-file port workaround,
      above); sending, the CQ message button, paddle break-in with paddle echo, and the encoder as
      speed pot all work. flwkey sends the whole mode register (`0E`) on every Parameters tick box
      (its log shows C4/C6/CC/84): paddle echo acted on, swap and autospace ignored by design (D7).
      Weight, ratio etc. are not sent when the field changes - flwkey only sends them in its
      Load Defaults block, i.e. on **Done** (which applies and closes the Parameters window) and at
      connect. Ratio 66 + Done: dashes clearly
      longer, so Load Defaults is parsed and applied. flwkey 1.2.4 has **no "Read parameters"**
      control, and after a fresh connect its Parameters window shows its own saved values (Min/Rng
      WPM 10/25, not the M32's 5/30) - it never uses Dump EEPROM for that window. Our Dump EEPROM
      reply is therefore only checked by wk_bench.py (format), not by a host program; K1EL's
      WK3tools (Windows) would be the one to try. Still to check: flwkey on the classic.
- [ ] **flwkey** (superseded by the entry above - kept for the classic): connect (revision ≥ 20 → it sends Set WK2 Mode), "Read parameters" (our
      Dump EEPROM: `0xA5` + the 15 settings in Load Defaults order, rest zero) — does flwkey
      show the M32's speed/mode/pitch sensibly? Then send its Load Defaults back.
- [ ] Messages: a logger's / WK tool's "send message 1..6" plays keyer memories 1..6.

## 5. Revision 31 (D6)

**RUMlogNG, Pocket, 2026-10-16 (temporary build answering 31, with a top-line marker for WK3
mode; not committed):** RUMlog sent Set WK3 Mode (`00 14`) - the Pocket showed the marker - and
sending with sidetone, Esc, paddle break-in and speed all worked in WK3 mode, i.e. the WK3 Load
Defaults layout (bytes 9/15 = X2MODE/X1MODE) and the WK3 sidetone handling hold up with a real host.
RUMlog's WK3 settings offer no sidetone frequency change on the M32 (pitch stays the M32's, D7).
The Pocket is back on the committed build (revision 23). Still owed for D6: N1MM with 31.

- [ ] Set `MorseWinKeyer::REVISION = 31`, rebuild, repeat N1MM plus one more logger. Does the host
      send Set WK3 Mode (`00 14`)? If so, check sidetone (62500/f), X1MODE/X2MODE and Load
      Defaults bytes 9/15. Then settle on 23 or 31 and update DESIGN.md.

## 6. Accessibility Edition

- [x] Hear "Win Keyer" in the menu, "Waiting for host" on entry, "Host connected" / "Host closed".
      (2026-10-16, a second Pocket flashed from 10.0-a11y to `pocketwroom-accessibility` + uploadfs,
      no voice-pack alarm. Menu-entered: Host connected, TEST, Host closed, mode kept. Auto-detect from
      the main menu: Host connected, OK, and on Host Close back to the menu with its announcement.
      To return that Pocket to 10.0 it needs the m32kip build *and* its uploadfs.)
- [ ] Host CW cuts a running announcement (CW on the air wins, as for the paddle).

## Decisions taken 2026-10-16 (Willi), on the list below and the classic menu gap

1. **Paddle echo sends one space per word gap** (the M32 decoder's gap, its Interword Spc), never
   twice, never at the start - implemented. The space leaves while BREAKIN is still high as long as
   Interword Spc is at most 7 (the default); above that it arrives just after break-in ends.
2.-9. Kept as they are: letterspace, paddle mute, weighting, speed-pot window, Key ext TX (keys
   unless Never), menu place, TOT (no power-off while a host session is open), Vcc on the classic.
- **Classic menu gap closed (option B):** at the classic's main menu FN goes through the same
  filter as in WinKeyer mode - long press acts on release, a hold over 3 s is a line. The filter is
  now `HeldLineFilter` in `ClickButton.h` (host tests in `Software/tests/clickbutton`). Not
  fixable in firmware, now in the manual: switching the classic on while a DTR-on/RTS-off logger
  holds the port boots it into download mode (dark screen).

**Bench, 2026-10-16, after implementing the decisions:**
- Classic main menu, DTR on / RTS off held 8 s: no Audio In Adj. (menu answered the protocol right after).
- Classic main menu, finger: FN 1.5 s → Audio In Adj. on release; 5 s → nothing. First try found a
  bug - the long press the filter delivers at release was written back into `ClickButton::clicks`,
  which `Update()` clears only after the multi-click window, so Audio In Adj.'s own FN poll saw it and
  quit at once (and WinKeyer mode would have toggled scroll mode repeatedly). Fixed in the filter (a
  released button never passes on a -1 it did not just deliver) and in the menu (acts on a local copy,
  clears the field first); host test `test_filter_written_back_once` added.
- Found on the way, older than WinKeyer: Audio In Adj. on the classic polled FN once per round (1216
  samples + a full OLED redraw), so a quick tap was missed. Now polled while sampling, with a latch.
- Pocket (a11y edition) paddle echo: keyed words arrive as `G TEST TEHM M TE` - one space per word
  gap, inside BREAKIN (`[C2] ... ␣ [C0]`), never doubled or leading.

**Code review of the branch (2026-10-16), nine findings, all fixed:** a tune arriving while the
preferences menu is open is refused (that menu reads serial but never runs the tune watchdog -
verified on the classic: refused inside, CC/C0 outside); merge-letter parameters can no longer index
past the Morse table; the 1st extension also counts paddle keying; ACT_EXIT is sent when a host
closes an auto-opened session; the port is polled once per loop() pass; the sender shares
KEYOUT_EDGE_MS; paddle echo goes through encodeProSigns() (<ka> <ve> <bk> ch now echoed as KA VE
BK CH, ä as ';'); a stale comment; wk_bench.py `--lines none` no longer walks through DTR=0/RTS=1
(which reset the M32 on every open). Regression after the fixes: 17/17 on the classic.

## Interpretations to confirm on the bench (not covered by DESIGN.md)

These were decided while implementing; each is a one-line change if the bench says otherwise.

1. **Paddle echo has no word spaces.** DESIGN.md §7.4 says the decoder's space must not become an
   echoed space byte, so none is sent. K3NG sends one `' '` after a word gap — check what N1MM
   shows for paddle-sent text.
2. **Letterspace (X1MODE)** 0–15 steps of 2 % is applied to the letter space.
3. **X2MODE Paddle Mute** (WK3 mode only) is read as "paddle CW in sidetone only, no TX keying".
4. **Weighting** adds `dit × (w − 50)/50` to every mark (dit and dah alike) and takes it from the
   following gap — so the gap never goes negative up to weight 90.
5. **Speed pot window:** while the host is in pot mode (speed 0) *and* has sent Setup Speed Pot,
   the encoder is held inside that window; otherwise it has the M32's whole range.
6. **Key ext TX:** WinKeyer keys the TX for every value except Never (it is a keyer).
7. **Menu place:** Transceiver submenu, right after iCW/Ext Trx.
8. **TOT:** an open host session keeps the M32 from switching off.
9. **Read Back Vcc on the classic** reports the voltage measured at boot (reading it at runtime
   switches Vext, which powers the display).

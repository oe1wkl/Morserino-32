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

- [ ] **N1MM Logger+** (Windows): F1 CQ, Esc stops it, a paddle touch stops a *repeating* CQ
      (needs BREAKIN), paddle echo appears in N1MM, the encoder moves N1MM's speed (pot), N1MM's
      speed field wins over the encoder. Confirm "Reverse paddles", "Keying mode", "Sidetone
      frequency" do nothing (by design, D7).
- [ ] **RUMlog** (macOS) or **CQRLOG** (Linux). CQRLOG sends only 2 of the 3 Setup Speed Pot
      bytes, so the next byte is eaten as the third (as on a real WinKeyer) — check that CQRLOG
      still works.
- [x] **flwkey 1.2.4, macOS, Pocket (2026-10-16):** connects (with the prefs-file port workaround,
      above); sending, the CQ message button, paddle break-in with paddle echo, and the encoder as
      speed pot all work. flwkey sends the whole mode register (`0E`) on every Parameters tick box
      (its log shows C4/C6/CC/84): paddle echo acted on, swap and autospace ignored by design (D7).
      Weight, ratio etc. are not sent when the field changes - flwkey only sends them in its
      Load Defaults block, i.e. on **Apply** (and at connect). Ratio 66 + Apply: dashes clearly
      longer, so Load Defaults is parsed and applied. Still to check: "Read parameters" (Dump
      EEPROM), and flwkey on the classic.
- [ ] **flwkey**: connect (revision ≥ 20 → it sends Set WK2 Mode), "Read parameters" (our
      Dump EEPROM: `0xA5` + the 15 settings in Load Defaults order, rest zero) — does flwkey
      show the M32's speed/mode/pitch sensibly? Then send its Load Defaults back.
- [ ] Messages: a logger's / WK tool's "send message 1..6" plays keyer memories 1..6.

## 5. Revision 31 (D6)

- [ ] Set `MorseWinKeyer::REVISION = 31`, rebuild, repeat N1MM plus one more logger. Does the host
      send Set WK3 Mode (`00 14`)? If so, check sidetone (62500/f), X1MODE/X2MODE and Load
      Defaults bytes 9/15. Then settle on 23 or 31 and update DESIGN.md.

## 6. Accessibility Edition

- [ ] Hear "Win Keyer" in the menu, "Waiting for host" on entry, "Host connected" / "Host closed".
- [ ] Host CW cuts a running announcement (CW on the air wins, as for the paddle).

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

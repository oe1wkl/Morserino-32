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
Still open below: classic M32, DTR/RTS variants, timing on the key line, the loggers, rev 31.

## 1. Before anything else (DESIGN.md §2.4)

- [x] **Pocket, 1200-baud touch:** `pocketwroom` has `board_upload.use_1200bps_touch`. With the
      Pocket at the main menu, run `wk_bench.py PORT --only echo`. It must answer the Echo Test,
      not drop into the bootloader.
- [ ] **DTR/RTS on open and close, each variant**, with `--lines dtr`, `both` and `none`: does
      the M32 reboot when the port opens or closes? (Classic with DTR on / RTS off holds GPIO0
      low; harmless unless it resets meanwhile.) Note the results per variant — the manual's
      DTR/RTS paragraph should be made concrete from them.

## 2. Script, both variants

- [ ] Classic: start Transceiver → WinKeyer, run the full script. Then leave the mode and check
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

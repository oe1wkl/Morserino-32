# Remote WinKeyer — bench checklist (two Morserinos and a logger)

Branch `remote-winkeyer` (off `m32kip`), implementing `DESIGN.md` §9.1 (R1–R8, decided 2026-10-17).
**Nothing below has been tested on hardware yet.** Builds: all 10 envs; host tests (menu ring, ClickButton,
KIP core + vectors) pass. `wk_bench.py` was not run — it needs a flashed device.

**Set-up.**
- **A** = the operator's Morserino: runs **Remote WinKeyer** (on the classic the menu says **Remote WinK**),
  USB to the computer with the logger. Its own "to Tx" jack must stay quiet throughout: put an LED or a meter
  on it.
- **B** = the station's Morserino: runs **Remote Rig**, "to Tx" into a dummy-load transmitter, or just an LED /
  scope on the key line.
- Both on one LAN to start with. Same **Pass Phrase** on both; **TRX Peer** on A = B's IP address.
- Flashing: classic = `heltec_wifi_lora_32_V2`, Pocket = `pocketwroom`. **Ask which edition before flashing a
  Pocket.** The Accessibility Edition (`pocketwroom-accessibility`) also needs `uploadfs`: four new clips,
  74 unreachable ones pruned (see the commit "a11y: render only reachable clips"), pack stamp `c270c1dc`.
- The script: `Software/tests/winkeyer/wk_bench.py PORT [--lines dtr|both|none] [--only step,...]`. Steps:
  `echo values open send prosigns xoff status potspeed buffered defaults tune breakin close`.

**Safety:** B keys a real transmitter whenever A sends. Use a dummy load.

## 1. Menu and preference

- [ ] Transceiver ring, both directions, on both variants: iCW/Ext Trx → WinKeyer → **Remote WinKeyer**
      (Pocket) / **Remote WinK** (classic) → Remote Keyer → Remote Rig → QSO Bot (or the wrap).
- [ ] **WinKeyer Tx** (Pocket only): in the All view right after Key ext TX; first in the preferences of
      WinKeyer and of Remote WinKeyer; values Local / Remote, default Local; survives a reboot.
      Absent on the classic.
- [ ] Remote WinKeyer's preferences (double click) **without** a link: WinKeyer Tx, keyer block, Glitch Filter,
      Pass Phrase, general block **without** Serial Output, paddles — and **no Key ext TX**.
- [ ] **With** the link up: the list opens on **Rig: Delay**; change Rig Hang, check B stored it (B's own
      preferences). Drop the link while the menu is open: the menu closes by itself.

## 2. Start from the menu

- [ ] B in Remote Rig, A: Transceiver → Remote WinKeyer. Screens "Connecting...", "Calling rig", then the start
      screen **Remote WinKeyer / B's IP / Waiting for host**. Top line: WiFi symbol.
- [ ] Run the full `wk_bench.py` against A (`--lines both` on the classic, default on the Pocket). All steps
      pass; B keys the `send`, `prosigns`, `buffered` and `tune` steps; **A's jack stays quiet**.
- [ ] Leave with a long press: B shows **End: BYE**; on the classic the Configuration Tool works again at 115200.
- [ ] B switched off: A shows **No answer — Check rig and pass phrase** and returns to the menu; the
      Configuration Tool works (port handed back).

## 3. Auto-detect (Pocket)

- [ ] **WinKeyer Tx = Remote**, A at the main menu: `wk_bench.py PORT --only echo,open,send,close`. Host Open
      answered within 50 ms (the script prints it); the mode starts (screens as in 2); **note how long the start
      takes**. The `send` text goes out on B once linked. `close` returns A to the menu, B shows End: BYE.
- [ ] Same with **WinKeyer Tx = Local**: WinKeyer starts as before (regression).
- [ ] Questions during the start: right after `open`, run `--only values` (or `status`) while A is still showing
      "Connecting...": the answers must come back (the parser is served during the start, R6).
- [ ] B off, WinKeyer Tx = Remote: the logger opens; A shows **No answer**, then stays in the mode (no WiFi
      symbol), and the logger's CW produces no sidetone. Switch B on (Remote Rig): A links within a few seconds,
      "Link restored"; the next F-key goes out on B.
- [ ] No pass phrase set on A, WinKeyer Tx = Remote: the logger opens; error screen, the mode stays, host CW is
      dropped; a long press leaves to the menu.

## 4. With a logger (N1MM Logger+ or RUMlog)

- [ ] Logger set up as for WinKeyer (BENCH.md §4: DTR on / RTS off on the Pocket, both on on the classic).
      F1 CQ goes out on B; the paddle breaks in and stops a repeating CQ; the speed pot (encoder) works.
- [ ] Paddle CW (Iambic B, then Straight Key with the Glitch Filter) keys B; decoded paddle text reaches the
      logger if it asked for paddle echo.

## 5. Link loss while the logger is sending (R4)

- [ ] Start a repeating CQ in the logger, then take B away (power off, or long-press it out of Remote Rig).
      Within ≤ 5 s: A's WiFi symbol goes, "Link lost"; **the logger stops repeating** (BREAKIN). B's key is
      up (End: timeout).
- [ ] F-keys while the link is down: nothing on A's sidetone, nothing queued (BUSY never sticks).
- [ ] Bring B back: A reconnects by itself; the next F1 goes out **whole** — no tail of the old message.

## 6. BUSY versus the playout delay (R5)

- [ ] `wk_bench.py -v` (or a logger's status view): after the last element BUSY clears about **Rig Delay +
      20 ms** after the sidetone ends, not at once. Set Rig Delay to 500 ms (double click on A): the hold follows.
- [ ] If a logger with PTT over CAT is at hand (N1MM with a CAT-controlled rig behind B's remote control):
      the end of a message is no longer clipped.

## 7. Tune and long marks (R3a)

- [ ] Logger **Tune for 5 s** (longer than Rig Limit Kyr = 3 s): B keys for the full 5 s (source 1 → Rig Limit SK
      applies). Before this change B would have lifted at 3 s.
- [ ] **Tune for 15 s**: B lifts at 10 s (Rig Limit SK); A ends the tune itself shortly after (status clears).
- [ ] `wk_bench.py --only buffered` with a Key Buffered of 5 s: same as the 5 s tune.
- [ ] Right after a tune, send a CQ: speed and timing at B normal (the source returns to 0 a second later).

## 8. Gates and sidetone

- [ ] Pin config with both key outputs off (`09 02`): A sidetone only, **B stays idle**.
- [ ] Paddle-only sidetone: host CW silent on A, still keyed on B.
- [ ] A's jack: quiet in every step above, whatever Key ext TX says.

## 9. Keying timing at B (scope on B's key line)

- [ ] PARIS at 20 and 40 WpM from the logger: B's marks and gaps match WinKeyer's local timing within the M32KIP
      accuracy (spec §4.3 / D2). Weighting 60 reproduced.

## 10. Classic M32

- [ ] Remote WinK on the classic with a logger at 1200 baud: as WinKeyer (FN/DTR handling unchanged), plus B keys.
      Leaving restores 115200.

## 11. Accessibility Edition (if flashed)

- [ ] Menu entry spoken "Remote Win Keyer"; preference "Win Keyer transmitter" with "Local" / "Remote".
- [ ] Start: "Connecting", "Calling rig", "Linked to rig", then "Waiting for host" / "Host connected".
- [ ] "Link lost" / "Link restored" never play over keying (sidetone not muted).
- [ ] No pack-stamp warning at boot; buildfs log without "File system is full" (checked on the build host).
- [ ] Spot-check a few entries whose raw-text clip was pruned (e.g. WiFi Trx, Preview Char, VBand Kbd, WinKeyer,
      a Tone Pitch value like 622 Hz): each still speaks its override text.

## Interpretations to confirm (not spelled out in DESIGN.md)

1. **Text queued during the start** (R6) is sent once the link is up — after a slow WiFi connect that can be
   ~10 s late. If that is too late for a logger that sends a CQ on connect, the alternative is to drop the
   buffer when the link comes up after more than a few seconds.
2. **BREAKIN at link loss** is raised once and clears one word space later, as after the paddle; it is not held
   for the whole outage. Text arriving while the link is down is dropped without echo.
3. **Source 1 lingers one second** after a long mark (so the rig judges the key-up by the straight-key limit);
   a paddle or host element inside that second also carries source 1 (wpm field 0 at B).
4. **A tune refused while the link is down or still starting** (0x0B is ignored): the logger's tune button does
   nothing then, rather than sounding a sidetone that keys nothing.
5. On a start error from auto-detect with no pass phrase / no WiFi, the mode stays without ever calling; only
   leaving and re-entering (or a new Host Open from the menu) retries.

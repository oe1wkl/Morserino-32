# Remote WinKeyer — bench checklist (two Morserinos and a logger)

Branch `remote-winkeyer` (off `m32kip`), implementing `DESIGN.md` §9.1 (R1–R8, decided 2026-10-17).
Builds: all 10 envs; host tests (menu ring, ClickButton, KIP core + vectors, protocol selftest) pass.

## Results so far (2026-10-18, one Pocket, standard edition, no rig reachable)

Flashed `pocketwroom` from this branch (firmware 10.0, protocol 1.5). TRX Peer 192.168.1.237 did not answer
(no Remote Rig running), so every remote check below ran **with the link down**; nothing was keyed remotely.

- **Local WinKeyer regression (auto-detect, WinKeyer Tx = Local):** `wk_bench.py` all automatic steps pass
  (echo, values, Vcc, EEPROM dump, Host Open 23–25 ms, send PARIS 3.01 s, prosigns, XOFF, status, buffered,
  defaults, tune, close). One missed status reply in one run, 3/3 on repeat (the known rare HWCDC stall). Not
  run: the hand-operated encoder and paddle checks.
- **Remote WinKeyer by auto-detect (WinKeyer Tx = Remote), rig unreachable:**
  - Host Open answered in 25 ms (revision 23); the mode started.
  - **R6:** an Echo Test every 0.4 s for 25 s through WiFi connect, ten calls to the rig and the "No answer"
    screen: 59/59 answered, slowest 66 ms.
  - **R6:** 100 characters sent right after Host Open: BUSY+XOFF (0xC5) at once, cleared to 0xC0 when the start
    ended without a link (9 s); nothing was echoed or sent.
  - **R4 (link down):** text dropped (no echo, no BUSY), status 0xC0, Tune (0x0B) ignored.
  - Host Close returned to the menu; the M32 protocol worked again at once.
- **Menu entry 61 "Transceiver/Remote WinKeyer"**, not remotely executable (correct, as WinKeyer).
- **Found and fixed (commit "a failed start leaves the menu usable"):** after Remote Keyer failed to start ("No answer"), `menuExec()` returned
  without setting `m32state` back to `menu_loop`, so the protocol answered every later `PUT menu/start` with
  NOT AT MENU (m32kip bug; the new Remote WinKeyer failure path had copied it). Both fixed and confirmed.
- **Not caused by this branch, noted for later:** a `PUT menu/stop` sent while no mode loop is running (e.g. while
  Remote Keyer is still connecting) stays armed in `goToMenu` and makes the next mode exit at once. Several other
  early returns in `menuExec()` (empty Practice Set, file player errors, WiFi Trx setup failure) also leave
  `m32state` at `active_loop`.

## Results with the link up (2026-10-18, Pocket = Remote WinKeyer by auto-detect, classic 2nd edition = Remote Rig)

Same LAN, new pass phrase on both (set by Willi), Pocket TRX Peer = the classic. The classic ran a `-D
KIP_MEASURE=1` build for its KIPM/KIPT reports (normal build put back afterwards); its "to Tx" was empty. One
script drove both (`PUT menu/start now/50` on the classic, 1200-baud WinKeyer bytes to the Pocket).

- **Sending:** PARIS PARIS at 20 WpM reproduced at the rig, marks and spaces within −45…+38 µs of the
  Pocket's key line; CQ CQ after a reconnect likewise. The classic's menu shows "Transceiver/Remote WinK".
- **R3a:** a 5 s Tune went through whole (key-down-limit counter `kdl` stayed 0 — source 1, so Rig Limit SK).
  A 15 s Tune was cut by the rig at 10 s (`kdl` 1) and the Pocket ended its own tune 10.13 s in (status
  KEYDOWN+BUSY → BUSY → idle).
- **R5:** after Tune off BUSY stayed up 150–160 ms = the rig's playout delay (125–140 ms) + 20 ms. After text
  ending in a word space the hold is invisible, as designed (the word gap outlasts it).
- **R4:** the rig leaving (its BYE) raised BREAKIN on the Pocket within 10–50 ms; the long text in progress
  stopped ("VVV V"), BREAKIN cleared one word space later, text sent while the link was down was dropped (no
  echo, status idle). The rig back: the Pocket relinked within ~3 s by itself and the next text went out whole.
- **Found and fixed (commit "M32KIP keyer: a late BYE copy no longer drops the next session"):** the rig's
  three BYE copies arrived while the keyer was already calling and left `gByeSeen` set, so the next session was
  dropped as soon as it landed — in Remote WinKeyer a spurious BREAKIN ~3 s after the rig came back (and a
  wasted reconnect cycle in Remote Keyer). Confirmed gone on the re-run.
- **Fixed (master branch `winkeyer-status-flush`, carried here):** an unsolicited status byte (e.g. BUSY → idle
  at the end of a message) sometimes reached the host only with its next write — reproduced as 7 of 40 with the
  link up (none in 120 locally). `pump()` now flushes bytes that have sat in the HWCDC ring for 10 ms: 0 of 120
  withheld afterwards, PARIS timing unchanged.

Still open: the screens and voice (watch the display during a run), the paddle and encoder, the menu entry from
the device, a real logger (N1MM / RUMlog), a link across the Internet, the classic as Remote WinKeyer, and the
Accessibility Edition.

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
- [x] Questions during the start: answered throughout (59/59 Echo Tests, ≤ 66 ms; 2026-10-18).
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

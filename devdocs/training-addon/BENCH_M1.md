# M1 bench — protocol 1.6 trainer events

*Written 2026-10-30 with the M1 firmware change (branch `training-addon`). Willi runs this;
Claude cannot bench. Run it on a **classic M32 (1st/2nd edition)** and on an **M32 Pocket**.*

## 0. Build and flash

Built clean for `heltec_wifi_lora_32_V2`, `pocketwroom` and `pocketwroom-accessibility` on
2026-10-30. Before flashing, decide the edition per device — standard or Accessibility — and say
which (never inferred; the Accessibility image also needs its SPIFFS voice pack).

From `~/Documents/GitHub/Morserino-32-training-addon/Software/src`:

```
pio run -e heltec_wifi_lora_32_V2 -t upload     # classic
pio run -e pocketwroom -t upload                # Pocket, standard edition
```

No new on-device text, no new preference: nothing owed to the voice pack.

## 1. Automated part (≈ 1 minute per device, hands off)

Close the Config Tool and any serial monitor first. From `Software/tests/protocol`:

```
python3 selftest.py
python3 trainer_events.py
```

What it checks — all from the host, against the device's own sounding:

| # | Check | Why |
|---|---|---|
| 0 | `GET capabilities`: protocol 1.6, `trainer-events` | discovery |
| 1 | `cw/play/paris` and `paris paris`: one `done` `end`, timed against the sounded length | **R13**: `done` at the last element, not a word space later |
| 1b | `PUT config/Interchar Spc` inside the running Keyer changes that timing at once | open item 4 (fixed in this change: `setParameter()` now calls `updateTimings()`) |
| 2 | `cw/stop`: `ok` first, then `done` `stopped` | reply order a command queue relies on |
| 3 | `cw/repeat`: no `done` while it repeats | |
| 4 | `cw/play` over a running `cw/repeat` ends once | it used to inherit the repeat (fixed) |
| 5 | unplayable text: `done` at once | fallback path in `fetchNewWord()` |
| 6 | `cw/stop` while idle: `ok`, no `done` | |
| 7 | Serial Output **Nothing / Keyed / All**: `done` always arrives; with Keyed the played text is **absent** from the stream, with All present | open item 2 (needed for M3c) |
| 8 | `menu/stop` mid-text: `done` `stopped` | leaving the Keyer now stops playback (it used to resume later — fixed) |

The script sets 20 WpM and restores the speed, Interchar Spc and Serial Output afterwards
(each `PUT config` writes NVS — see §4).

## 2. Hands-on part

```
python3 trainer_events.py --paddle
python3 trainer_events.py --echo 120
python3 trainer_events.py --echo 90 --echo-menu "Koch Trainer/Learn New Chr"
```

- `--paddle`: when asked, touch a paddle during the repeating PARIS. Expect `done` `stopped`
  and **no** `{"ok":...}` (1.6 change: the paddle stop used to send one nobody asked for).
- `--echo 120`: start the Echo Trainer with a paddle touch, then answer some prompts right, some
  wrong, correct one with `<err>` (eight dits) before finishing, and let one run out (Echo Think
  Time). Every `trainer`/`result` is printed and validated; the run must see at least one OK, one
  ERR and one timeout. **Look at the numbers:** `latency` should feel like your reaction time
  (a few hundred ms to 2 s), `duration` like the time you spent keying.
- `--echo-menu "Koch Trainer/Learn New Chr"`: let some repetitions pass **without** answering —
  expect **no** events for those; then answer a few — expect `"mode":"learn"` and a `koch` value.

## 3. Pocket only: R1 delivery under load

```
python3 trainer_events.py --soak 500
```

500 random 1–5-letter items, each timed from `ok` to `done` against its sounded length. Pass:
none missing, all within −80…+200 ms. The printed min/median/p95/max is the number the M3
trainer will trust; please paste it back. Run it on the classic too if time allows (UART, expected
to be tighter).

## 4. Answers already found in the code (confirm on the bench where marked)

- **Open item 3 — `PUT config` is not session-scoped.** `setParameter()` writes NVS on every
  `PUT config` (`writePreferences("morserino")`). So the trainer must read Serial Output (and
  spacing) on connect, restore them on goodbye and on `pagehide`, and say in the UI that it changes
  them while it runs. Snapshots are not touched. No new NVS entries (existing keys are rewritten).
- **Open item 4 — speed and spacing.** `PUT control/speed` applies at once (`changeSpeed()` →
  `updateTimings()`). `Interchar Spc` / `InterWord Spc` via `PUT config` did **not** until the
  next speed change or mode start; fixed in this change, checked by step 1b.
- **Open item 2 — `cw/play` text with Serial Output = Keyed:** by code path absent (generated
  text takes the `0b100` path); step 7 confirms it.

## 5. Optional

- **BLE Serial build:** with a BLE session and a USB session both handshaken, a `cw/play` from
  USB produces its `done` on both transports (events go to every handshaken transport). The v1
  trainer is USB-only (R4), so this is a nice-to-know, not a gate.
- Paste a few captured objects (`-v` prints them) for the selftest fixtures — one `done` of each
  reason and one `result` of each kind, from each variant.

## 6. Results

| Device | Edition | selftest | trainer_events | --paddle | --echo | Koch learn | soak (min/med/p95/max) |
|---|---|---|---|---|---|---|---|
| classic | standard | OK | 28/28 (2026-10-31, inside the `--paddle` run) | both paddle checks OK | 47/47, OK / ERR / timeout seen | 40/40 (`"koch":5`, `u`, M32 order) | +15 / +76 / +141 / +153 ms, 500/500 |
| Pocket | standard | OK | 27/27 (2026-10-30); 28/28 on M1 + ZLP (2026-10-31) | both paddle checks OK | OK / ERR / timeout seen | 41/41 after the koch fix | −41 / +7 / +75 / +79 ms, 500/500 (3rd run, 2026-10-31) |

Notes from the Pocket run (2026-10-30):
- R13 end alignment: `paris` −74 ms, `paris paris` −22 ms, `paris` at Interchar 6 −2 ms against the
  sounded length (window −80…+200). The −74 is the first play after the Keyer started; the soak will
  show whether that is start-up jitter or a pattern.
- `GET control/speed` at the start had its reply withheld until the harness nudged it with `GET
  capabilities`: the known HWCDC residual, not M1 (the new events flush).
- The closing speed restore took the menu object announced after `menu/stop` as its reply; the
  harness now waits for the `control` reply and reports it as a check.
- `--paddle` run: both paddle checks pass (`done` `stopped`, no unrequested `ok`). Its automated
  part and the one before `--echo` failed with `done` `stopped` at 0 ms after `ok`: any paddle
  contact stops `cw/play` (`m32_v6.ino:1260`, pre-existing; M1 only reports it). A hand near the
  touch paddles while waiting for the hands-on part. The script now says "hands off" first.
- `--echo 120`: 8 results, OK / ERR / timeout all seen, latency 713–1045 ms, durations plausible;
  undecodable keyed patterns show as `*`, as on the display. Captured objects are now the
  selftest fixtures.
- **The withheld `GET control/speed` reply is a 64-byte reply** (exactly one full USB packet),
  withheld in every run. Hypothesis: HWCDC sends no zero-length packet after a full packet, so the
  host's read stays open until the next traffic — which would also explain the old ~1/900
  residual ("always after 6–7 + 64·k bytes"). Pre-existing, but it reaches M1: a `trainer`/`result`
  of 64·k bytes would wait for the next traffic. Probe: `Software/tests/protocol/usb64_probe.py`.
  **Confirmed 2026-10-30:** 63-byte replies 20/20 arrived, 64-byte replies 19/20 withheld with 0
  bytes at the host. Core 2.0.17's `hw_cdc_isr_handler` never sends a zero-length packet after a
  full final packet. **Fixed on master 2026-10-31** (PR #226, `cb6a10a`): `M32Tee::flushReply()`
  now ends every reply with a zero-length packet; benched 0/20 withheld, 1000 `kochlesson` rounds
  with 0 stuck. Both M1 events already call `flushReply()`, so they get the ZLP once master is
  merged into `training-addon`.
- **2026-10-31: the first Koch learn run tested the wrong firmware.** The ZLP fix's own bench had
  re-flashed the Pocket with master, so the run got no 1.6 capability and no events at all (17 failed).
  Master (`26742e9`) was then merged into `training-addon` (`4cce896`, with M1 still uncommitted on
  top; the only conflict was the changelog in `Software/README.md`, both sides kept). Both variants
  build and the selftest passes. The Pocket was flashed with M1 + ZLP (standard edition). **The rest of
  the bench runs on this image:** the hands-off part again, then Koch learn and the soak.
- **Koch learn, first run on M1 + ZLP:** 6 results, OK and ERR both seen, latency 392–1418 ms,
  duration ~650 ms for one character. It found two problems:
  - Firmware: the `koch` field was missing. Learn New Chr and Preview Char leave `kochActive` off
    (that flag also steers the word probabilities), but the spec says "every Koch Trainer mode".
    Fixed in `echoTrainerEval()`, so these modes now report it too. The protocol doc now says the
    same, plus: `repeat` is always 0 there, and there is never a timeout.
  - Harness: it asked for a timeout in Learn mode, where an unanswered repetition sends nothing by
    design. It is now mode-aware:
    - It checks the `mode`.
    - It checks that `koch` is in every Koch result and in no other result.
    - Learn and Preview must send no timeouts.
    - The validator rejects a learn or preview result that has no `koch` or is marked timeout.
      The selftest's old learn-timeout fixture, written from the source, was impossible, so it now
      sits among the fixtures that must be rejected.
  - Re-flashed, standard edition. **Re-run: 41/41**: 6 results, all `"mode":"learn"` with
    `"koch":17`, OK and ERR seen, no event for the repetitions left unanswered, latency 557–1578 ms.
    Captured objects replace the selftest's learn fixtures.
- **Soak, first run (2026-10-31): 500/500 `done` events arrived, but the timing failed:**
  min −10, median 164, p95 269, max 342 ms; 149 outside the window. The harness was at fault.
  - **What the firmware does:** after a text's last element the keyer keeps a word space
    (`m32_v6.ino` ~2818). The soak sent the next item 0.2 s after `done`, so most items waited
    ~200 ms for that word space, and the harness counted the wait as lateness. This is correct CW,
    and `done` still comes at the last element (R13). The protocol doc now tells a trainer about the
    word space.
  - **What changed in the harness:** items now start idle (they sleep past the word space). It
    also prints the three best and three worst items, and keeps a model of the word-space wait in
    case items are sent closer together.
  - **Not explained yet:** the spread. A full wait adds ~+215, yet the minimum was −10. The
    re-run's worst-item list should show where it comes from.
  - The closing speed restore got no reply (64 bytes). The check now nudges the device and reports
    WITHHELD or "never sent", to rule out a ZLP residual.
- **Soak, second run (items start idle):** 500/500 `done` events arrived, all with held 0. Errors:
  min −103, median −27, p95 48, max 87 ms; 83 outside the window, all on the early side. The single
  `paris` was −95 ms.
  - **The ±100 ms spread came from the host's clock reads, not the device.** `M32Link` opens the
    port with a 0.1 s timeout and read `read(4096)`. pyserial waits out that timeout unless 4096
    bytes come, so every arrival time, `ok` and `done` alike, was 0–100 ms late at random, and their
    difference scattered by ±100 ms. The R13 numbers from 2026-10-30 (−74 / −22 / −2) carry the
    same noise.
  - Now it reads `read(max(1, in_waiting))` (`m32_link.py`, and `_play_and_capture`), which returns
    on the first byte. That also speeds up every other harness script. The median of −27 ms may be
    real; the re-run will tell.
  - The speed restore passed this time. But it restored 20 WpM, because the failed restore of the
    first soak had left the device at 20 instead of 19. Why that reply was missing is still unknown;
    the nudge diagnostic stays in.
- **Soak, third run (exact host reads): PASS, 30/30.** 500/500 items; min −41, median +7, p95 +75,
  max +79 ms. One outlier is at −41; the rest lie between 0 and +79 ms, spread evenly rather than
  bunched around one value. That looks like the start of playback falling on some ~80 ms cycle, a
  wait before the first element rather than anything at the end. The three latest items all begin
  with a dah (`t…`); worth a look if M3 needs it tighter. **For the M3 trainer:** `done` comes
  0–80 ms after the sounded length, measured from `ok`.

Notes from the classic run (2026-10-31, standard image `heltec_wifi_lora_32_V2`, M1 + ZLP + koch fix):
- Automated part: 27/28. `paris` +22 ms, Interchar 6 +23 ms.
- The one failure was `paris paris` at +203 ms (the limit is +200). During that play the volume
  went 12 → 8 in five steps: the knob was turned. That is ~40 ms per step, so a volume change
  stretches the sounding on the classic, probably the status-line redraw holding the generator.
  This is pre-existing, not M1: `done` still comes at the last element, just of a longer sounding.
  **For M3:** a trainer that times against the sounded length must allow for a volume turn in the
  middle of a prompt. The script now says "hands off the paddles and the knob".
- **Decisions (Willi, 2026-10-31):**
  - The paddle stop no longer sends an `ok`: **kept**. Only `done stopped` is sent; `PUT cw/stop`
    still answers `ok` first.
  - Pocket prompt 338, repeat 1 (`"answer":""` with latency 837 and duration 2594): it was `<err>`
    with nothing keyed after it. Not a bug. `<err>` erases the whole echo answer
    (`m32_v6.ino` ~3230). The protocol doc now describes this case.
- `--paddle` run with hands off the knob: 30/30, both paddle checks included. `paris` +21 ms,
  `paris paris` +68 ms, Interchar 6 +22 ms.
- **Soak: PASS, 30/30.** 500/500 items; min +15, median +76, p95 +141, max +153 ms.
  - Unlike the Pocket's even 0–80 ms, the delay grows with the item: one letter (`t`, `r`, `s`)
    +15 ms, five letters (`arats`, `siert`, `apsep`) about +150 ms. So the sounding is stretched
    while it plays; the delay is not a wait at the start.
  - Not explained: `paris` at the start of a run is only +21 ms. Most likely the OLED's display work
    holds the generator now and then, e.g. a scroll once the soak's text fills the screen. Not
    verified, and not M1 (`done` still comes at the last element).
  - **For M3 on the classic:** `done` comes 15–155 ms after the sounded length, more for longer
    prompts.

# M32 Mode Comparison Matrix

Audit of every interactive mode against the axes in the briefing. Companion
files: `todo-resolutions.md` (every `TODO(audit)` answered) and `divergences.md`
(ranked refactoring backlog). Audit HEAD = `9478983`; citations relative to
`Software/src/Version 6 and newer/`.

---

## Summary

**Both variants build cleanly at HEAD.** `heltec_wifi_lora_32_V2` (Classic/OLED)
→ SUCCESS (Flash 60.2 %); `pocketwroom` (Pocket/TFT, default env) → SUCCESS
(Flash 65.9 %). Build from `Software/src/` with `pio run -e <env>`.

**The three biggest consistency problems**

1. **The games fork the control grammar.** The classic modes define a tight
   gesture set (turn = speed; red-click = speed/volume; red-long = scroll;
   black-double = prefs; black-long = exit). The four games each re-improvise:
   start gesture varies (paddle / click / both), the **red long-press is
   overloaded** (exit, *or instant forfeit in Invaders*, *or high-scores in
   Morsel*), and live **speed/volume bypass `changeSpeed`/`changeVolume`** or are
   absent. (H1–H3 in `divergences.md`.)
2. **Persistence is fragmented.** Three NVS namespaces (`morserino`, `m32game`,
   `radiocave`), game scores split across them, no uniform score-reset, no schema
   version on game saves — and `playerCall`/`playerName` are stored yet
   **unreachable from the preferences menu** (set only inside Fight the Pileup or
   over serial). (M3, M5, L5.)
3. **A structural split drives duplication.** Classic modes run inside the global
   `loop()` state machine (`m32_v6.ino:884`); games and the QSO Bot are
   self-contained blocking `::run()` functions. The QSO Bot reimplements the
   classic control block almost verbatim, and each game hand-rolls its own button
   + `resetTOT` handling. (L6.)

**Three things that are already pleasingly uniform**

1. **One escape hatch truly is universal.** Black-knob **long-press → menu**
   works in every classic mode, every game, and the bot (`m32_v6.ino:1077`,
   `MorseGame.cpp:709`, `MorseQsoBot.cpp:960`, etc.). The convention's core
   promise holds.
2. **All CW audio goes through `MorseOutput::pwmTone`** via `keyOut()`
   (`m32_v6.ino:3208`); no mode invents a tone path. Games and the bot are
   correctly **local-sidetone-only** through the dedicated `morseGame`/
   `morseQsoBot` states, which are excluded from every RF/external-TX gate
   (`morsedefs.h:324-333`).
3. **The QSO Bot is a model conformer.** It reuses `changeSpeed`/`changeVolume`/
   `encoderState`/`setupPreferences` unchanged (`MorseQsoBot.cpp:938-986`),
   demonstrating the classic grammar is portable to a new mode on both variants —
   exactly the target state for the games.

Also uniform: table-driven menu navigation (`MorseMenu.cpp:133`), shared scroll/
wrap via `DisplayWrapper` + `NoOfVisibleLines`, and a consistent start-splash
helper (`showStartDisplay()`).

---

## Complete menu tree

Source: `menuText[]`/`menuNav[]` (`MorseMenu.cpp:67,133`), enum `menuNo`
(`morsedefs.h:350`). `[L]` = present only when LoRa enabled (classic);
`[G]` = `CONFIG_CW_GAME` (TFT); `[Q]` = `CONFIG_QSO_BOT` (both variants).

```
CW Keyer
CW Generator
├─ Random · CW Abbrevs · English Words · Call Signs · Mixed · File Player
Echo Trainer
├─ Random · CW Abbrevs · English Words · Call Signs · Mixed · File Player
Koch Trainer
├─ Select Lesson          (preferences-style picker; stays in menu)
├─ Learn New Chr          (echo, generatorMode = KOCH_LEARN)
├─ CW Generator → Random · CW Abbrevs · English Words · Mixed
└─ Echo Trainer  → Random · CW Abbrevs · English Words · Mixed · Adapt. Rand.
Transceiver
├─ LoRa Trx [L] · WiFi Trx · iCW/Ext Trx
QSO Bot [Q]
├─ SOTA/POTA · Standard · Contest
CW Decoder
Games [G]
├─ Morse Invaders · Fight Pileup · Radio Cave · Morsel
WiFi Functions
├─ Disp MAC Addr · Config WiFi · Check WiFi · Upload File · Update Firmw · Wifi Select
Go To Sleep
```

`menuN` is computed at compile time (`morsedefs.h:337`): base 43, −1 without
LoRa, +5 with Games, +4 with QSO Bot. **Variant note:** the Classic build has
LoRa Trx but no Games/QSO-Bot-by-default differences; the Pocket build has
Games + QSO Bot but **no LoRa Trx** (`LORA_DISABLED`).

---

## Mode comparison matrix

**Controls** key — Turn/Click/Long are the **black (encoder) knob**; RED is the
second button. "classic std" = the row behaves exactly like the incumbent
classic grammar.

| Mode | Start | Exit | Speed / Volume | Prefs entry | CW audio | Koch filter | NVS | Code structure |
|---|---|---|---|---|---|---|---|---|
| **CW Keyer** | active on entry | black-long | classic std (turn=spd, RED=spd/vol) | black-double | `pwmTone` via keyer | n/a | `morserino` (prefs, cwMem) | global `loop()` `morseKeyer` |
| **CW Generator** (6 sub) | armed → paddle/black-click | black-long | classic std | black-double | `pwmTone` via `generateCW` | no (free) | `morserino` | global `loop()` `morseGenerator` |
| **Echo Trainer** (6 sub) | armed → paddle | black-long | classic std; adaptive uses `changeSpeed` + restore | black-double | `pwmTone` | no (free) | `morserino` | global `loop()` `echoTrainer` |
| **Koch: Select Lesson** | n/a (picker) | black-click back | n/a | — | — | sets `kochFilter` | `morserino` `kochFilter` | `displayKeyerPreferencesMenu` |
| **Koch: Learn New** | armed → paddle | black-long | classic std | black-double | `pwmTone` | yes (`kochActive`) | `morserino` | echo path, `KOCH_LEARN` |
| **Koch: Gen/Echo** (9 sub) | as Gen/Echo | black-long | classic std | black-double | `pwmTone` | **yes** (`kochActive`) | `morserino` | global `loop()` |
| **LoRa Trx** `[L]` | active on entry | black-long | classic std | black-double | `pwmTone`; RF via RadioLib | n/a | `morserino` (loraQRG…) | global `loop()` `loraTrx` |
| **WiFi Trx** | active (after pair) | black-long | classic std | black-double | `pwmTone`; UDP/ESP-NOW | n/a | `morserino` (wlan…) | global `loop()` `wifiTrx` |
| **iCW / Ext Trx** | active on entry | black-long | classic std | black-double | `pwmTone` + decoder | n/a | `morserino` | global `loop()` `morseTrx` |
| **CW Decoder** | active on entry | black-long | enc default = **volume** | black-double | decode only | n/a | `morserino` | global `loop()` `morseDecoder` |
| **QSO Bot** `[Q]` (3) | active on entry | black-long | **classic std (reused verbatim)** | **black-double** | `pwmTone` (local only) | content | `morserino` `playerCall` | self-contained `run()` |
| **Morse Invaders** `[G]` | **paddle/touch** | black-long **and RED-long** | RED toggles spd/vol but writes prefs **directly** | none (lobby FN-cycle) | `pwmTone` (local) | **yes** (lesson) | **`m32game`** `hs*` table | self-contained `run()` |
| **Fight the Pileup** `[G]` | **click** (lobby) | black-long / RED-long | own FN spd/vol (code challenge) | none (lobby) | `pwmTone` (local) | content | `morserino` `playerCall/Name` | self-contained `run()` |
| **Radio Cave** `[G]` | **click** | black-long / RED-long | not exposed in play | none | `pwmTone` (local) | content | **`radiocave`** `save` blob | self-contained `run()` |
| **Morsel** `[G]` | **click or key** | black-long; RED-long = **high scores/exit (varies)** | RED = **word length**, not volume | none (lobby) | `pwmTone` (local) | **yes** (lesson) | `morserino` `hi/hv/wlen` | self-contained `run()` |

Per-axis detail for the games (lobby/result/score specifics) is in the notes
below; control divergences are itemized in `divergences.md`.

---

## Top-bar inventory  (resolves UX §4 / line 103)

**Classic standard** — status line built by `updateTopLine()` +
`displayCWspeed()` (`m32_v6.ino:2433,2407`). Positions are character columns.

| Col | Content | Notes |
|---|---|---|
| 1 | `x2` / `<>` (Generator only) | word-doubler / auto-stop indicator |
| 2 | keyer-mode letter `A B U N S`, or `d` (Decoder) | `getKeyerModeSymbol()` |
| 3 | `SK` / `CK` | straight-key vs iambic |
| 3–6 | `(eff)` WpM in Gen/Echo, `r NN s` rx-wpm in Trx | effective/Farnsworth or decoded |
| 7–8 | **WpM value — bold when encoder in speed mode** | the shared "speed slot" |
| 10–12 | `WpM` label | — |
| right | volume bar (`displayVolume`) | highlighted in volume mode |
| right | LoRa / WiFi logo | only while transmitting |

**Games** replace the bar with bespoke TFT HUDs (allowed by UX §0/§4): Invaders
shows Score/Lives/Level/WPM (`drawHUD`, `MorseGame.cpp`); others similar. **None
reuse the standard WpM slot or format** — a candidate for a shared "WpM chip"
even within game HUDs. **QSO Bot** uses the classic status line unchanged.

---

## NVS inventory  (resolves CLAUDE.md §4)

| Namespace | Keys (representative) | Written by | Read by |
|---|---|---|---|
| `morserino` | `wpm`, `sidetoneVolume`, `brightness`, `kochFilter`, `kochCharsLength`, `theme`, `vAdjust`, `lastExecuted`, `fileWordPtr`, `fPartCount/Sel`, `loraBand/QRG/Power`, `wlanSSID*`, `wlanPassword*`, `wlanTRXPeer*`, `useEspNow`, `version_major/minor`, snapshots; **`playerCall`, `playerName`**; **Morsel `hi`,`hv`,`wlen`** | `MorsePreferences.cpp`, `m32_v6.ino` (snapshots, serial identity 3852/3861), `MorsePileup.cpp:325`, `MorseMorsel.cpp` | prefs, all modes, Morsel, Pileup, QSO Bot, `MorseJSON` |
| `m32game` | `hs%d_s` (score), `hs%d_k` (koch), `hs%d_l` (sublevel), `hs%d_w` (wpm) — top-N table | `MorseGame.cpp:176` | `MorseGame.cpp:159` |
| `radiocave` | `save` (single struct blob: progress + state) | `MorseRadioCave.cpp:283` | `MorseRadioCave.cpp:265` |

**Versioning:** only `morserino` carries `version_major/minor`. `m32game`,
`radiocave`, and Morsel's `hi/hv` are **unversioned** (L5). **Scores split** three
ways: Invaders→`m32game`, Morsel→`morserino`, Radio Cave→`radiocave`, Fight the
Pileup→none persisted (multiplayer). See `divergences.md` M5.

---

## Lifecycle & per-mode notes

**Classic entry** — all show a `showStartDisplay()` splash, then Keyer / Trx /
Decoder are immediately active; Generator & Echo enter **armed** (paddle or
black-click starts generation). (`MorseMenu.cpp:447-746`.)

**Morse Invaders** (`MorseGame.cpp`): lobby (FN cycles WPM/Vol/Level, paddle
starts) → countdown → playing (black-click = pause, black-long = exit, RED-click
= encoder spd↔vol toggle, **RED-long = instant Game Over**) → level-up → game
over. **Result screen:** Score / level / WPM / accuracy / hit-miss-drop +
high-score table + "NEW HIGH SCORE!"; prompts **"Click: Play Again"** /
**"Long press: Exit"** (`MorseGame.cpp:959-1027`). Scores: `m32game` top-N.

**Fight the Pileup** (`MorsePileup.cpp`): lobby with name/call entry
(`enterString`), single/multi select; code-challenge has FN spd/vol toggle and an
explicit per-attempt **timeout** ("Timeout %ds"); result **"Click: play again"**
/ **"Long press: exit"**. Multiplayer over ESP-NOW. Identity in `morserino`; no
persistent high score.

**Radio Cave** (`MorseRadioCave.cpp`): text-adventure; **click to start**;
auto-saves to `radiocave` `save` on exit; in-game `NEW`+`Y` wipes the save and
restarts. Resumable (UX §3.4 conformant — saves silently, no y/n prompt). No
rounds/play-again.

**Morsel** (`MorseMorsel.cpp`): lobby ("Click or key to start"; RED = word
length; RED-long = high scores); single & ESP-NOW multiplayer ("Start as
Server"/join, roster). Result → high scores → lobby. Scores: `morserino`
`hi/hv`. Koch-lesson aware.

**QSO Bot** (`MorseQsoBot.cpp`): three contest flavors; classic status line and
**classic control grammar reused verbatim** (turn=spd/vol/scroll, RED=toggle/
scroll/dim, black-double=prefs, black-long=exit). Local-sidetone-only. Uses
`playerCall`. Token matching trims/normalizes input (`m32_v6.ino:3629` pattern).

---

## Items needing on-device verification

- **M4 — LCD left-edge start-up flicker** (`divergences.md`): static analysis
  points at the `DisplayWrapper` `init()` / `Vext`-settle / backlight-before-clear
  sequence (`m32_v6.ino:549-559`, `MorseOutput.cpp:640`), but the exact frame
  requires a scope/eye on hardware. Any fix **must be re-tested USB-only (no
  battery)** to avoid the reverted #157 black-screen regression. **VERIFY-ON-DEVICE.**
- Several control gestures (paddle-debounce feel, double-click windows) are
  timing-dependent and were read statically; behavior on real encoders should be
  confirmed when changing H1–H3.

---

## Coverage

Every §3 mode appears above. Out-of-scope per briefing: pure preferences screens
(documented only as *entered* via black-double-click), WiFi Functions (firmware
update / file upload / network select — utility, not interactive training), and
hardware drivers below `MorseOutput`/`DisplayWrapper`.

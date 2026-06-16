# Divergence List (Refactoring Backlog)

Sorted by **user-visible impact** (high → low), then **effort** (S → L) within a
band. Each entry: what differs · which modes (citations) · convention's verdict
or decision needed · impact · effort · risk.

Markers: **INTENTIONAL?** = may be a deliberate design choice, confirm before
changing. **VERIFY-ON-DEVICE** = static analysis insufficient, needs hardware.
**DECISION NEEDED** = Willi picks A/B.

Citations are relative to `Software/src/Version 6 and newer/`.

Legend — impact: High (breaks muscle memory / confusing), Medium (noticeable
inconsistency), Low (cosmetic / internal). Effort: S (<½ day), M (1–2 days),
L (multi-day / cross-cutting).

---

## HIGH impact

### H1. Game start gesture is not unified — **S**
- **Differs:** to begin play, Morse Invaders requires a **paddle/touch**
  (`MorseGame.cpp:661,701`), Radio Cave a **click** (`MorseRadioCave.cpp:734-738`),
  Fight the Pileup a **click** from the lobby (`MorsePileup.cpp:926`), Morsel
  **either click or key** (`MorseMorsel.cpp:642`).
- **Convention:** UX §3.2 asks for one rule (`TODO(audit): click or first CW
  input — pick one`). Classic Generator/Echo arm and start on **paddle**.
- **Verdict / DECISION NEEDED:** pick one start gesture for all games. Reusing
  the classic "paddle to start" is the natural choice, but "click to start" is
  friendlier in a menu-derived flow. 
- **Impact:** High · **Effort:** S · **Risk:** low (per-game one-liners).

### H2. Exit gesture is overloaded and inconsistent across games — **M**
- **Differs:** classic modes exit on **black-knob long-press only**
  (`m32_v6.ino:1077-1080`). Games additionally bind **red long-press**, and its
  meaning varies *by state and by game*:
  - Morse Invaders: red long-press = **exit** in lobby/paused/game-over, but
    **instant forfeit (Game Over)** while playing (`MorseGame.cpp:730,856-857,916,1022`).
  - Morsel: red long-press = **high scores** in the lobby, **exit** while playing
    (`MorseMorsel.cpp:709,879`).
  - Radio Cave & Pileup: red long-press = **exit** in most states
    (`MorseRadioCave.cpp:778`, `MorsePileup.cpp:966`), other meaning in text/entry
    sub-states.
- **Convention:** UX §1 "one escape hatch, everywhere; no mode may redefine a
  global gesture." The black-knob long-press IS preserved everywhere (good); the
  *added* red-long-press overloads are the violation.
- **Verdict:** keep black-long-press as the universal exit; remove/repurpose the
  conflicting red-long-press bindings (esp. Invaders' "forfeit", Morsel's
  "high scores"). 
- **Impact:** High · **Effort:** M · **Risk:** changes behavior players have
  learned per-game; needs the manual updated in lockstep.

### H3. Games don't reuse the standard speed/volume mechanism — **L**
- **Differs:** classic speed/volume = red-click toggles `encoderState`, encoder
  turn calls `changeSpeed()`/`changeVolume()` (`m32_v6.ino:1044-1058,1125-1130`).
  QSO Bot **reuses this exactly** (`MorseQsoBot.cpp:938-986`). The four games do
  **not**:
  - Morse Invaders mimics the gesture (RED toggles speed↔vol) but writes
    `MorsePreferences::wpm`/`sidetoneVolume` **directly**, bypassing
    `changeSpeed/changeVolume` and their clamping/JSON/persist side-effects
    (`MorseGame.cpp:831-855`).
  - Fight the Pileup has its own FN spd/vol toggle in the code-challenge
    (`MorsePileup.cpp:1088,1315`).
  - Morsel repurposes the red button for **word length**, not volume
    (`MorseMorsel.cpp:703`); Radio Cave exposes neither during play.
- **Convention:** UX §0/§2 — a game that needs speed/volume must reuse the
  classic mechanism, never fork it; speed must be displayed the standard way and
  the user's setting restored on exit.
- **Verdict:** route all in-game speed/volume through `changeSpeed/changeVolume`
  (extract a shared "live-adjust" helper, see L6); standardize the gesture.
- **Impact:** High · **Effort:** L · **Risk:** touches every game loop and their
  HUD redraws; verify Koch/Farnsworth timing still recomputes.

---

## MEDIUM impact

### M1. No uniform way to reset scores — **S**
- **Differs:** Radio Cave has in-game `NEW` + `Y` confirm to wipe its save
  (`MorseRadioCave.cpp:1306-1314`). Invaders / Morsel / Pileup have **no** reset
  UI. `resetDefaults()` (`MorsePreferences.cpp:1530`) clears only `morserino`, so
  it wipes Morsel `hi/hv` but not Invaders (`m32game`) or Radio Cave (`radiocave`).
- **Convention:** UX §7 `TODO(audit): decide and document a uniform way to reset
  scores`.
- **Verdict / DECISION NEEDED:** one reset path (a preferences item "Reset game
  scores", or a uniform per-game lobby action).
- **Impact:** Medium · **Effort:** S · **Risk:** low.

### M2. Result-screen / control-hint phrasing is inconsistent — **S**
- **Differs:** the same two choices are worded differently per game:
  `"Click: Play Again" / "Long press: Exit"` (Invaders `MorseGame.cpp:996-997`,
  Pileup `MorsePileup.cpp:1790-1791`) vs `"click = skip   hold = quit"` /
  `"click = high scores   hold = quit"` (Morsel `MorseMorsel.cpp:377,943`) vs
  split `"Click"` / `"to start"` (Radio Cave). Casing also drifts
  ("Play Again" vs "play again").
- **Convention:** UX §3.5 — same wording + same input mapping in every game.
- **Verdict:** adopt one phrasing/casing ("Click: …" / "Long press: …") and one
  vocabulary (pick "Long press" *or* "hold", not both).
- **Impact:** Medium · **Effort:** S · **Risk:** none (strings only) — update
  manual screenshots.

### M3. Player call/name not settable from the preferences menu — **M**
- **Differs:** `playerCall`/`playerName` (NVS `morserino`) are set only by Fight
  the Pileup's in-game `enterString` (`MorsePileup.cpp:325-326,851`) or the serial
  protocol (`m32_v6.ino:3852,3861`); they are **not** `prefPos` items
  (`morsedefs.h:387`). A user who never opens Pileup cannot set their identity on
  the device, yet Morsel/QSO Bot consume it (`MorseMorsel.cpp:1155`,
  `MorseQsoBot.cpp:188`).
- **Verdict:** add a preferences entry to view/edit call & name (extra-scope
  item #2). **Caveat:** `pliste[]` parameters are `uint8_t` value/min/max/mapping
  (`MorsePreferences.h:162-171`) — they **cannot hold strings**. This needs a new
  string-preference editing flow (reuse the on-device text entry pattern that
  Pileup's `enterString` / the WiFi-SSID entry already implement) rather than a
  plain `prefPos`.
- **Impact:** Medium · **Effort:** M · **Risk:** `prefPos`/`pliste[]`/`prefName[]`
  must stay in sync (silent NVS panic otherwise); a wholly new string-pref widget
  is required — don't shoehorn into the numeric pref list.

### M4. LCD start-up flicker on the left edge — **M · VERIFY-ON-DEVICE**
- **Symptom (extra-scope item #3):** a brief flicker at the left of the TFT at
  boot. The Pocket/TFT build binds `display` to the **`DisplayWrapper` library**
  (`MorseOutput.cpp:54`); the panel uses `TFT_OFFSET_X=35` on a 240-wide ST7789
  (`platformio.ini`, `M32PocketLGFX.h:90`). Boot sequence: `Vext` on with **no
  settle delay** (`m32_v6.ino:549`) → `initDisplay()` → `display.init()` →
  `display.clear()` (`MorseOutput.cpp:640-661`). A left-edge flash is consistent
  with (a) panel GRAM/backlight lit by the library `init()` before the offset
  window + first themed clear are applied, and/or (b) no power-settle delay
  between `Vext` and `init`.
- **Constraint:** the in-tree replacement (`M32PocketLGFX.h`) caused a current
  spike / USB-only black screen and was reverted (#157→#174); the library path is
  mandatory. **So the fix must be firmware-side sequencing, not a backend swap.**
- **Verdict / experiments (on device):** (1) add a short delay after `Vext` before
  `initDisplay()`; (2) start backlight at 0 and ramp only after the first
  `clear()`+`refreshDisplay()`; (3) confirm CASET/RASET window+offset is applied
  before any pixels are lit. Mark **VERIFY-ON-DEVICE**.
- **Impact:** Medium (every boot, cosmetic) · **Effort:** M · **Risk:** high-touch
  area — must re-test **USB-only, no battery** to avoid re-introducing the #157
  black-screen regression.

### M5. NVS namespace sprawl — **M**
- **Differs:** three namespaces — `morserino` (prefs + Morsel/Pileup/QSO data),
  `m32game` (Invaders scores), `radiocave` (Radio Cave save). Flat, ad-hoc keys.
  (`MorseGame.cpp:161`, `MorseRadioCave.cpp:263`, `MorseMorsel.cpp:1154`,
  `MorsePileup.cpp:312`.)
- **Convention:** CLAUDE.md §4 wants one canonical namespace + key scheme.
- **Verdict / DECISION NEEDED:** pick a scheme (e.g. keep `morserino` for prefs,
  one `m32scores` namespace for all game scores with `<game>.<item>` keys), and a
  migration for existing installs.
- **Impact:** Medium · **Effort:** M · **Risk:** **NVS migration** — must read old
  keys once and rewrite, or users lose scores/saves.

### M6. CW-source visual distinction not standardized — **M**
- **Differs:** Echo/Trx/Bot use font weight (incoming `REGULAR`, outgoing `BOLD`,
  `MorseOutput.h:33-34`; `m32_v6.ino:2485,2499`); games render their own colored
  text with no shared rule for user-CW vs machine-CW vs system messages.
- **Convention:** UX §5 `TODO(audit): standardize … and apply uniformly`.
- **Verdict:** define the canonical mapping (weight for mono displays, a palette
  role for TFT) and apply across Echo, Transceiver, QSO Bot, and games.
- **Impact:** Medium · **Effort:** M · **Risk:** low-moderate (display only).

### M7. Single black-click means different things across classic modes — **S to document / L to change** · **INTENTIONAL?**
- **Differs:** black single-click = **start/stop** in Generator/Echo, but
  **select keyer memory** in Keyer/CW-Trx (`m32_v6.ino:1090-1106`).
- **Convention:** UX §1 wants a stable per-gesture meaning; this is a *classic-vs-
  classic* split, so Willi decides which wins (§Guiding principle).
- **Verdict / DECISION NEEDED:** likely **INTENTIONAL** (different mode needs) —
  if kept, document it explicitly in UX_CONVENTIONS as an allowed context split.
- **Impact:** Medium · **Effort:** S (document) / L (unify) · **Risk:** unifying
  would break long-standing keyer-memory muscle memory.

### M8. Game-specific settings don't use the preferences mechanism uniformly — **M** · **INTENTIONAL?**
- **Differs:** some game settings are real `prefPos` items editable via the
  normal menu (`posInvaderOrient`, `posTheme`, `posQsoBotContestType`,
  `morsedefs.h:402-410`); others are lobby-only and not in preferences at all
  (Invaders WPM/level FN-cycle `MorseGame.cpp:712`, Morsel word length).
- **Convention:** UX §10 — settings reached/edited the same way for every mode.
- **Verdict / DECISION NEEDED:** decide which game settings belong in the
  preferences menu vs the lobby, and apply consistently.
- **Impact:** Medium · **Effort:** M · **Risk:** low.

---

## LOW impact

### L1. Stray untracked `m32_v6.ino 2.cpp` in the source tree — **S**
- 117 KB **untracked** duplicate of the 177 KB `m32_v6.ino` (an old copy) sits in
  `Software/src/Version 6 and newer/`. It is the exact "multiple definition"
  hazard `scripts/clean_ino_dupes.py` exists to strip; it survives on disk
  between builds (iCloud/Finder copy). Builds pass because the pre-build hook
  removes it, but anyone compiling via Arduino IDE or with the hook disabled
  hits a link error.
- **Verdict:** delete it; consider adding `*.ino [0-9]*.cpp` to `.gitignore`.
- **Impact:** Low (local only) · **Effort:** S · **Risk:** none.

### L2. Orphaned `M32PocketLGFX.h` dead code — **S**
- The in-tree TFT LGFX wrapper from the reverted #157 is **included nowhere**
  (TFT path uses `DisplayWrapper`, `MorseOutput.cpp:54`). It only `extern`s a
  `display` that is never instantiated.
- **Verdict:** remove, or add a header comment marking it as a retired
  experiment, so no one resurrects the #157 regression by `#include`-ing it.
- **Impact:** Low · **Effort:** S · **Risk:** none (confirm no env defines a flag
  that pulls it in — none found).

### L3. CLAUDE.md §3.4 references a non-existent `buildCwStream` — **S**
- The hard rule names `buildCwStream` as the prosign-preserving function; **no
  such symbol exists**. Real path: `encodeProSigns()` (`m32_v6.ino:3035`) →
  `generateCWword()` (`m32_v6.ino:1868`) → `cleanUpProSigns()` (`m32_v6.ino:2561`).
- **Verdict:** correct the rule's wording (doc-only).
- **Impact:** Low · **Effort:** S · **Risk:** none.

### L4. Duplicated UI helpers across games — **S**
- `drawCentredText()` is defined independently in `MorseGame.cpp` and
  `MorsePileup.cpp`; `enterString()` (on-device text entry) is Pileup-local
  (`MorsePileup.cpp:763`) though name/call entry is wanted elsewhere (M3).
- **Verdict:** hoist a shared `drawCentredText` and a reusable text-entry widget
  into `MorseGameMode`/`MorseOutput`.
- **Impact:** Low · **Effort:** S · **Risk:** none.

### L5. Unversioned game save-state — **S**
- `radiocave` `save` blob, `m32game` score keys, and Morsel `hi/hv` carry **no
  version stamp** (unlike preferences' `version_major/minor`,
  `MorsePreferences.h:71`). A struct/layout change silently loads incompatible
  data.
- **Verdict:** add a one-byte schema version to each game's NVS area and validate
  on read (ties into M5).
- **Impact:** Low (latent) · **Effort:** S · **Risk:** low.

### L6. Duplicated classic control-loop logic — **M**
- The QSO Bot reimplements the classic encoder/red-button block almost
  line-for-line (`MorseQsoBot.cpp:938-1003` ≈ `m32_v6.ino:1033-1153`); each game
  hand-rolls its own `resetTOT()` + button scaffolding. ~60+ lines duplicated per
  consumer (extra-scope item #1).
- **Verdict:** extract a shared "live-controls" helper (encoder→speed/vol/scroll,
  red→toggle/scroll/brightness, black→exit/prefs) that classic loop, QSO Bot and
  games call. Enables H3 cleanly.
- **Impact:** Low (internal) · **Effort:** M · **Risk:** moderate — central to
  every mode; refactor behind the existing behavior, test all variants.

### L7. Implicit vs explicit TOT reset — **S** · **INTENTIONAL?**
- Classic modes never call `resetTOT()` directly; it fires as a side-effect of
  screen-update calls inside `MorseOutput` (`resetTOT()` at `MorseOutput.cpp:1580`,
  called from print/scroll/status). Games call `resetTOT()` **explicitly** on
  every button/encoder event; QSO Bot relies on the implicit path.
- **Risk:** any future user activity that does not repaint would not reset the
  watchdog. The explicit approach (games) is more robust.
- **Verdict:** adopt explicit `resetTOT()` on input as the documented pattern
  (cheap insurance), keep the implicit path as backup.
- **Impact:** Low (works today) · **Effort:** S · **Risk:** low.

### L8. Ad-hoc variant gating vs named feature flags — **M**
- Most variant code uses named flags (`CONFIG_TFT`, `CONFIG_CW_GAME`, …) but some
  gates on bare board/display macros: `#ifdef ARDUINO_heltec_wifi_kit_32_V3`
  (`morsedefs.h:160`), `#ifndef CONFIG_TFT` for `NoOfVisibleLines`
  (`MorseOutput.h:20`).
- **Convention:** CLAUDE.md §1 — variant behavior should ride named config flags,
  not ad-hoc `#ifdef`s.
- **Verdict:** introduce named flags where bare board macros leak behavior.
- **Impact:** Low · **Effort:** M · **Risk:** low (build-matrix retest).

### L9. CW Generator/Echo don't Koch-filter (vs Koch Trainer) — **S** · **INTENTIONAL?**
- Plain Generator/Echo use `posRandomOption`/content type, not the Koch level;
  only Koch Trainer sub-modes filter (`kochActive`, `m32_v6.ino:217,2267`).
- **Verdict:** almost certainly **INTENTIONAL** (the "free" trainers). Document it
  in UX §6 so it isn't mistaken for a bug.
- **Impact:** Low · **Effort:** S · **Risk:** none.

### L10. "Timeout counts as a miss" is not a coded uniform rule — **S** · **INTENTIONAL?**
- Per-round timeouts are game-local: Pileup has an explicit attempt timeout
  (`MorsePileup.cpp:889`); other games have none. No shared "timeout = miss" rule.
- **Verdict:** either codify a shared rule (UX §8) or document that per-round
  timeouts are deliberately game-specific.
- **Impact:** Low · **Effort:** S · **Risk:** none.

---

## Decisions needed from Willi (summary)

1. **H1/H2/H3** — the games' control grammar: one start gesture, drop the
   conflicting red-long-press overloads, route speed/volume through the shared
   mechanism. Highest user-visible payoff.
2. **M5** — choose the canonical NVS scheme and accept a one-time migration.
3. **M3** — confirm adding on-device call/name editing (needs a string-pref
   widget, not a numeric `prefPos`).
4. **M7/M8/L9/L10** — confirm the **INTENTIONAL?** items so they can be written
   into UX_CONVENTIONS as allowed, rather than left looking like drift.

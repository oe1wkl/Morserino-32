# M32 Refactoring Plan

Derived from the 2026-06 consistency audit (`mode-matrix.md`,
`todo-resolutions.md`, `divergences.md`). Item IDs below (H#/M#/L#) reference
`divergences.md`.

**Sequencing principle:** documentation and housekeeping first, then
behavior-preserving refactors that *enable* the user-facing changes, then the
muscle-memory changes (gated on Willi's sign-off), with the hardware-sensitive
flicker isolated. **Every phase ends with: build both variants
(`pio run -e heltec_wifi_lora_32_V2` and `-e pocketwroom` from `Software/src/`)
and update the EN + DE manuals for anything user-visible.**

Status: ☐ not started · ◐ in progress · ☑ done.

---

## Phase A — Documentation consolidation ☑ *(done 2026-06-15)*
*Zero firmware risk. No build needed.*

- ☑ Folded the resolved markers from `todo-resolutions.md` into `CLAUDE.md`
  (config-flag list, build commands, directory map, NVS scheme, English/length,
  word-list storage, extra invariants §3.8–9).
- ☑ Folded the resolved markers into `UX_CONVENTIONS.md` (control grammar §1,
  speed/volume/prefs gestures §2, top-bar slot table §4, prosign rendering §5,
  Koch filtering §6, multiplayer flow §9).
- ☑ Documented the **intentional** items as allowed rules (not drift):
  - **M7** — UX §1a: black single-click = start/stop in trainers vs memory-select
    in Keyer/Trx (allowed context split).
  - **M8** — UX §10/§12: game settings may live in the lobby; shared settings stay
    in preferences.
  - **L9** — UX §6: Generator/Echo deliberately do **not** Koch-filter; only Koch
    Trainer sub-modes filter.
  - **L10** — UX §8: per-round timeouts are deliberately game-specific.
- ☑ **L3** — fixed `CLAUDE.md` §3.4: `buildCwStream` removed; real path documented
  as `encodeProSigns → generateCWword → cleanUpProSigns`.
- ☑ Specified the **target games control-grammar** in `UX_CONVENTIONS.md` §12 for
  Phase F.

## Phase B — Housekeeping ☑ *(done 2026-06-15; both variants build SUCCESS)*
*S effort, no behavior change.*

- ☑ **L1** — stray `m32_v6.ino 2.cpp` was already gone; added `*.ino [0-9]*.cpp`
  guard to `.gitignore` so it can't return.
- ☑ **L2** — removed orphaned `M32PocketLGFX.h` (reverted-#157 dead code, included
  nowhere); fixed the now-stale reference comment in `M32OledLGFX.h`.
- ☑ **L7** — added guarded `MorseOutput::resetTOT()` on paddle (`updatePaddleLatch`),
  button, and encoder activity in `m32_v6.ino`'s `loop()`; implicit screen-update
  reset retained as backup.

## Phase C — Shared helpers ☑ *(done 2026-06-15; both variants build SUCCESS)*
*Behavior-preserving refactor.*

- ☑ **L4a** — hoisted `drawCentredText` into `MorseGameMode::drawCentred()`; both
  games delegate to it. Byte-for-byte identical (W=170, bg=PAL_BG), no behavior
  change.
- ↪ **L4b** — the reusable text-entry widget (Pileup's `enterString`) is TFT-only
  and Pileup-themed, and M3 needs it on **both** variants (OLED has no sprite).
  **Moved to Phase E (M3)**, where the cross-variant design belongs.
- ↪ **L6** — **moved to Phase F.** The shared live-controls handler's final shape
  depends on what the games need (H3 routes in-game speed/volume through it), so it
  is designed once there for `loop()` + QSO Bot + games rather than twice.

## Phase D — Low-risk user-facing consistency ☑ *(done 2026-06-15; both variants build SUCCESS)*
*High value-to-risk. Manuals updated (EN+DE).*

- ☑ **M2** — result/game-over screens standardized to Title-Case `Click: <action>`
  / `Long press: Exit` (Pileup casing fixed; Morsel game-over split into two
  canonical lines). Compact in-play / lobby HUD hints left terse/lowercase
  (width-constrained) — documented in UX §12 + §3.5.
- ☑ **H1** — game start gesture unified to **paddle/key**. Invaders & Pileup were
  already paddle-only (Pileup click = single/multi setting); dropped the
  click-start fallback from the Morsel & Radio Cave single-player lobbies. UX §3.2
  / §12 and the EN+DE manuals updated.
- ↪ **M1** — score reset **moved to Phase E** (decided): build it against the
  consolidated single-namespace store (M5) to avoid resetting 3 namespaces then
  reworking it.

## Phase E — Persistence & player identity ☑ *(done 2026-06-15; both variants build SUCCESS)*
*Light-touch decision: NO namespace migration; existing scores preserved.*

- ☑ **L5** — versioned Morse Invaders high scores (`ver` key). Radio Cave (blob
  `magic`+`version`) and Morsel (`hv`) were already self-versioned. Absent stamp =
  current format → no data loss on upgrade.
- ☑ **M3** — player **Call Sign** / **Op Name** now editable from the preferences
  menu (top-level, both variants) via the new reusable **`MorseTextEntry`** widget
  (encoder char-picker rendered through `MorseOutput` → cross-variant). Saved to
  `morserino/playerCall|playerName`.
- ☑ **M1** — uniform **Reset Scores** preferences action: confirm (FN=yes), then
  clear `m32game`, `morsel` (`hi`/`hv`), `radiocave` (`save`).
- ↪ **M5** (full consolidation) — **deferred by decision.** The game stores are
  already individually versioned and working, so a migration is risk-without-
  benefit. The four namespaces are documented in CLAUDE.md §4. (Audit had wrongly
  put Morsel scores in `morserino`; they live in `morsel`.)
- **Reusable next:** `MorseTextEntry` is built to also serve WiFi credentials
  (replacing the insecure web-form) and CW keyer-memory entry — future work.

**VERIFY-ON-DEVICE:** the `MorseTextEntry` layout on the 14-char OLED and the three
new preferences items (esp. Reset Scores clearing the right keys).

## Phase F — Control-grammar unification ◐ *(in progress)*
*Highest impact; breaks learned behavior. Sign-off 2026-06-18: Invaders give-up
= black-long→menu (forfeit dropped); Morsel lobby high-scores shortcut dropped.
Sign-off 2026-06-20: in Morsel multiplayer, black-long stays "step back one
level" (exit at the top) and the red-long shortcut is dropped. L6/H3/H2 all done;
M6 deferred to its own display-design pass.*

- ☑ **H2** (done 2026-06-20, both variants build SUCCESS) — RED long-press exit
  bindings removed from every game's flow so **black-knob long-press is the sole
  game exit everywhere**: Invaders; Morsel (SP **and** the multiplayer
  mode-select / role-pick / server-client wait / MP results); Pileup
  (lobby/challenge/results **and** the main playing loop — `MorsePileup.cpp:1703`,
  an a7ff6b6 miss closed here); Radio Cave (never had a red-long). Morsel's
  multi-level MP flow keeps **black-long = step up one level, exiting at the top**
  (2026-06-20 decision); the server lobby's red *single*-click = word length is
  kept. Manuals corrected EN+DE (Invaders + Pileup game-exit lines, Morsel MP
  back-nav). (Pileup's text-entry RED-long = the widget's "done" — intentionally
  kept.)
- ☑ **H3** (done 2026-06-19, both variants build SUCCESS) — all **four** games
  now route in-game speed/volume through the shared value-cores
  `changeSpeedValue` / `changeVolumeValue` (declared in `MorsePreferences.h`):
  Invaders (lobby + playing), Pileup (challenge ×2), Morsel, Radio Cave. (The
  audit guessed Radio Cave didn't expose live speed/volume — it does, so it was
  included.) Each game keeps its own speed model; `wpmMin/wpmMax == 5/60`, so the
  games' hardcoded `5..60` ranges were already equal — speed is behavior-identical.
  Two latent bugs fixed in passing: **Morsel and Radio Cave in-game volume now
  reaches the Pocket codec** (they set `sidetoneVolume` but skipped
  `soundSetVolume` — the same silence Invaders had before `8133a71`), and their
  volume range is normalized `0..20`→`0..19` (canonical `volumeMax`). Net −20
  lines. **H3 touches only the game files + the header; the classic modes still
  call the unchanged wrappers, so classic behavior is unchanged — the
  classic-mode re-test caveat belonged to L6, not here.**
  **VERIFY-ON-DEVICE (Pocket):** speed + volume adjust correctly in each game,
  and Morsel/Radio Cave volume is now actually audible.
- ☑ **L6** (moved from Phase C; done 2026-06-19, both variants build SUCCESS) —
  **speed/volume value-core extracted.** `changeSpeedValue` / `changeVolumeValue`
  (`m32_v6.ino`) now carry the value logic only — clamp + `updateTimings` + codec
  (`soundSetVolume`) + serial `jsonControl` — and draw nothing. The classic
  wrappers `changeSpeed` / `changeVolume` call the cores and add only the classic
  status-line update (plus, for speed, the `charCounter` NVS-write debounce reset).
  Behavior-preserving: `loop()`, the echo trainer and the QSO Bot still call the
  wrappers. The broader input-dispatch unification sketched originally
  (encoder→scroll, RED→toggle/dim, black→exit/prefs) was **not** required for H3
  and was deliberately not undertaken — scope held to the speed/volume path H3
  consumes.
- ☐ **M6** — standardize the CW-source visual distinction (user/machine/system)
  across Echo, Transceiver, QSO Bot, games.

## Phase G — LCD flicker ☐
*Isolated. VERIFY-ON-DEVICE. Blocks nothing.*

- ☐ **M4** — experiment on `Vext` settle-delay, backlight-after-first-clear, and
  offset-window-before-pixels. **Must test USB-only / no battery** to avoid
  re-triggering the reverted #157 black-screen regression. Do NOT swap the
  DisplayWrapper backend.
- ☐ **L8** — replace ad-hoc board `#ifdef`s with named config flags when touching
  display/board code.

---

## Risk-vs-impact ranking (the judgment calls)

| Item | User impact | Risk | Verdict |
|---|---|---|---|
| H1, M1, M2 | High/Med | Low | Do early (Phase D) — easy wins |
| H2, H3 | **High** | Med–High (muscle memory) | Do, gated on sign-off + manual (Phase F) |
| M3 | Medium | Med (new widget) | Worth it; after Phase C helper |
| M5 | Medium | Med (migration) | Worth it **only with** a tested migration |
| M6 | Medium | Low–Med | With Phase F |
| M4 | Medium (every boot) | **High** (hardware regression) | Isolate (Phase G); don't block others |

**Intentional — documentation only (Phase A):** M7, M8, L9, L10.

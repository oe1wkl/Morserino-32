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

## Phase C — Shared helpers ☐
*Behavior-preserving refactor. Enables Phases D–F. Verify no functional diff.*

- ☐ **L4** — hoist `drawCentredText` + a reusable on-device text-entry widget into
  `MorseGameMode`/`MorseOutput`.
- ☐ **L6** — extract one shared "live-controls" handler (encoder→speed/vol/scroll,
  RED→toggle/scroll/dim, black→exit/prefs) used by the classic loop, the QSO Bot,
  and the games.

## Phase D — Low-risk user-facing consistency ☐
*High value-to-risk. Update manuals.*

- ☐ **M2** — unify result-screen / control-hint phrasing and casing.
- ☐ **M1** — one uniform score-reset path.
- ☐ **H1** — unify the game start gesture (recommend classic "paddle to start").

## Phase E — Persistence consolidation ☐
*Contained migration risk. Test upgrade-in-place.*

- ☐ **M5 + L5** — one canonical score namespace/scheme, schema-version byte, and a
  one-time migration reading old `m32game`/`radiocave`/`hi-hv` keys.
- ☐ **M3** — expose player call/name in preferences via the Phase C text-entry
  widget (string pref, not a numeric `prefPos`; keep `prefPos`/`pliste[]`/
  `prefName[]` in sync).

## Phase F — Control-grammar unification ☐
*Highest impact; breaks learned behavior. GATE on Willi's sign-off + manual updates.*

- ☐ **H2** — remove overloaded RED long-press (Invaders forfeit, Morsel lobby
  high-scores); keep black-long-press as the sole exit.
- ☐ **H3** — route all in-game speed/volume through the shared mechanism (Phase C).
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

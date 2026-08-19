# M32 Serial Protocol — Conflict Resolution Plan

Companion to [`conflicts.md`](conflicts.md). Tracks the remaining conflicts to closure, with
Willi's ratified decisions and a phased order so they can be resolved step by step. Tick items
off as they land. Protocol stays at **1.3** for everything here; genuinely new commands are
deferred to a future **1.4** (see the last section).

---

## Already resolved (this audit cycle)

- **C1** firmware version, **C2** reset/defaults — fixed in firmware, marked resolved in `conflicts.md`.
- **C9** hardware string, **C16** stale examples — fixed when the protocol-doc `GET device`
  description + examples were refreshed (still need the ✅ mark in `conflicts.md`).
- **C-NVS-NAMESPACE** — snapshot (`snap0..snap7`) and Morsel namespaces corrected in CLAUDE.md +
  `devdocs/consistency-audit/mode-matrix.md`.

## Ratified decisions (2026-06-22)

| # | Decision |
|---|---|
| **C3** error key | Adopt the firmware's `content`; **fix the doc**, not the firmware. |
| **C5** WiFi password write-only | Intentional (security) — **document only**. |
| **C6** game scores | **Out of scope for 1.3**; record as a **planned 1.4** feature so it isn't forgotten. |
| **C7** hardware settings | **Keep read-only**; document that they can't be restored from a backup. |
| **C13** variant-dependent params | **Document**; the tool already skips absent params. |
| **C-VER** versioning | **(1) now:** document the additive/compatibility rule. **(2) later (1.4):** a `GET capabilities` query. |

---

## Phase 1 — Documentation completeness pass · ✅ DONE 2026-06-22

One coherent sweep of `M32 Protocol.md` + the draft spec + `conflicts.md` housekeeping. No firmware
change (except C4, below, which rode the same branch). Lowest-risk, clears most of the list.
**Status: all items below landed.** (The *optional* part of C14 — making the firmware error name the
byte limit — was left as a future nicety; the ceiling itself is documented.)

- [ ] **C3** — change the error format/example to `{"error":{"content":"…"}}` (the shipped reality).
- [ ] **C8** — fix terminator wording: a single **LF** (`\n`, ASCII 10); a trailing CR is tolerated — drop "carriage return".
- [ ] **C10** — document `GET controls` (plural) → `{"controls":[{name,value}×2]}`.
- [ ] **C11** — document `PUT wifi/select/0` = select EspNow.
- [ ] **C12** — document the `PUT device/protocol/off` reply: `{"end m32protocol":{"content":"Goodbye!"}}`.
- [ ] **C14** — state the `file/data` chunk ceiling (≤256 decoded bytes); *(optional)* have the firmware error name the limit.
- [ ] **C17** — note that snapshots exclude `Serial Output` and `Time-out`.
- [ ] **C5** — note WiFi password is **write-only by design** (never read back).
- [ ] **C7** — note hardware settings are **read-only** (informational; not restorable from a backup).
- [ ] **C13** — note the parameter set is **build-dependent**; a restore must tolerate `INVALID PARAMETER` per item.
- [ ] **C-VER (1)** — add a short **"Versioning & compatibility"** section: additive within a major version; clients must ignore unknown keys; identify via `device.protocol` / `firmware` / `build`.
- [ ] **C6** — add a **"Planned for protocol 1.4"** note: game-score read/clear commands (so it's on record).
- [ ] Housekeeping: mark **C3/C5/C6/C7/C9/C13/C16/C-NVS/C-VER** resolved-or-deferred in `conflicts.md`; clear the corresponding `PENDING-DECISION` markers in `PROTOCOL_SPEC.draft.md`.

## Phase 2 — The one firmware bug · ✅ DONE 2026-06-22

- [x] **C4** — fix `GET control/volume`: swap the min/max args so it reports `minimum:0, maximum:19`
  (`m32_v6.ino:3821`; the inert swap at `:2641` fixed too for consistency). Built clean both variants.

## Phase 3 — Config-tool robustness · ✅ DONE 2026-08-19

- [x] **C-ERR-HANDLING** — `sendAndParse` now raises an `{"error":{…}}` reply as an exception
  (reading `content`, falling back to `name`) and logs it with the command that caused it, instead
  of handing it back to callers that only checked for their own key. Probes that *expect* an error
  (feature detection on older firmware) pass `{quiet:true}` to keep the log clean.
- [x] **C-BRACE** — `waitForResponse` is string- and escape-aware: braces inside a JSON string no
  longer count toward the framing, so a `{`/`}` in a CW memory / SSID / call name can't desync it.
  The same naive counter in `m32_file_manager.html` was fixed with it. **With that, the
  `await sleep(50)` in the CW-memories loop is no longer load-bearing and was removed** — the
  reply is framed correctly in the first place, so there is no tail to wait for.

## Phase 4 — Optional firmware robustness

- [ ] **C15** — cap the serial input-line length in `serialEvent()` and emit an error past the cap; document the maximum command length.

---

## Protocol 1.4 · ✅ SHIPPED 2026-08-19 (branch `protocol-1.4`)

All three deferrals went in one bump, as designed. `M32P_VERSION` is `"1.4"`; the changelog block
and the three new sections are in `Documentation/Protocol Description/M32 Protocol.md`.

- **C6** — `GET game/scores` returns every game's high-score table (Invaders, Morsel, Trailblazer,
  Fox Hunt, both Memory Chain modes) plus whether Radio Cave has a saved game; `PUT
  game/scores/clear` wipes them through the very same code the menu's *Reset Scores* runs
  (`MorsePreferences::clearGameScores`). **Read and clear only — nothing writes a score back**, so
  a host cannot forge one. Both are `CONFIG_CW_GAME`-only, i.e. Pocket builds.
- **C-VER (2)** — `GET capabilities` returns `{protocol, features[]}`, where `features` lists only
  the *build-dependent* commands (`configs/details`, `game/scores`, `stats/log`). Everything else
  is implied by the protocol version. Firmware older than 1.4 answers with an error, which is
  itself the answer.
- **C-BULK** — `GET configs/details[/<from>]`, paginated, 8 parameters per page; **7 round trips
  instead of 50** for the Pocket's 49 parameters (0.31 s vs ~1.2 s of device time over USB).
  Designed in [PROTOCOL_1.4_DESIGN.md](PROTOCOL_1.4_DESIGN.md); see its epilogue for the two design
  points that changed during implementation and the measured results.

**Config tool** got the page loop (with a fallback to the per-parameter loop for pre-1.4 firmware),
the capability query at connect, and a Game Scores panel on the Dashboard tab.

**Hardware status (M32 Pocket, firmware 9.0, 2026-08-19):** flashed and verified **over USB** —
handshake reports protocol 1.4; `GET capabilities` lists `configs/details`, `game/scores`,
`stats/log`; the paged read returns the same 49 parameters in the same order as `GET configs`, with
items byte-identical to `GET config/<name>`; error paths and recovery behave; `GET game/scores`
returns all seven games. **Still unverified:** the same commands **over BLE**; `PUT
game/scores/clear` (not run — it would destroy real scores; the device's tables were empty anyway,
so it would not have proved much); and that a cleared table stays cleared when a grid game or
Memory Chain is started again in the same session (the cache-invalidation fix this branch also made
to the on-device *Reset Scores*).

(The broader `utility-enhancements.md` track — host-side **file backup/restore**, diff, selective
restore — is separate from the conflict list and can proceed in parallel whenever wanted.)

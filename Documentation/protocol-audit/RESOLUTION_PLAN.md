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
  `devdocs/mode-matrix.md`.

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

## Phase 3 — Config-tool robustness

- [ ] **C-ERR-HANDLING** — `sendAndParse` detects an `{"error":{…}}` reply and surfaces the message to the user (read both `content` and `name`).
- [ ] **C-BRACE** — make `waitForResponse` framing string/escape-aware (or "read until it parses"), so a `{`/`}` inside a CW memory / SSID / call-name can't desync it.

## Phase 4 — Optional firmware robustness

- [ ] **C15** — cap the serial input-line length in `serialEvent()` and emit an error past the cap; document the maximum command length.

---

## Deferred to protocol 1.4 (recorded so it isn't forgotten)

When a 1.4 is opened, these become real, additive commands — bump `M32P_VERSION`, add a 1.4
changelog block, document each:

- **C6** — game-score access: e.g. `GET game/scores` and `PUT game/scores/clear`, so a backup tool
  can capture and reset Invaders / Morsel / Radio Cave state.
- **C-VER (2)** — `GET capabilities`: returns the list of supported objects/commands, so a tool can
  adapt to firmware versions without trial-and-error.

(The broader `utility-enhancements.md` track — host-side **file backup/restore**, diff, selective
restore — is separate from the conflict list and can proceed in parallel whenever wanted.)

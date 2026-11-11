# Snapshot bundles — handoff

**Read `DESIGN_NOTES.md` next door first.** It carries the analysis: what a
snapshot actually contains, the CW Schule Graz set transcribed out of cdaller's
trainer, the traps, and a sketch of a bundle file. This file is the practical
half — what is decided, what is not, where the code is, and what to do first.

**Goal:** import a whole set of snapshots in one go, from a **configuration file**
rather than hard-coded definitions, so the same mechanism serves CW Schule Graz,
CW Ops, LICW or anyone else.

**Status 2026-11-11:** format signed off by Willi and **built** on branch `snapshot-bundles`: importer in the Config Tool (Snapshots tab, *Import Snapshot Bundle…*), the Graz bundle at `Software/Utilities/snapshot-bundles/cw-schule-graz.json`, the author's guide at `Documentation/Snapshot Bundles/README.md`, manuals EN/DE. Tested against a simulated device; bench test on hardware owed.

---

## Decided (do not reopen)

* **The Koch lesson is out of scope** (Willi, 2026-09-07). These presets are
  settings for a *type of practice*, repeated at whatever lesson the student has
  reached. A snapshot never carries `kochFilter` and that is correct here — which
  removes the only firmware change the feature looked like it might need.
* **A file, not code.** The whole point is that another school's set is a data
  change, not a patch.
* **Menu by NAME, never by index.** `menuText[]` is conditionally compiled;
  index 32 is *WiFi Trx* on a classic and *iCW/Ext Trx* on a Pocket. Resolve the
  path string against the connected device's own `GET menus`.
* **Entries are partial and that is the normal case.** The Graz set defines five
  to eight settings per slot, never the ~40 a stored snapshot holds.

## Answered by Willi, 2026-11-09 (were "needs CW Schule Graz")

1. **"Wide" and "Koch" are both real.** Wide spacing is for *writing down* what
   you hear, Koch (= narrow, no extra spacing) for *head copy*. The Koch forms all
   use InterWord Spc 6 / Interchar Spc 3. **Consequence:** with only 8 slots a
   student cannot hold both forms of one exercise, so the choice is made at
   import time (see the format proposal below).
   Slot meanings: 6 = abbreviations, 7 = words, **from the File Player** (they
   use their own training files, not English Words), 8 = call signs.
   **Prefixes (V10):** Graz suggested them, so they will probably use them. With
   all 8 slots taken, slot 8 has to serve call signs *and* prefixes. It can,
   because Prefixes use the same settings (Calls Region, Call Prefixes, spacing)
   and ignore Length Calls: recall 8, then turn one step on to
   Echo Trainer/Prefixes.
2. **The bundle names the slot** each entry goes into.
3. **Afterwards the student gets their own settings back**, so the baseline
   restore (the accumulation trap below) is required, not optional.
4. **Hand-editing must be easy and well documented**, whether or not Graz edit
   theirs: other CW schools will write their own bundles.

## Format (signed off 2026-11-11, built)

The real Graz set: `Software/Utilities/snapshot-bundles/cw-schule-graz.json`. Authors' reference: `Documentation/Snapshot Bundles/README.md`.
What it does differently from the sketch in DESIGN_NOTES.md, and why:

* **Values are written the way the device shows them.** For a parameter with a
  list of choices, the file uses the text (`"Length Calls": "3"`,
  `"Random Groups": "All Chars"`, `"Each Word 2x": "ON"`), never the internal
  index. cdaller's `Length Calls 1` means "3", which is exactly the trap a
  hand-editor falls into. Plain numbers stay numbers (`"InterWord Spc": 45`). The
  tool resolves the text against the device's own `mapped values`
  (`GET configs/details`, protocol 1.4; `GET config/<name>` before that), so a misspelt value is an error that lists the
  allowed choices.
* **Menu by its path, as the device prints it** (`"Echo Trainer/Call Signs"`), the
  same string as `menuName` in a snapshot export.
* **One `koch` overlay per entry** instead of a general variants mechanism:
  `"spacing"` at the top names the two forms; an entry's `"koch"` object lists
  only what changes. The student picks wide or Koch once for the whole import.
* **Strict JSON**, so any editor or validator can check it. JSON has no comments,
  so every level allows a `"note"` string, which the importer ignores.

The importer would: read the file; check every slot, menu path, parameter name
and value against the connected device before writing anything; show a preview
(which slots get overwritten, what each sets); then for each entry apply its
settings onto the student's baseline and store it; then **read each stored
snapshot back** (`GET snapshot/<n>`) and report any setting that did not make it
in; and finally restore the student's own settings and menu.

The read-back answers open item 5 below without a firmware change: it catches
`Time-out` and every other excluded setting on every firmware since 1.3. An
`inSnapshot` field would only add a warning *before* writing.

## Open — ours to decide

5. **Add `inSnapshot` to the protocol?** The tool cannot currently warn that a
   setting will not survive into a snapshot. `storedInSnapshot()` knows; nothing
   reports it. One additive line in `fillConfigObject()`, exactly like `default`
   in V9. Without it a bundle cannot be validated before it is applied — and
   cdaller's own slot 2 sets `Time-out`, which is silently dropped.
6. **`PUT snapshot/set` before or after the importer?** It would write a
   snapshot's contents directly, killing the accumulation trap and the
   NVS-commit cost and leaving live settings alone. It is now an *optimisation*,
   not a prerequisite: with partial entries the apply-then-store route is fast
   enough.

---

## The two things that will bite whoever builds this

**The accumulation trap.** `PUT snapshot/store/<n>` snapshots the *live*
configuration; there is no command that writes snapshot contents directly. So
importing N snapshots means N rounds of apply-then-store, and with partial
entries, entry 2 inherits whatever entry 1 changed. "Undefined keeps the value as
it is" silently becomes "as the previous lesson left it". The importer must read
the student's baseline first (`GET configs`), restore anything a previous entry
changed that this one does not define, and put the whole baseline back at the
end.

**Not everything can live in a snapshot.** `storedInSnapshot()` excludes the
device's own behaviour (time-out, serial output, encoder clicks, quick start),
hardware wiring, transmit/connectivity gates, and the game/QSO-bot settings.
Training settings are all included. A bundle that sets an excluded parameter
changes the device and then loses it — see open item 5.

## Where the code is

| | |
|---|---|
| `Software/Utilities/m32_config_tool.html` | `snapImportConfirm()` — the single-snapshot import this generalises; `snapImportMenuNumber` already resolves a menu by name |
| `MorsePreferences.cpp` | `doWriteSnapshot()`, `storeSnapshotBlob()`, `decodeSnapshot()`, `applySnapshot()`, `storedInSnapshot()` |
| `MorseJSON.cpp` | `jsonGetSnapshot()`, `fillConfigObject()` (where `inSnapshot` would go) |
| `Documentation/Protocol Description/M32 Protocol.md` | `GET snapshot/<n>`, `GET menus`, `PUT snapshot/store\|recall\|clear` |
| CLAUDE.md §4 | the NVS budget, if snapshot contents ever change |

## How to work on it

* The config tool is a single file; serve it over `http://localhost` (not
  `file://`, which breaks its `fetch` of `m32_pref_help.json`) and drive it with
  a stubbed `sendLine`/`waitForResponse` to test protocol flows without hardware.
  That is how the #214 review was done and it caught real bugs.
* **Saving the file publishes it.** `~/sync-to-dropbox.sh` now publishes
  `origin/master`, so working-tree edits are safe — but confirm that is still the
  case before editing (`~/sync-to-dropbox.sh --status`).
* Anything user-facing needs Willi's sign-off before it is built, not after.

## Reference implementation to learn from, not copy

`https://github.com/cdaller/morserino32-trainer` ships the Graz set hard-coded in
`js/m32-configuration-ui.js`. The full set is transcribed in `DESIGN_NOTES.md`.
Two bugs in it were reported as
[cdaller/morserino32-trainer#9](https://github.com/cdaller/morserino32-trainer/issues/9):
the raw menu indices, and `Time-out` being set but never stored.

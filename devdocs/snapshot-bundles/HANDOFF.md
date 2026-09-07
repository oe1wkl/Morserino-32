# Snapshot bundles — handoff

**Read `DESIGN_NOTES.md` next door first.** It carries the analysis: what a
snapshot actually contains, the CW Schule Graz set transcribed out of cdaller's
trainer, the traps, and a sketch of a bundle file. This file is the practical
half — what is decided, what is not, where the code is, and what to do first.

**Goal:** import a whole set of snapshots in one go, from a **configuration file**
rather than hard-coded definitions, so the same mechanism serves CW Schule Graz,
CW Ops, LICW or anyone else.

**Status 2026-09-07:** designed to the point of decision, nothing built.

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

## Open — needs CW Schule Graz

1. Are the "wide" vs "Koch" spacing variants the right model, or an artefact of
   how cdaller's buttons happen to be laid out? (They differ in exactly two
   values: InterWord Spc and Interchar Spc.)
2. Does the bundle name its target slots, or does the tool assign them in order?
3. What should the student be left running afterwards — their own settings back,
   or the first lesson ready to go?
4. Do they want to author bundles themselves? If so the format has to be pleasant
   to hand-edit.

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

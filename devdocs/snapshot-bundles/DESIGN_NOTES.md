# Importing several snapshots at once — design notes

**Status: not built.** Deferred 2026-09-06; two of the open questions were
answered on 2026-09-07 and the prior art has now been read, so what remains is
mostly format design. See "Where this stands" at the end.

## What was asked for

CW Schule Graz have defined a set of snapshots for their curriculum, one per
practice. Today a student would import them one at a time through the
Configuration Tool's Snapshots tab (added in #214). The ask is to import a whole
set in one go, from a **configuration file rather than hard-coded definitions**,
so the same mechanism serves CW Ops, LICW or anyone else's scheme.

A bundle entry need not define every preference; the ones it leaves out keep
whatever the student already has.

---

## Answered

### The Koch lesson is not needed (Willi, 2026-09-07)

The earlier draft of this note led with the discovery that a snapshot cannot
carry `kochFilter`, and flagged it as possibly decisive. **It is not.** These
snapshots are settings for a *type of practice*, repeated at whatever Koch
lesson the student has currently reached. The lesson is deliberately not part of
the preset.

That removes the only firmware change this feature looked like it might need.
(The underlying fact still holds and is still worth knowing: `doWriteSnapshot()`
stores the `storedInSnapshot()` subset of `pliste[]`, the menu pointer,
`kochCharsLength`, and the custom-chars pair. `kochFilter` is none of those,
because `posKochFilter` sits past `posSerialOut`. So recalling a snapshot never
changes the lesson — which is exactly the behaviour wanted here.)

### There is prior art, and it is worth copying from

`https://github.com/cdaller/morserino32-trainer` — Christoph Daller's browser
trainer — already ships the CW Schule Graz set, in
`js/m32-configuration-ui.js`, as one hard-coded `setupCwSchoolSnapshotN()`
method per snapshot plus a wired-up button each. Willi's direction: keep the
data, drop the hard-coding.

Read out of that file, this is the whole CW Schule Graz set:

| slot | menu | settings |
|---|---|---|
| 1 | `menu/set/20` | InterWord Spc 30, Interchar Spc 3, Random Groups 0, Length Rnd Gr 1, Each Word 2x 0, Max # of Words 20 |
| 2 | `menu/set/17` | InterWord Spc 7, Interchar Spc 3, Random Groups 0, **Time-out 0**, Each Word 2x 0 |
| 3 | `menu/set/25` then `29` | InterWord Spc 7, Interchar Spc 3, Random Groups 0, Length Rnd Gr 1, Each Word 2x 0, Max # of Words 20 |
| 4 | `menu/set/20` | InterWord Spc 45, Interchar Spc 15, Random Groups 0, Length Rnd Gr 9, Each Word 2x 0, Max # of Words 15 |
| 5 | `menu/set/1` | *(none)* |
| 6 | `menu/set/26` | InterWord Spc 7, Interchar Spc 15, Random Groups 0, Length Abbrev 2, Each Word 2x 0, Max # of Words 20 |
| 7 | `menu/set/8` | InterWord Spc 45, Interchar Spc 15, Each Word 2x 1, Max # of Words 0 |
| 8 | `menu/set/13` | InterWord Spc 45, Interchar Spc 15, Length Calls 1, Each Word 2x 1, Max # of Words 0 |

Four of them (4, 6, 7, 8) have a second "Koch" button. **Those variants differ
only in the two spacing values** — e.g. slot 4 is 45/15 in the wide form and 6/3
in the Koch form. So "Koch" there means *Koch-course spacing*, not a lesson
number, which is consistent with the answer above. Any format needs a way to
express "the same exercise at different spacing" without repeating the entry.

Five observations that shape the format:

1. **Entries are partial** — five to eight settings, never the ~40 a stored
   snapshot holds. This is the normal case, not an edge case.
2. **An entry may set only a menu** (slot 5 is `menu/set/1` and nothing else).
3. **The menu is set by raw number.** That is the cross-variant hazard already
   found and fixed for single-snapshot import in #214: `menuText[]` is
   conditionally compiled, so index 20 is not the same entry on a classic as on
   a Pocket, nor across firmware versions. **A bundle must carry the menu path
   as a string** and resolve it against the connected device's own `GET menus`,
   exactly as `snapImportConfirm()` now does.
4. **Slot 2 sets a preference that a snapshot cannot hold.** `Time-out` is one
   of the historic exclusions in `storedInSnapshot()`, so `PUT config/Time-out/0`
   changes the running device and is then silently dropped when the snapshot is
   written. Recalling that snapshot later does not restore it. Worth telling
   Christoph; worth having the importer *catch*, rather than reproducing it.
5. **Parameter names are the protocol's own display names** (`InterWord Spc`,
   `Each Word 2x`, `Max # of Words`). Those are what `PUT config/<name>/<value>`
   takes, so a file can use them directly and stay readable.

---

## Still open, and still true

### The accumulation trap

`PUT snapshot/store/<n>` snapshots the **live** configuration; there is no
command that writes a snapshot's contents directly. So importing N snapshots
means N rounds of *apply, then store* — and with partial entries that goes wrong
quietly. If entry 1 sets Word Length 3 and entry 2 says nothing about it, entry
2's snapshot inherits the 3, not the student's own value. "Undefined keeps the
value as it is" becomes "as the previous lesson left it".

The importer must therefore read the student's configuration first
(`GET configs`), restore anything a previous entry changed that this one does
not define, and put the whole baseline back at the end.

### Cost

Every `PUT config/<name>/<value>` triggers a full `writePreferences()` NVS
commit. The CW Schule Graz set is ~50 writes across eight slots — tens of
seconds, and three times that over BLE. Tolerable because the entries are
partial. A bundle of *full* snapshots would be minutes.

### `PUT snapshot/set` would remove both problems

A protocol command that writes a snapshot's contents directly would kill the
accumulation trap (nothing accumulates), collapse the cost to one blob write per
slot, and leave the student's live settings untouched. It would also fix the
#214 wart where importing a single snapshot necessarily replaces the running
configuration. Costs: firmware change, two-variant build, protocol 1.5, and a
way to carry ~40 values in one command.

Still the recommendation if bulk import is built at all — but note it is now an
*optimisation*, not a prerequisite: the apply-then-store route works, and with
partial entries it is fast enough.

### A validation gap: the tool cannot tell what is snapshot-storable

Finding 4 above is not something the Configuration Tool can currently warn
about — the firmware does not report which preferences `storedInSnapshot()`
admits, so the tool cannot tell an author that `Time-out` will be dropped. The
cheap fix is the same one-line, additive pattern used for `default` in V9: add
`"inSnapshot": true/false` to the config object in `fillConfigObject()`. Then a
bundle can be validated before a single write is made.

---

## Sketch of a bundle file

Not settled — a starting point for the conversation with CW Schule Graz.

```jsonc
{
  "m32SnapshotBundle": 1,
  "name": "CW Schule Graz",
  "description": "Practice presets for the Graz curriculum",
  "variants": [                          // optional; the "Koch" buttons today
    { "id": "wide", "name": "Wide spacing", "default": true },
    { "id": "koch", "name": "Koch spacing" }
  ],
  "entries": [
    {
      "slot": 4,
      "name": "Random groups, long",
      "menu": "Koch Trainer > CW Generator > Random",   // by NAME, never index
      "settings": {
        "InterWord Spc": 45, "Interchar Spc": 15,
        "Random Groups": 0, "Length Rnd Gr": 9,
        "Each Word 2x": 0, "Max # of Words": 15
      },
      "variants": {
        "koch": { "InterWord Spc": 6, "Interchar Spc": 3 }   // overlay, not a copy
      }
    }
  ]
}
```

Before writing anything, the importer should check that every menu path resolves
on this device, every parameter exists on this firmware, every value is in
range, and (once the firmware can say) that every parameter is snapshot-storable
— then show the author or student exactly what it is about to do, in the manner
of the existing single-snapshot import preview.

---

## Where this stands

Answered: the Koch lesson is out of scope; the data exists and has been
extracted; the format should be a file, not code, and should generalise beyond
Graz.

Left to settle with CW Schule Graz: whether the variants ("wide" vs "Koch"
spacing) are the right model or an artefact of the current buttons; whether the
bundle should name its target slots or the tool should assign them; and what the
student should be left running afterwards — their own settings, or the first
lesson ready to go.

Left to settle here: whether to add `inSnapshot` to the protocol so bundles can
be validated, and whether to build `PUT snapshot/set` before or after the
importer.

## Related

- `Software/Utilities/m32_config_tool.html` — `snapImportConfirm()`, the
  single-snapshot import this would generalise, and `snapImportMenuNumber`,
  which already resolves a menu by name.
- `MorsePreferences.cpp` — `doWriteSnapshot()`, `storeSnapshotBlob()`,
  `decodeSnapshot()`, `applySnapshot()`, `storedInSnapshot()`.
- `Documentation/Protocol Description/M32 Protocol.md` — `GET snapshot/<n>`,
  `GET menus`, `PUT snapshot/store|recall|clear`.
- CLAUDE.md §4 — the NVS budget, against which any change to snapshot contents
  has to be costed.

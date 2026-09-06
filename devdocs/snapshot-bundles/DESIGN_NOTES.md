# Importing several snapshots at once — design notes

**Status: not built. Deferred 2026-09-06 by Willi**, pending a conversation with
CW Schule Graz about what a curriculum bundle actually has to carry. This note
exists so that conversation starts from what the firmware really does rather
than from what it looks like it does.

## What was asked for

CW Schule Graz have defined six snapshots for their curriculum, one per
practice. Today a student would import them one at a time through the
Configuration Tool's Snapshots tab (added in #214). The ask is to import a whole
set in one go — and, importantly, **a bundle entry need not define every
preference**: the ones it leaves out should keep whatever the student already
has.

That last part is reasonable and matches how the firmware already reads a
snapshot. It is also where all the difficulty is.

---

## Findings

### 1. A snapshot does not contain the Koch lesson

This is the finding most likely to decide whether the feature is worth building
in its current shape, so it comes first.

`doWriteSnapshot()` (`MorsePreferences.cpp`) stores exactly five things:

| stored | what it is |
|---|---|
| `vals[i]` for `i < posSerialOut` | the `pliste[]` values that `storedInSnapshot()` admits |
| `lastExec` | `menuPtr`, the menu entry the snapshot was taken from |
| `kochLen` | `kochCharsLength` — the **length of the character set**, derived from the Koch sequence or the custom chars |
| `useCustom` | whether Custom Chars is on |
| `customSet` | the custom character string |

`MorsePreferences::kochFilter` — the actual **Koch lesson number** — is not in
that list. It lives only in the `morserino` namespace (read at
`MorsePreferences.cpp:1790`, written at `:2089`) and `applySnapshot()` never
touches it. Recalling a snapshot therefore leaves the student's lesson exactly
where it was.

`posKochFilter` sits *after* `posSerialOut` in the `prefPos` enum — in the
"special cases" block — which is why the `i < posSerialOut` loop cannot see it.

**Consequence:** a per-lesson curriculum snapshot cannot set the lesson, which
is very likely the one thing CW Schule Graz most wants it to set. Worth
confirming with them before anything else is designed. If they do need it,
that is a firmware change (widen what a snapshot carries, with a blob version
bump — the format is already versioned, `SNAP_BLOB_VERSION`), and it should
probably be settled before the bundle format is, not after.

### 2. Partial snapshots are already supported by the format, but nothing writes one

`decodeSnapshot()` uses `255` to mean "not contained", and `applySnapshot()`
skips those entries, leaving the running value alone. So the *storage* format
has supported partial snapshots all along.

What has never existed is a way to *create* one: `doWriteSnapshot()` writes
every `storedInSnapshot()` parameter from the live configuration, so a snapshot
the device wrote is always complete. The partial case only arises from an older
firmware's snapshot that predates a parameter.

This means a hand-authored bundle can be partial, but a bundle **exported** from
a device never will be. If the Configuration Tool grows a bundle export, it will
produce full bundles, and partial ones will remain something a club authors by
hand or with a script. That is worth saying out loud in whatever documentation
the feature gets, because the asymmetry is surprising.

### 3. The accumulation trap

`PUT snapshot/store/<n>` snapshots the **live configuration**. There is no
command that writes a snapshot's contents directly. So importing N snapshots
means N rounds of *apply these values, then store*.

With partial entries that goes wrong in a way that is easy to miss. Suppose the
student has Word Length 4, and:

- entry 1 sets Word Length 3, says nothing about Interword Space
- entry 2 sets Interword Space 9, says nothing about Word Length

Applied naively in sequence, snapshot 2 comes out with Word Length **3** — the
value entry 1 left behind — not the student's own 4. Each entry inherits every
earlier entry's changes. "Undefined keeps the value as it is" quietly becomes
"as the previous lesson left it", which is not what anyone means.

The fix is not hard but has to be deliberate: read the student's configuration
first (`GET configs`), and before each entry restore any parameter that a
previous entry changed and this one does not define. Restore the full baseline
at the end, so the student's own settings survive the import.

### 4. Speed

Every `PUT config/<name>/<value>` calls `setParameter()`, which calls
`writePreferences("morserino")` — one NVS commit per parameter. That is fine for
the occasional single change and expensive in bulk:

| bundle | writes | rough time |
|---|---|---|
| 6 full snapshots (~40 params each) | ~240 | minutes |
| 6 partial snapshots (~8 params each) | ~48 | tens of seconds |

Over BLE, multiply by the `BLE_TIMEOUT_FACTOR` of 3 the iOS bridge already
applies. So partial bundles are not merely a convenience — they are what makes
the feature usable at all, which is another reason to settle §2 early.

---

## The option that removes most of this

A new protocol command — `PUT snapshot/set/<n>` carrying the values — would
write a snapshot without going near the live configuration. That kills the
accumulation trap (nothing accumulates), removes the NVS-commit-per-parameter
cost (one blob write per snapshot), and leaves the student's settings untouched.
It would also retroactively fix the wart in the single-snapshot import shipped
in #214, where importing one snapshot necessarily replaces what you are
currently using.

Costs: a firmware change, a two-variant build, protocol 1.5 and a documentation
round, plus a way to pass ~40 values in one command (the existing
`PUT file/data` base64 chunking is the obvious precedent). The Configuration
Tool would keep the apply-then-store path as a fallback for older firmware, or
simply refuse and say why.

Recommendation: if bulk import is built at all, build it on this. The
apply-then-store route is achievable but every part of it is a workaround for
not having this command.

---

## Questions for CW Schule Graz

1. **Does a lesson snapshot need to set the Koch lesson?** (See §1 — today it
   cannot.) If yes, that is the first thing to fix and it is a firmware change.
2. Which settings does a bundle entry actually define? A short list per lesson
   keeps the import fast; a full one makes each lesson self-contained but slow.
3. Should the bundle name its target slots, or should the tool assign them in
   order?
4. What should a student see afterwards — their own settings back, or the last
   lesson loaded and ready to run?
5. Do they want to author bundles themselves? If so the format needs to be
   pleasant to hand-edit, which argues for one file with a readable list rather
   than a concatenation of the existing single-snapshot exports.

## Related

- `Software/Utilities/m32_config_tool.html` — the single-snapshot export/import
  from #214, whose `snapImportConfirm()` is the code any bulk version would
  generalise.
- `Documentation/Protocol Description/M32 Protocol.md` — `GET snapshot/<n>`,
  `PUT snapshot/store|recall|clear`.
- `MorsePreferences.cpp` — `doWriteSnapshot()`, `storeSnapshotBlob()`,
  `decodeSnapshot()`, `applySnapshot()`, `storedInSnapshot()`.
- CLAUDE.md §4 for the NVS budget, which any change to snapshot contents has to
  be costed against.

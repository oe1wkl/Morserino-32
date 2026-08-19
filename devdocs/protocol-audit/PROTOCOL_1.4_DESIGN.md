# Protocol 1.4 — bulk preference read

*Draft for sign-off, 2026-08-17. Opens the 1.4 slot recorded in
[RESOLUTION_PLAN.md](RESOLUTION_PLAN.md); the two items already parked there
(game scores, `GET capabilities`) are siblings of this one and should ship in
the same bump.*

## The problem, with measurements

A client that wants to render the preferences must today issue **one command per
parameter**:

```
GET configs                 -> 48 names + values + displayed strings
GET config/<name>           -> ×48, for minimum/maximum/step/isMapped/mapped values
```

Measured on an M32 Pocket (Wroom), firmware 9.0, over BLE:

| | |
|---|---|
| `GET configs` (all 48 entries, one reply) | **0.12 s** |
| `GET hardware` (small reply, round-trip floor) | **0.06 s** |
| Config tool's Preferences tab, end to end | **~10 s** |
| The same tab over USB | ~2 s |

The transport is not the bottleneck: 0.12 s for the whole list means roughly
24 KB/s and a round-trip floor near two BLE connection intervals. The 10 s is
48 sequential round trips, plus the `await sleep(50)` the tool puts between them
(~2.4 s of the total — see [the separate note on that sleep](#the-50-ms-sleep)).

**What one bulk reply would cost.** Summing the actual `prefParam` table
(`MorsePreferences.cpp`, 48 entries) at the shape `jsonGetConfig` emits:

- **~10.3 KB** serialized in total, mean **218 bytes** per parameter
- largest single parameter **361 B** (`LICW Carousel`, 14 mapped values)
- descriptions account for **2.1 KB** of the 10.3 KB

At the measured 24 KB/s that is **~0.45 s of wire time** — against 7–9 s today.

## Options considered

**A. One monolithic reply.** `GET configs/details` returns all 48 objects in a
single JSON document.

*Rejected.* Two reasons, either sufficient. Heap: `const char*` members are
stored by pointer, not copied, so the cost is node overhead — roughly
48 × (8 members + ~10 array elements) × ~20 B ≈ **18–20 KB** of
`DynamicJsonDocument`, allocated while Bluedroid is up. Today's largest
comparable allocation is the 4096 B for `GET configs`. It would *probably* work
and would fail in the field when it did not. Recovery: a torn multi-KB reply is
documented protocol behaviour that the client answers by re-issuing the GET —
re-fetching 10.3 KB per tear is a bad recovery story over BLE.

**B. Monolithic but trimmed** — drop `description` (2.1 KB, and the config tool
only uses it as a fallback when `m32_pref_help.json` has no entry).

*Rejected.* Saves 20 % of the bytes and none of the heap risk, at the cost of
making a parameter's representation depend on which command you asked. Not worth
the divergence.

**C. Paginated, full fidelity — recommended.** The firmware chooses the page
size; the client follows a continuation marker. Bounded memory, bounded tear
cost, and each item is byte-for-byte what `GET config/<name>` already returns.

## Recommended design

```
GET configs/details             — first page
GET configs/details/<from>      — page starting at parameter index <from>
```

Reply, one page per command:

```json
{"configdetails":{
  "from":0, "count":8, "total":48, "more":true,
  "items":[
    {"name":"Keyer Mode","value":2,
     "description":"Iambic Modes, Non-squeeze mode, Straight Key mode",
     "minimum":1,"maximum":5,"step":1,"isMapped":true,
     "mapped values":["","Iambic A","Iambic B","Ultimatic","Non-Squeeze","Straight Key"]},
    …
  ]}}
```

Points that matter:

- **`items[]` entries are exactly the object `GET config/<name>` returns**, minus
  its `{"config":…}` wrapper. One truth about a parameter's representation; no
  client needs to know which fields live where.
- **The firmware picks `count`, not the client.** A page of 8 is ~1.7 KB
  serialized and ~3.5 KB of document — the same order as today's `GET configs`,
  so no new memory ground is broken. Six pages for 48 parameters. Because the
  client just follows `more` and `from + count`, the firmware can retune the page
  size later without breaking anybody.
- **`total` is the count on *this* device**, which varies by hardware variant and
  build. It is advisory (for progress display); `more` is the authority.
- **Ordering is `allOptions[]` order**, i.e. the on-device preferences-menu
  order, so a client can render top-to-bottom without sorting. Index `<from>` is
  an index into that same sequence.
- **Out-of-range `<from>`** returns the existing `INVALID PARAMETER` error rather
  than an empty page, so a desynchronised client fails loudly.

Expected end to end: 6 round trips ≈ **0.6–0.9 s over BLE**, ~0.2 s over USB.

## Firmware sketch

One new function beside `jsonGetConfig` in `MorseJSON.cpp`, plus a dispatch
entry in the `m32Get` `configs` branch:

- reuse the per-parameter emit loop of `jsonGetConfig` (extract its body into a
  helper that fills a `JsonObject`, rather than forking it — CLAUDE.md §5);
- `DynamicJsonDocument doc(4096)` for a page of 8, matching the existing
  `GET configs` sizing;
- walk `MorsePreferences::allOptions[]` from `<from>`, stopping at the page size
  or the end of the array;
- output through `m32out` under `protocolActive()` like everything else
  (CLAUDE.md §3.9) — `MorseJSON::jsonSend` already chunks at ~256 B.

No new preference, so no `prefPos` triple. No new on-screen text, so the
accessibility duty (CLAUDE.md §8) is satisfied by inspection — worth stating in
the commit message so the next reader does not have to re-derive it.

## Client side

The config tool's `loadPreferences()` replaces its 48-iteration loop with a page
loop. **It must keep the old path as a fallback**, because a 1.4 tool will be
pointed at 1.3 firmware for years:

1. try `GET configs/details`;
2. on `INVALID COMMAND` (or any error), fall back to the existing per-name loop.

This is exactly the case `GET capabilities` — the other parked 1.4 item — exists
to remove. If both ship together, a client can ask once instead of probing. That
argues for doing them in one release rather than two.

**The `await sleep(50)` at the head of that loop goes with it** (decided
2026-08-17). It is 48 × 50 ms = 2.4 s of pure padding: `sendAndParse` has
already awaited the complete reply, so the device has finished dispatching by
the time the sleep runs. It could be deleted today — no preference string in
`MorsePreferences.cpp` contains a brace, so the framing hazard below cannot fire
from a `GET config/<name>` reply — but there is no point doing it twice when the
loop itself is about to disappear.

> **Do not generalise that deletion to the other loops.** The identical
> `await sleep(50)` in the **CW memories** loop is load-bearing for as long as
> **C-BRACE** is open: `waitForResponse` counts raw braces, so a `}` inside a
> user-entered memory can frame a reply early, and the sleep is what lets the
> remainder land before the next `sendAndParse` clears `readBuffer`. Same for the
> `sleep(100)`s that follow a fire-and-forget `sendLine` — nothing is awaited
> there, so they genuinely pace the device.

The iOS app inherits the fix for free: it hosts the same tool.

## Versioning and duties

- Additive only; no existing command changes shape. Bump `M32P_VERSION` to
  **1.4** and add a 1.4 changelog block.
- `Documentation/Protocol Description/M32 Protocol.md` — new subsection under
  *Configuration (Parameters)*, plus the changelog. (The protocol description is
  the deliberate exception that lives under `Documentation/`, not `devdocs/` —
  CLAUDE.md §7.)
- User manuals: **not affected**, no user-visible device behaviour changes.

## Sign-off (2026-08-19) and what changed in implementation

Willi signed off on all three recommendations: **`GET configs/details`** as the
command name, **page size 8**, and **shipping alongside `GET capabilities`** —
plus `GET game/scores` (C6), so 1.4 carries all three parked items at once.

Two details of this design changed while it was being built:

**1. Ordering is `pliste[]`, not `allOptions[]`.** The draft above proposed the
preferences-menu order so a client could "render top-to-bottom without
sorting". Two findings killed that:

- `allOptions[]` is not a list of parameters. It also carries the action items
  — Call Sign, Op Name, Reset Scores, Practice Set — which have **no `pliste[]`
  entry at all** (CLAUDE.md §3.10). Paging over it would mean paging over a
  sequence whose members are not all emittable, and an index `<from>` into it
  would not mean what `GET configs` means by the same index.
- The rationale did not hold anyway: the config tool does **not** render in
  device order. It lays the parameters out in its own `PREF_GROUPS`, each an
  explicit list of names. No client we have wants menu order.

So `GET configs/details` walks `pliste[0..posSerialOut]` — **the same set, in
the same order, as `GET configs`** — and index `<from>` means the same
parameter in both commands. That is the stronger guarantee.

**2. The page size is a ceiling, not a promise.** Eight parameters is the cap,
but the loop also stops early if the document is within ~420 B (one worst-case
parameter) of its 4096 B capacity. ArduinoJson drops `add`s *silently* when its
pool is full — that is exactly how `GET menus` once lost its whole tail — so
the page is bounded by memory as well as by count. With 48 parameters the guard
never fires; it is there so that a future parameter table cannot turn a page
into a silently truncated one. Clients are unaffected: they were already told to
follow `count` and `more` rather than assume a fixed step.

## Result — measured on hardware, 2026-08-19

Firmware 9.0 on an M32 Pocket (Wroom), over USB. The build has **49**
parameters, not the 48 this document estimated (that count came from the
accessibility edition's table, which is one shorter — it unflags the Font Size
preference), so the full read is **7 pages**, the last carrying a single item.

| | |
|---|---|
| `GET capabilities` | 0.025 s |
| `GET configs` (49 names) | 0.069 s |
| `GET configs/details`, all 7 pages | **0.314 s** |
| 8 × `GET config/<name>` | 0.196 s → ~1.2 s for all 49 |
| `GET game/scores` (7 games) | 0.025 s |

So **3.8× fewer seconds of device time** over USB — and rather more than that
in the config tool, which additionally dropped 49 × 50 ms of `await sleep(50)`
along with the loop. The BLE figure was not measured.

Verified on the device: every page's `from` matches what was asked; `count`
matches `items.length`; the 49 items are the same set **and the same order** as
`GET configs`; and three probed items (first, middle, last) are byte-identical
to what `GET config/<name>` returns for them. An out-of-range page, a
non-numeric page and an unknown `configs/…` sub-command all return errors, and
the next valid page still works afterwards.

The firmware and client paging arithmetic were also checked against each other
off-device for table sizes 1, 2, 8, 9, 10, 48, 49 and 56 — including the
exact-multiple case, which must not emit a trailing empty page.

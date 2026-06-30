# QSO Bot — Improvement Plan

*Derived from `~/Documents/Privat/Claude/QSOBot_Analysis.md` (June 2026),
reconciled against the actual firmware in `MorseQsoBot.cpp/.h` + `qso_content.h`.*

> **Read this first.** The source analysis surveys five external QSO bots and
> ranks ten recommendations for the M32. But its feature matrix rates the M32
> built-in bot from the *outside* — and it is badly out of date. The current
> firmware is a descriptor-driven turn-taking state machine, not a keyword
> template bot, and it **already implements most of what the analysis
> recommends.** This plan separates "already done" from the genuine remaining
> gaps, then prioritises only the gaps.

---

## 1. What the firmware already does (correcting the analysis)

The analysis's §5 priority list is largely already satisfied. Before planning
new work, the record:

| Analysis recommendation | Status in firmware | Evidence |
|---|---|---|
| **P1 Session memory** | **Mostly done.** Per-QSO `QsoActors` (botCall/ref/name/qth/rig/ant/wx/age, userCall, **userName**), `gRstReceived`, `gField*`. | `MorseQsoBot.cpp:138`, `:282` |
| **P2 Progressive QSO sequencing** | **Done by design.** The descriptor *is* the sequence; the user can't skip ahead because the bot drives turn-by-turn. Standard QSO has explicit layers (RST+name+QTH → rig/ant/wx/age → 73). | `kStandardSteps` `:853` |
| **P3 `?` alone = repeat** | **Missing** (only `agn`/`rpt` repeat). | `isRepeatTrigger` `:566` |
| **P4 `<BT>` (`=`) as phrase boundary** | **Partial.** SLOT_INFO parser treats `=`/`es`/`bt` as field boundaries. Cross-token "later overrides earlier on same topic" *within one over* is NOT done for the simple slots (first match wins). | `parseInfoToken` `:454`, RT_EXPECT `:1123` |
| **P5 Formality mirroring** | **Missing.** | — |
| **P6 Address user by name** | **Done.** `fb dr [USERNAME]`, `tnx fb qso dr [USERNAME]`. | `:858`, `:862` |
| **P7 RST normalisation** | **Done.** `normalizeCutNumbers` (T/A/N→0/1/9), `5nn`→`599`, and the concat buffer reassembles split `5 9 9`→`599`. | `:363`, `:378`, `:1128` |
| **P8 Bot-initiated mode** | **Done.** The dynamic opening: user silent → bot calls CQ as activator/runner. | `STEP_WAIT_USER_CQ` `:656` |
| **P9 Graceful unrecognised input** | **Partial.** Recovery exists (`qrz?`, `agn agn`, `pse ur rst?`) but is fixed, not varied. | `processOver` `:749` |
| **P10 LLM / companion mode** | Out of on-device scope (firmware unchanged; needs a server). | — |
| Error correction (analysis 4.x) | **Done.** `eeee` and `<err>`/`<hh>` both retract. | `inputFeed` `:97`, `:113` |
| RST `5 9 9` split-token (analysis 4.8) | **Done** via concat. | `:1128` |
| Multi-word QTH/name (analysis 2.1 limitation) | **Done.** `appendDedup` accumulates multiple value tokens. | `:433` |
| Speed mirroring / QRS-QRQ (matrix) | **Partial.** Bot keys at the live keyer WpM (`wpm:0`), so it already matches the user's set speed. Keyed `qrs`/`qrq` is not parsed. | `startBotTx` `:584` |

**Net:** of the analysis's ten priorities, P1/P2/P6/P7/P8 are effectively done,
P4/P9 are partial, and only P3/P5/P10 are genuinely open. The most valuable
new work is therefore *not* on the analysis's top of the list.

---

## 2. Genuine remaining gaps + one the analysis missed

1. **Bare `?` = repeat last over** (analysis P3) — not implemented.
2. **Varied recovery prompts** (analysis P9) — currently fixed strings.
3. **Within-over correction: later value overrides earlier** (analysis P4) —
   for simple slots, first match wins; a corrected RST/serial later in the
   *same* over is ignored unless the user keys `eeee` first.
4. **Formality mirroring** (analysis P5) — bot always uses full
   `[USERCALL] de [BOTCALL]` framing.
5. **`qrs`/`qrq` recognition** — nudge keyer WpM from keyed text.
6. **No off-device test harness** *(analysis missed this; highest engineering
   value).* The matcher (slot grammars, cut numbers, concat, callsign shape,
   noise filter) is pure logic and has already produced two real bugs — the
   `maxPfxLen` callsign bug and the **"callsigns starting with K trigger a
   premature qrz?"** fix (commit `28c03f3`). It is currently only testable by
   keying on hardware. A host-compiled unit test would lock in regressions.
7. **LLM / WiFi companion mode** (analysis P10) — strategic, separate project.

---

## 3. Prioritisation (effort × impact)

Effort: S ≈ <½ day, M ≈ 1–2 days, L ≈ multi-day/project.
Impact is on *training realism* unless noted.

### Cross-cutting enabler — QSO difficulty / competence setting

> ✅ **DONE** (branch `qso-bot-improvements`). The `posQsoBotLevel` pref
> ("QSO Difficulty": Beginner / **Intermediate** / Advanced, default
> Intermediate) is wired through the three parallel arrays + `qsoBotOptions[]` +
> `allOptions`; manuals (EN+DE) updated. Read once at `run()` start into
> `gLevel`. Behaviours wired **now**: retry budget (±1, floor 1), patience
> timeouts (`overEndSilenceMs`/`noReplyMs`), recovery-prompt tone (gentle/curt
> pools), and Beginner RST spell-out (`5nn`→`599` at the single `startBotTx`
> choke point). Still reading-`gLevel`-later: speed mismatch (T2.2), formality
> (T2.3), repeat terseness. Existing devices upgrade cleanly: a missing NVS key
> falls back to the pliste default (Intermediate) and is written back.

Several items below should *not* behave the same for a first-week learner and a
seasoned op. Rather than gate each feature with its own ad-hoc flag, add **one
"QSO Difficulty" preference** that a number of bot behaviours read. This matches
the house design rule of a single selector pref over scattered toggles (cf.
[[ble-mutually-exclusive-single-selector]]) and gives the bot a coherent
personality at each level instead of unrelated knobs.

Proposed levels: **Beginner / Intermediate / Advanced** (3 is enough; a 4th
"Expert" only if a level proves too coarse). What scales:

| Behaviour | Beginner | Intermediate | Advanced | Status |
|---|---|---|---|---|
| Recovery patience / retries | generous, long timeouts | default | few retries, tight timeouts | ✅ wired |
| Unrecognised-input variety (T1.3) | clearer, calmer prompts | mixed | curt | ✅ wired |
| Bot's own cut numbers | spelled out (`599`) | cut (`5nn`) | cut (`5nn`) | ✅ wired (Beginner spell-out) |
| Speed mismatch (T2.2) | never | occasional, ±4 WpM | more frequent, ±4–8 WpM | ⬜ T2.2 |
| Repeat style | full, clear repeats | partial | terse (`agn`) | ⬜ later |
| Formality (T2.3) | always full preambles | mirrors the user | drops `... de ...` readily | ⬜ T2.3 |

**Mechanics / cost (M).** This is a new numeric `prefPos` (e.g.
`posQsoBotLevel`), a `uint8_t` selector — so it fits the standard
`pliste[]`/`prefName[]` path and needs the **three parallel arrays in sync**
(`prefPos` enum, `pliste[]`, `prefName[]`); a missing `prefName[]` entry is a
silent boot-time NVS panic, not a compile error (CLAUDE.md §3.9,
[[prefpos-parallel-arrays]]). Add it to `qsoBotOptions[]` so it shows only in
the QSO-bot preference set. Manual update (EN + DE) required — it's
user-visible.

**Sequencing note.** Worth landing *before or alongside* T2.2/T2.3, because
those features want a level to read. The behaviour table can start small (only
T2.2 reads it) and grow as the other items land — the pref itself is the
enabler.

### Tier 1 — do first (high impact-per-effort)

| # | Item | Effort | Impact | Notes |
|---|---|---|---|---|
| **T1.1** | **Off-device matcher unit tests** | M | High (engineering) | ✅ **DONE** (branch `qso-bot-improvements`). Pure primitives extracted to `MorseQsoBotMatch.h` (firmware calls them via thin context bridges); host harness in `Software/tests/qso_bot_matcher/` (`make run`, 84 checks). Covers cut numbers, RST, zone/serial, K-prefix callsigns + the bare-`K` noise regression (`28c03f3`), SLOT_INFO parsing. Both variants rebuild clean. Not user-visible → no manual update. |
| **T1.2** | **Bare `?` = repeat last over** | S | Medium, authentic | ✅ **DONE**. `?` added to `isRepeatTrigger` (header + test); full-over repeat via `gLastBotTx`. Manuals (EN+DE) updated. |
| **T1.3** | **Varied recovery prompts** | S | Medium (feel) | ✅ **DONE**. Randomised pools (`kCallRecovery`/`kGenRecovery`/`kRstRecovery`) via `pickPrompt`; all lowercase, short. Manuals (EN+DE) updated. Future: gate tone by the QSO Difficulty setting. |

### Tier 2 — worthwhile, more design

| # | Item | Effort | Impact | Notes |
|---|---|---|---|---|
| **T2.1** | **Within-over override** for simple slots | M | Medium | Let a later valid token replace an earlier match within the same over (currently first-match-wins). Aligns with analysis 4.3. Must not regress the deferred-turn logic that fixed talking-over-the-user. |
| **T2.2** | **`qrs`/`qrq` recognition + speed-mismatch provocation** | M | Medium | Parse keyed `qrs`/`qrq` to step the keyer WpM. **To make the feature actually used, the bot must sometimes create a reason to use it:** on some QSOs it sends at a WpM *moderately* different from the device's current setting — 4–8 WpM faster or slower (smaller is imperceptible; larger is just frustrating). Two trigger points: (a) when the bot *initiates* (calls CQ), and (b) when it *answers the user's CQ*. This requires the two parties to run at independent speeds. **Gated by the QSO Difficulty setting** (cross-cutting enabler above): off at Beginner so it never confuses a learner who hasn't met `qrs`/`qrq` yet. |
| **T2.3** | **Formality mirroring** (P5) | M | Medium | Track whether the user drops the `... de ...` preamble; conditionally strip the preamble from bot overs (except first/last). Templates are fixed strings, so needs a post-expand transform or template variants. |

> **T2.2 implementation dependency — independent bot speed.** Today the bot
> keys with `MorseCwEngine::PlayOpts.wpm = 0` (`startBotTx`,
> `MorseQsoBot.cpp:586`), i.e. it inherits the live keyer timings and therefore
> *always* matches the device's current WpM. The speed-mismatch provocation is
> only possible once the bot can be driven at its own explicit WpM. So T2.2
> really has two parts: (1) give the bot a per-QSO `botWpm` (stored in
> `QsoActors`, picked at `pickBotIdentity`) and pass it to `playStart` instead
> of `0`; (2) on the `qrs`/`qrq` request, converge `botWpm` toward the user's
> set speed. Keep the divergence bounded (±4–8 WpM) and clamp to the keyer's
> legal WpM range so the bot never produces an unreadable or sub-minimum speed.
> The encoder's live speed control still adjusts the *user's* sidetone/keyer
> speed independently, per UX conventions.

### Tier 3 — strategic / optional

| # | Item | Effort | Impact | Notes |
|---|---|---|---|---|
| **T3.1** | **Content breadth** | S–M (ongoing) | Low–Medium | Grow `qso_content.h` pools (names/QTHs/rigs), add more SOTA/POTA refs. Pure content; low risk. |
| **T3.4** | **Leading-digit callsign prefixes** | S | Low | *Found during T1.1.* `looksLikeCallsign` requires the digit in position 1–2, so `2E0xxx`/`3D2xxx`/`3Z…` etc. are not recognised as callsigns. Real but uncommon on a training partner. If changed, update the asserted-limitation test in `Software/tests/qso_bot_matcher/`. |
| **T3.2** | **More QSO descriptors** | M each | Medium | e.g. a deeper ragchew, or a "DX pileup-style" short exchange. The descriptor architecture makes this additive. |
| **T3.3** | **LLM / WiFi companion mode** (P10) | L (project) | High for advanced users | Firmware change is small (point MOPP/WiFi Transceiver at a server); the work is the server + safe-output constraints. Out of scope for the built-in bot; track as a separate initiative. |

### Explicitly *not* recommended

- **"Topic-flag dict / level-gate" rebuild (analysis P1/P2 as written).** The
  turn-taking descriptor already enforces sequencing; bolting on a free-form
  level-gate model would fight the architecture for no user-visible gain.
- **PROGMEM wrappers** on content tables (CLAUDE.md §5 — wrong for ESP32).

---

## 4. Definition of done for any item here

Per CLAUDE.md §8, each shipped item must:

1. Build for **both** variants (`pio run -e heltec_wifi_lora_32_V2` and
   `pio run -e pocketwroom`).
2. Respect the QSO-bot hard rules: stays on `morseQsoBot` state (never keys a
   real rig, §3.8), audio via `MorseOutput::pwmTone` (§3.1), trim the decoder's
   trailing space before matching (§3.3), don't break prosign handling (§3.4).
3. Conform to `devdocs/UX_CONVENTIONS.md` (speed/volume/exit/prefs grammar).
4. Update the EN **and** DE user manuals for any user-visible change (T1.2,
   T1.3, T2.x), or record an explicit TODO. T1.1 is internal → no manual.

## 5. Suggested order

1. **T1.1** (test harness) — *before* touching matcher behaviour, so T2.1 etc.
   land with regression cover.
2. **T1.2 + T1.3** — quick, high-feel wins.
3. **QSO Difficulty pref** (cross-cutting enabler) — land the pref + plumbing
   early; later items read it instead of inventing their own gates.
4. **T2.1**, then **T2.3**, then **T2.2** as appetite allows (T2.2/T2.3 read
   the difficulty setting).
5. **T3.x** opportunistically; T3.3 only as a deliberate separate project.

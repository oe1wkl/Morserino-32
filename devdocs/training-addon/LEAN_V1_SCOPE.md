# Training add-on — lean v1 scope

*Drafted 2026-10-29 from `RESEARCH.md` and a read of the firmware, the protocol document and the
Config Tool; **revised the same day** to make phrases a first-class item type after Willi's
steer (the OZ1THC Phrases Trainer and IW7DMH trainer were read in full — `RESEARCH.md` §1b).
**A proposal, not a decision:** every item marked **[Willi]** needs sign-off
(`ux-decisions-need-willi-signoff`). Line numbers are for master at this date.*

**The idea in one paragraph.** Add two small, device-timed events to the serial protocol (protocol 1.6),
then build **one** host-side trainer on top of them: a **Koch copy trainer** in which the *host*
chooses each item, the *M32 plays it* (`PUT cw/play`), and the answer is scored by the host. Items come
in two kinds — **groups/words** from the current Koch lesson, and **phrases** (2–8 words, CW-idiomatic,
filtered to the characters the learner knows). The answer is either **typed** on the computer keyboard
or **keyed on the paddle** (read back from the decoded stream, as the OZ1THC and IW7DMH tools do);
both feed the same scoring. The host measures timing, keeps a per-character mastery model and a
persistent history. Everything else in `RESEARCH.md` §3 is deferred.

---

## 1. Why this slice

| Gap (RESEARCH.md §3) | Covered by this slice |
|---|---|
| 3 typed-copy scoring | yes — the trainer |
| 4 response time | yes — host-measured for typed copy, device-measured for Echo |
| 6/7 mastery model, confusable pairs | yes — host-side, per character |
| 12 durable history | yes — local, exportable |
| 13 plan / curriculum | partly — Koch lesson suggestion and one-click advance; no calendar |
| 18 phrases as one unit | **yes — the least crowded ground** (§4b); the device's Echo Trainer cannot do it |
| 19 keyed-echo scoring | yes — as an answer source for the same engine (§4b) |
| 8 copy-behind, 9 real-callsign pools, 15 element timing, 10/11 sim | **deferred** (phrases in copy-behind form are partly covered: type after the phrase ends) |

Why **host-driven** rather than watching the device's own Generator (what tegmento does): a mastery model
needs to *choose* the next item. In device-driven Generator/Koch the device picks the words, so a host
can only observe. `PUT cw/play` already exists, works on both variants, and needs no flash or NVS.

## 2. What I verified in the code

- **`cw/play` is host-drivable but needs the Keyer.** `m32Put` accepts `play`/`repeat`/`recall` only in
  `morseKeyer`, `morseTrx` or `kipKeyer` and answers `CW/PLAY: Keyer not active` otherwise
  (`m32_v6.ino:5836–5861`). The host can enter the Keyer with `PUT menu/start/<n>`.
- **There is no "playback finished" signal.** Playback ends silently when `getM32PWord()` returns empty
  and `playCW` is cleared (`m32_v6.ino:2946–2951`). A host must guess the duration. *This is the one
  event the trainer cannot do without.*
- **Latent limit:** `playCWIndex`/`playCWMaxIndex` are `uint8_t` (`m32_v6.ino:151–152`), so a `cw/play`
  text over 255 characters is cut or wraps. Irrelevant for single items; worth a separate fix. *(To verify.)*
- **Echo Trainer has natural hook points for device-side timing:**
  prompt ends → `GET_ANSWER` (`:2751`); first paddle closure of the answer → `COMPLETE_ANSWER`
  (`:1885–1889`); word space detected → `EVAL_ANSWER` (`:1878`); no answer within *Echo Think Time* →
  `EVAL_ANSWER` with an empty response (`:1903`); the verdict is computed in `echoTrainerEval()`
  (`:3404–3447`), where `MorsePracticeStats::recordWord()` is already called.
  Today the verdict leaves the device only as the literal text "OK"/"ERR" in the raw character stream.
- **Pocket delivery trap.** On the Pocket, USB is HWCDC; bytes written to it can sit in the TX ring
  until the next traffic. `M32Tee::flushReply()` exists for command replies, and its comment states that
  asynchronous events are *deliberately not flushed* (`M32ProtocolOut.cpp:52–77`). For the new events the
  delivery time **is the measurement**, so they must flush, and the bench must prove it
  (see also the HWCDC withheld-replies note).
- **The Config Tool cannot host a streaming trainer without surgery.** `sendAndParse()` clears
  `readBuffer`, sends, and returns the **first** JSON object in the stream (`m32_config_tool.html:1077`);
  an unsolicited event arriving in that window would be dropped or taken for the reply. A trainer needs an
  object demultiplexer, not a bolt-on tab.
- **Publishing is by push.** Tools are single self-contained files published from `origin/master` by
  `sync-to-dropbox.sh`; saving does not publish. `m32_pref_help.json` and `targets.json` show that side
  files are possible, but I have not checked that a `.js` file is copied. *(To verify before M2.)*
- Protocol version is `M32P_VERSION "1.5"` (`morsedefs.h:66`); new commands/objects are additive.
- **The device has no phrase unit.** The Echo Trainer's prompt is "a word or group of characters"
  (manual, Echo Trainer); its sub-menus are Random, CW Abbrevs, English Words, Call Signs, Mixed,
  File Player and Practice Set. A phrase trainer has to be host-side, and the only device feature it
  needs is the `done` event (a phrase is far under the 255-character `cw/play` limit).
- **A keyed answer is already in the raw stream, but unlabelled.** *Serial Output* offers Nothing /
  Keyed / Decoded / Keyed+Decoded / Generated / All; origins are merged into one text stream
  (`m32_v6.ino:3171`, `:3210`, `:3227`). Text sounded by `cw/play` takes the generated path
  (`displayGeneratedMorse` → `SerialOutMorse(…, 0b100)`), so with *Keyed* the prompt does not appear
  in the stream and the answer arrives clean. *By code path only — bench to confirm.* The eight-dot
  error sign arrives as `<err>`; `*` is what the decoder emits for an undecodable sequence, and the
  older tools use it as their "delete" signal (`m32_v6.ino:3584`, `:3595`).
- **The QSO Bot already contains authentic QSO phrasing** as scripted lines with slots
  (`MorseQsoBot.cpp:840` ff., e.g. a bot over carrying report, name and QTH), and slot pools for names,
  QTHs, rigs, antennas, weather and ages in `qso_content.h`. That is a GPL, in-repo, already-approved
  seed for a CW-idiomatic phrase corpus (§4b).

## 3. Part A — protocol 1.6 events (firmware)

### Options

| | Event | Consumer in v1 | Cost | Verdict |
|---|---|---|---|---|
| E1 | `trainer`/`result` — one object per **evaluated Echo word**, device-timed | Echo log view; and tegmento / Morse Trainer Pro could adopt it | small (≈ 40 lines, 3 stamps + 1 emit) | **include** |
| E2 | `cw`/`done` — end of `cw/play` playback | the typed-copy trainer | tiny (≈ 10 lines at `:2948`) | **include** |
| E3 | `trainer`/`word` — a generator word has just been sounded | none (the host-driven trainer does not need it) | small | defer |
| E4 | element timing from the keyer (dit/dah/gap durations) for sending analysis | none yet | **large and risky** — it sits in the keying path (hard rule: never delay a dit) | defer; own design |
| E5 | **device-side phrase echo** — a multi-word unit in the Echo Trainer state machine (and manuals EN + DE, voice clips, preferences) | would make phrases work without a host | **large** — touches `echoStates`, the Echo preferences and the a11y duty | defer; the host-side phrase mode (§4b) comes first and tells us what the device version should do |

### Proposed wire format (draft — naming is open **[Willi]**)

```
{"cw":{"event":"done","reason":"end"}}                       reason: "end" | "stopped"
{"trainer":{"event":"result","mode":"echo","prompt":"cq","answer":"cq","ok":true,
            "repeat":0,"wpm":18,"koch":12,"latency":820,"duration":2100}}
```
- `latency` — ms from the end of the prompt's last element to the first paddle closure of the answer;
  `duration` — ms from that first closure to the word space that ends the answer. Both from the device's
  `millis()`, so host and link jitter do not enter. On think-time expiry: `"answer":""`, `"timeout":true`.
- Announced as `"trainer-events"` in `GET capabilities`; the `device` object's protocol becomes `1.6`.
- Emitted through `m32out`, gated by `protocolActive()` (hard rule 9), never from the keying path; each
  followed by a flush (Pocket trap above). Always on, like `control` and `menu` events — clients must already
  ignore unknown objects. *Alternative: opt-in `PUT trainer/events/on`.* **[Willi]**
- No new user-visible device text, so §8 (voice clips) owes nothing. Compiled on both variants.

### Work

Firmware (both variants) · protocol document (`Documentation/Protocol Description`) · additions to
`Software/tests/protocol/` (hardware-free selftest + bench steps, Pocket **and** classic) · changelog.
Estimate: **1–2 sessions plus bench**.

## 4. Part B — the host-side trainer

### Options considered

| | Trainer | For | Against |
|---|---|---|---|
| T1 | **Koch copy trainer, host-driven** — item kinds: groups, words, **phrases** (§4b); answer typed or keyed | the real gap; works on today's firmware (degraded without E2); both variants; any content later (callsigns) | overlaps tegmento's idea for groups/words, but not its mechanism; phrases and keyed echo are not covered by it |
| T1′ | Typed-copy, device-driven (observe the Generator) | reuses device Koch weights | host cannot schedule → no real mastery model; this is tegmento |
| T2 | Echo Coach — passive analytics from E1 | novel; trivial UI | passive; no scheduling |
| T3 | Sending-quality analyser | untouched niche | needs E4 |

**Recommendation: T1**, with T2 as a small second view that gives E1 a real consumer (an event nobody
reads goes untested).

### Form factor

| | Option | Verdict |
|---|---|---|
| A | New tab in the Config Tool | cheapest to start, but needs the demultiplexer inside a 3 700-line file that also feeds the iOS app — highest blast radius |
| **B** | **New page `m32_trainer.html`** sharing the Config Tool's conventions (Web Serial, `openTransport`-style seam, goodbye on `pagehide`) | **recommended** — isolated; the engine is testable in Node |
| C | Contribute upstream to tegmento | worth a conversation *in parallel*, not instead |

### Behaviour of the Copy Trainer v1 (draft defaults **[Willi]**)

1. **Connect** (Chrome/Edge, USB), read device info and capabilities, put the M32 into the CW Keyer.
2. **Items:** 1–5-character groups drawn from the current Koch lesson's characters (`GET kochlesson`),
   weighted toward weak characters (4b); and **phrases** (§4b), filtered to the same characters.
3. **Loop:** host sends `PUT cw/play/<item>`; on `cw`/`done` it starts the clock; the user answers (typed
   or keyed, §4b); host records *expected, answer, correct, first-input latency, total time* and the raw
   per-keystroke / per-character arrival times (so metrics can be redefined later without losing data);
   replay/feedback policy decided (4c).
4. **Speed:** character speed via `PUT control/speed`, Farnsworth via `PUT config/Interchar Spc` and
   `InterWord Spc`. Defaults adapted from Head Copy (MIT): fixed character speed, effective speed ±1 per
   session. *Whether config changes take effect immediately inside the Keyer is an open bench question.*
5. **Mastery model, per character (decision 4a):** Leitner box 1–5; promote on ≥ 3 attempts since the last
   box change and ≥ 90 % correct over the last 10; a miss drops two boxes; confusion pairs recorded.
   Latency is recorded and shown but is not a promotion gate (Head Copy's 550 ms is for single characters).
6. **Lesson advance:** when every character of the lesson reaches box ≥ 3 the page *offers* `PUT kochlesson/n+1`
   — one click, **never automatic** in v1.
7. **History:** IndexedDB in the browser, JSON export/import. No accounts, no cloud.
8. **Stats view:** per-character table (attempts, accuracy, median latency, box), confusion pairs, session log.
9. **Echo log:** a table of E1 results (prompt, answer, ok, latency, duration), also fed into the same store.
10. **Accessibility:** keyboard-only operation and `aria-live` feedback from day one (cheap now, expensive later);
    optional browser speech. *Not* part of the firmware voice duty in `CLAUDE.md` §8, but tegmento's README shows blind users ask for it.

## 4b. Phrases and the keyed answer (added after Willi's steer)

**Why phrases belong in v1.** Single words train recognition; phrases train what real QSOs need —
word boundaries at speed, short-term memory ("head copy"), QSO idiom, prosigns in context. The
research (`RESEARCH.md` §1b, §5) finds this the *least crowded* ground: the two tools that touch it
are send-direction only, exact-match, Windows-desktop, unlicensed or closed, last updated 2020–21,
with generic English sentences. The device has no phrase unit (§2). And it costs one device-side
feature we already plan (E2 `done`), because the host drives playback and the keyed answer is
already in the stream.

**How phrases fit (options).**

| | Option | Verdict |
|---|---|---|
| **P1** | **A phrase mode inside the same engine** — one scheduler, one scoring module, one store; phrases and groups feed the same per-character model | **recommended** |
| P2 | A separate "Phrases" page in the OZ1THC style | simplest to explain, but duplicates the link, scoring and store, and shares no mastery data |
| P3 | Device-side phrase echo (E5, §3) | later, and informed by P1; large firmware change |

**Corpus.** *Content is the hard part, not code* — and correct CW idiom is a product requirement
(CLAUDE.md §1), so Willi's review is part of the work.

| | Source | Verdict |
|---|---|---|
| C1 | **Our own CW-idiomatic set**, one JSON file (id, text, tags, word count; character set computed on load). Seeded from the QSO Bot's scripted lines and slot pools (GPL, in-repo), plus standard Q-codes and abbreviations in context, CQ calls, report/name/QTH/rig/weather exchanges, thanks and sign-offs, contest exchanges — written with prosigns. Start at ≈ 200–300 entries; plain-language sentences for later lessons | **recommended basis** |
| C2 | Template-generated variants (slots filled from the QSO Bot pools, real-style calls) for endless variety | cheap once C1 exists; the Koch filter must run *after* slot filling (calls contain digits) |
| C3 | **User-supplied files** — one phrase per line, file or paste, like IW7DMH's File / Cut-Paste modes and the M32 File Player; same filter and scoring | include in v1 — cheap, and clubs / CW schools bring their own texts |
| C4 | Third-party lists (OZ1THC's files, AD0WE's "ninja" texts) | no licence; plain English; **not copied**. Asking permission is a courtesy option **[Willi]** |
| — | German set | the manuals are EN + DE and German operators use German QSO wording; ship EN first and decide **[Willi]** whether DE is v1 |

**Tiers (decision 12a).**

| Tier | Lessons (M32 sequence) | Content | Authoring |
|---|---|---|---|
| A | from ≈ 10 | **Word chains**: 2–8 real words, drawn at random from the Koch-filtered word list and joined | none — a word list for the host, taken from the repo's own `english_words.h` |
| B | middle | **Plain-language phrases** built from the characters of those lessons | ≈ 100–150 phrases to write and review |
| C | ≈ 35 and up | **QSO lines** from the QSO Bot templates (17 unique fixed texts today), with slot-filled variants (C2) | extend the templates; Willi's review |

*Measured (2026-10-29, `english_words.h`, the Oxford-5000-based list with 4 881 distinct words):* the
number of words buildable from the first N characters of the M32 default Koch sequence is
**124 at N = 10, 229 at 12, 387 at 14, 961 at 16, 1 540 at 20, 2 651 at 32, 4 881 at 40**. Tier A is
therefore viable from about lesson 10–12, with a few hundred words — plenty for random chains. The
chains are *not grammatical*; they train word boundaries, memory and sustained copy, not sentence
sense. *Provenance:* the file's comment says "most of the oxford 5000" with weights from
hermitdave/FrequencyWords; the host copy inherits whatever attribution the firmware already carries,
to be checked before release (R10).

**The Koch filter, and its limit.** A phrase is offered only if every character is in the learner's
current lesson (spaces and the lesson's prosigns aside). Strict only, no lenient setting (decision 13a):
an unlearned character is uncopyable, so such results would have to be kept out of the mastery model.
User-file lines outside the set are skipped, with a visible count. *Expected limit:* in the first
dozen lessons almost no real phrase qualifies, so phrase mode is for the middle and later lessons;
below that the page says so and offers words and groups. I will compute the real coverage curve from
the draft corpus (phrases available per lesson) before we promise anything — this is risk R8.

*First measurement (2026-10-29, QSO Bot script, slots removed, M32 default Koch sequence
`mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAEB@:`):* the bot has 33 script lines, **17 unique
fixed texts**; the number sayable at lesson ≤ N is **2 at N = 10–20, 4 at 25–30, 7 at 35, 12 at 40, 17
at 45**. QSO idiom is character-hungry — `5nn`, `73`, `bk`, `tu`, `de` — so a QSO-only corpus makes
phrase mode a *late-lesson* feature (≈ lesson 35 and up). Early lessons need a different source of
multi-word items: see decision 12 (word chains and plain-language tiers).

**Ladder.** Phrase length — 2 → 8 words, as in OZ1THC's file set — is the difficulty axis. The
learner starts at a chosen length and the page *offers* a step up at ≥ 90 % word accuracy over the
last 10 items at that length, and a step down below 60 %; neither is automatic (decision 13b). Eligible
tiers are mixed by default; a selector pins one. Phrase speed and Farnsworth spacing use the same
controls as groups.

**Two answer sources, one scorer.**

| | Typed on the keyboard | Keyed on the paddle |
|---|---|---|
| Trains | recognition and copying; head copy when typed after the phrase ends | sending, memory, whole-phrase recall — the M32's Echo Trainer, for a whole phrase |
| Device setup | Keyer mode | Keyer mode with *Serial Output = Keyed*; the host reads it and warns — whether `PUT config` may set it per session is still to verify |
| End of answer | Enter | `=` / `<bt>`, an idle timeout, or Enter — **[Willi]** |
| Correction | Backspace | `<err>` or `*` (**decided, 11b**): the first deletes the last word, a second one right after restarts the whole answer |
| Timing | keystroke timestamps | character-arrival timestamps (USB jitter is milliseconds; a late byte on the Pocket only delays end detection, but R1 applies) |
| Feeds the mastery model | **yes** — perception errors, character confusions | **separate tallies** (send accuracy, recall): a keyed miss may be perception, memory or fingers and cannot be told apart, so it must not pollute the confusion model |

**Scoring a multi-word answer.** Case-insensitive, whitespace-collapsed, normalised to what the
device actually sounds (prosign notation defined once). Word-level alignment first (edit distance
over words), then character-level inside substituted words, giving per-word and per-character
results, phrase accuracy (words right ÷ words) and a "perfect" flag. Unlike the OZ1THC / IW7DMH
exact match this gives partial credit and feeds character confusions into the model. Pure functions,
tested in Node.

**Help.** *Replay* (counted; limited by a setting), and an optional *reveal first word / first
letter* (the idea from OZ1THC 2.0). An item answered with help is stored but excluded from
promotion.

**Scheduling.** A phrase is picked by the chosen length, by how many weak characters it contains,
and by not having been seen recently. Credit and blame go to the characters, not to the phrase —
there are no per-phrase boxes in v1.

**Cost.** About **+3 to 4 sessions** on top of the original estimate: corpus authoring and review
(1–2), phrase mode plus word-level scorer (1), keyed-answer source (1). The device side does not grow.

### Non-goals for v1 (the whole slice, §3–§4b)
Element timing / fist analysis · device-side `word` event · **device-side phrase echo (E5)** ·
real-callsign pools · QSO or contest simulation · a noise or QRM bed (OZ1THC mixes white noise on the
PC; ours would need host audio, which weakens the "device sounds it" identity) ·
accounts and cloud sync · iOS and Android · copy-behind as a growing delay (add later: it is only a
delay between `done` and the input window; typing after a phrase has ended works from day one) ·
a curriculum calendar / daily plan.

## 5. Build order

| Step | Content | Rough size |
|---|---|---|
| M0 | Decisions below; branch **`training-addon`** off `master` (CLAUDE.md branch workflow); tell Christof (OE6CHD) the wire format as a courtesy — he is the obvious first third-party consumer | — |
| M1 | Firmware E1 + E2, capability, protocol 1.6, docs, selftest, bench on Pocket **and** classic | 1–2 sessions + bench |
| M2 | Host link module: brace/quote-aware framer, command queue, event routing, reconnect, `pagehide` goodbye; Node tests | 1 session |
| M3 | Engine (scheduler, scoring, mastery, store) with Node tests; Copy Trainer UI | 2–3 sessions |
| M3b | **Phrases (§4b):** draft corpus C1 from the QSO Bot templates and **Willi's review**; coverage curve per Koch lesson; word-level scorer (Node tests); phrase mode, ladder, user files (C3) | 2–3 sessions + review time |
| M3c | **Keyed-answer source:** *Serial Output* check, stream framing (not JSON), end-of-answer rule, `<err>`/`*`; bench on both variants | 1 session + bench |
| M4 | Koch integration (`GET`/`PUT kochlesson`), speed/spacing control | 1 session |
| M5 | Echo log, stats view, help text, manual section **EN + DE**, FAQ line | 1–2 sessions |
| M6 | Bench with real learners (Willi; a CW-school group if available) | calendar time |

Total ≈ **10–14 sessions plus bench time** (7–10 before phrases were added) — the same order as the
WinKeyer work (5–7 sessions plus bench) in `TODO.md`, somewhat above it. These are estimates, not
commitments. **Phrases can be cut back to a v1.1** by stopping after M3 and M4 if you prefer a smaller
first release; the engine is built so that item kinds and answer sources are plug-ins.

## 6. Risks and how each is tested

| # | Risk | Test |
|---|---|---|
| R1 | Events delayed on the Pocket's HWCDC | Bench: timestamp each event on arrival, compare with a scope/audio capture; run 500 items; count late/merged events. Must pass **before** the host trusts `done` |
| R2 | `cw/play` stops when the user touches a paddle; the Keyer is also live while a learner types | Acceptable; `reason:"stopped"` is reported; the host replays the item |
| R3 | Config changes may not apply mid-session | Bench; fallback: set speed/spacing before each session, not each item |
| R4 | BLE refuses a session while a mode runs (`DEVICE BUSY`) | v1 is USB only; BLE flow later |
| R5 | A trainer page left open blocks auto-power-off (known; "heartbeat + lapse" deferred) | `pagehide` goodbye as in the Config Tool; the OM0RX fork's 10-minute idle expiry is a design data point |
| R6 | Characters the Keyer cannot play, pro-sign notation, case | Selftest enumerates the Koch set through `cw/play` |
| R7 | Host keyboard latency and OS timers | Irrelevant at the 550 ms scale; note in help |
| R8 | **Corpus coverage:** the Koch filter leaves few or no phrases in early lessons | Compute phrases-per-lesson from the draft corpus (M3b) *before* committing; the page says plainly when phrase mode is unavailable |
| R9 | **Keyed answer:** wrong *Serial Output* (prompt echoed into the answer, or nothing arrives); end-of-answer ambiguity; `<err>` vs `*`; merged origins | Host checks the setting and refuses with a clear message; end rule is configurable; bench with both variants and BLE refused for v1 (R4) |
| R10 | **Corpus provenance and CW correctness** (prosigns, abbreviations, QSO form) | Own text seeded from QSO Bot (GPL, in-repo); Willi reviews; no third-party lists copied (OZ1THC's carry no licence) |
| R11 | Raw stream is plain text interleaved with JSON | M2 framer must split them; selftest with injected noise and partial lines |
| R12 | **The filter must use the learner's real character set, not the default sequence.** My coverage numbers assume the M32 default sequence; LCWO order, a custom set or the Practice Set change them. Prosigns are one-character codes on the wire (`S` = `<as>`, `A` = `<ka>`, …) and the corpus must map to them | Read `GET kochlesson` (`value`, `characters`), `GET customchars` (when active) and the Practice Set; recompute coverage on connect; selftest per sequence. Check the known `GET kochlesson` truncation follow-up for long sequences (LICW BC2, PR #209) at M4 |
| R13 | **Where `cw`/`done` is emitted.** `playCW` is cleared in `fetchNewWord()` (`m32_v6.ino` ~:2972) when `getM32PWord()` finds the buffer empty; if that runs only after the final word's trailing space has been sounded, `done` arrives up to a word space (≈ 0.5 s at 18 WpM) late and biases every typed-copy latency | M1: measure on the bench against the sounded duration; emit at the last element's end, or carry the offset in the event; selftest asserts the bound |

## 7. Decisions for Willi

1. **Scope — DECIDED (Willi, 2026-10-29): the whole slice.** E1 + E2 + copy trainer (groups, words and
   phrases; typed and keyed answers; one scorer) + Echo log, in **one** protocol bump (1.6). Bundling E1
   with E2 avoids a second bump and a second bench round on both variants.
2. **Form factor — DECIDED (Willi, 2026-10-29): new page (B).** `Software/Utilities/m32_trainer.html`, engine in a
   separate module (Node-testable), linked from the Config Tool. Needs one added line per extra file in
   `~/sync-to-dropbox.sh` (explicit file list) — to be handed to Willi at publish time.
3. **Events — DECIDED (Willi, 2026-10-29): always on,** no opt-in command (the protocol already tells clients to
   ignore unknown objects; `cw`/`done` only follows a client's own `cw/play`; `trainer`/`result` is one small
   line per Echo word). Object names `cw` and `trainer`, with an `event` field as discriminator (room for
   the deferred `word` event); one capability, `trainer-events`.
4. **Mastery defaults — DECIDED (Willi, 2026-10-29).**
   **4a — promotion on accuracy, no latency gate.** Leitner boxes 1–5, every character starts in box 1;
   promote one box with ≥ 3 attempts since the last box change and ≥ 90 % correct over the last 10
   attempts; a miss drops two boxes (minimum 1). Each character in a group or phrase is one attempt (from
   the scorer's alignment). Latency is recorded (raw keystroke times kept) and shown, but is not a gate:
   `cw`/`done` fires after the whole group, so the first keystroke mixes recall and typing speed. Revisit
   with real data.
   **4b — groups 1–5 by box, weighted to weak characters.** Default "mixed": short groups while the
   average box is low, longer later; a fixed size can be pinned. Draw weight = 6 − box; the newest
   character of the lesson counts double until it reaches box 2.
   **4c — unscored replay after a miss.** The page shows the correct text and plays the item once more;
   that replay is not scored. The missed item returns after ≈ 4 other items and that attempt is scored
   normally. Replay *before* answering is help (§4b): counted, 1 per item by default, excluded from
   promotion.
   **4d — lesson advance offered, never automatic,** when every character of the lesson is in box ≥ 3;
   withdrawn if one drops back. The click does `PUT kochlesson/n+1` (changes the device's stored lesson
   — the button says so). No placement test: a learner at lesson 40 starts in box 1 everywhere
   (≈ 120 attempts, 10–15 minutes).
5. **Storage — DECIDED (Willi, 2026-10-29): local-only, no sync in v1.** IndexedDB, no accounts, no server;
   the attempt log (id, item, answer, per-character result, latency, timestamps) is the source of truth and
   box state is derived from it (so thresholds can be recomputed). One versioned JSON export; import merges
   by attempt id. The page requests persistent storage, shows the date of the last export and nudges after
   30 days or 20 sessions. Raw keystroke times are pruned after 180 days (summaries kept). One profile per
   browser. A single canonical URL (storage is per origin). File System Access auto-backup is a v1.1
   candidate; cloud accounts are out.
6. **Platforms — DECIDED (Willi, 2026-10-29): desktop browsers with Web Serial only,** the same set as the
   Config Tool (Chrome, Edge, Opera, Firefox 151+). Benched on Chrome and Edge; Firefox "untested" in the
   manual until someone benches it. No mobile in v1. The `openTransport()` seam is copied from the Config
   Tool and the engine has no DOM dependency, so a BLE host (iOS wrapper) can follow later. The page
   disables Connect without `navigator.serial`, as the Config Tool does.
7. **Naming and home — DECIDED (Willi, 2026-10-29).**
   **7a — name: "M32 Trainer"** (subtitle "Copy practice for your Morserino-32"); "Practice" and "Coach" are
   avoided (device terms: Practice Stats, Practice Set).
   **7b — files** in `Software/Utilities/`: `m32_trainer.html`, `m32_trainer_engine.js`,
   `m32_trainer_corpus.json`; published at the site root like the Config Tool (publish-script lines handed
   to Willi then). **Amended (Willi, 2026-11-01):** a fourth file, `m32_trainer_link.js` (the link module,
   M2), kept separate so it is tested alone and can be shared.
   **7c — links:** from the Config Tool header (hidden in the iOS wrapper — check how the wrapper handles
   `navigator.serial` first); from the "Using the Serial Output of the M32" section of the manuals EN + DE,
   plus the generated "What is new" and one FAQ line; the trainer links back to the Config Tool and to the
   right manual via `device.edition`. Not from the Installer; the morserino.info landing page is Willi's call.
   **7d — page UI English only in v1** (like the Config Tool); German practice text via user files.
8. **Collaboration — DECIDED (Willi, 2026-10-29).** Collaboration is neither a precondition nor a
   replacement: the build does not wait for replies. Willi writes every message himself; Claude drafts on
   request and sends nothing.
   **Christof Dallermassl, OE6CHD (tegmento):** first, before M1 starts — a direct message (not a public
   issue) with the draft wire format of §3, framed as a preview for any client, no obligation.
   **Oliver Bross, OM0RX (Morse Trainer Pro):** after protocol 1.6 is released, as a courtesy that
   `trainer-events` exist. The 8.2-OM0RX firmware-fork question (the "Update available" wording that
   offers 8.2 over 9.1; source availability) is a separate matter for Willi and is kept out of that message.
   **Tommy (OZ1THC) and Enzo (IW7DMH):** one courtesy note each at beta (M6): the trainer was inspired by
   their tools and the phrase corpus is our own; to Tommy also an open invitation to contribute phrases
   under an open licence (the only way his lists could come in, 12d). Enzo's old-release page thanks Willi
   for the Serial Output option.
   **Credits:** an About box names the adopted ideas — Head Copy (MIT) for the timing defaults, OZ1THC and
   IW7DMH for the keyed-phrase concept.
9. **Branch `training-addon` — DECIDED (Willi, 2026-10-29): go.** Created the same day as a permanent
   sibling worktree `~/Documents/GitHub/Morserino-32-training-addon` off master `3e8cc19`, nosync setup
   done, these docs committed as the first commit; nothing pushed.
10. **Phrases in v1** — **DECIDED (Willi, 2026-10-29): yes, option P1** (same engine, one scorer).
11. **Answer source** — **DECIDED (Willi, 2026-10-29): both**, typed and keyed, one scorer.
    **11a — DECIDED (Willi, 2026-10-29): end of a keyed answer** = idle timeout (default 4 s, adjustable
    2–10 s) with early finish once the target's word count has been keyed (1.5 s grace); Enter finishes at
    once; a keyed `=` or `<bt>` also finishes but is never required; ≈ 15 s without any input counts as
    no answer and the phrase is replayed (start timeout, my default — not yet confirmed).
    **11b — DECIDED (Willi, 2026-10-29): correction gesture** = the first `<err>` deletes the last keyed
    word; a second `<err>` right after it restarts the whole answer. Both `<err>` and `*` are accepted.
    Corrections do not change the final answer's score; their count is stored, and an answer that needed
    corrections does not count toward promotion.
12. **Corpus.** **12a — DECIDED (Willi, 2026-10-29): tiered.** Tier A: word chains (2–8 real words) from
    Koch-filtered words, for early lessons; Tier B: plain-language phrases for the middle; Tier C: QSO lines
    from the QSO Bot templates, for late lessons; plus user files (C3) on top.
    **12b — DECIDED: English first.** The corpus format carries a `lang` field; German users bring their
    own text through user files for now (no German word list or umlaut tier in v1).
    **12c — DECIDED: Willi reviews tiers B and C;** a second experienced CW operator looks over tier C
    before release (tier A is mechanical).
    **12d — DECIDED: OZ1THC's lists are left out.** A courtesy note to him stays under decision 8.
13. **Koch filter and ladder — DECIDED (Willi, 2026-10-29).**
    **13a — filter:** strict, no lenient setting. A phrase is offered only if every character is in the
    learner's active set (read from the device, R12); spaces are ignored. User-file lines outside the set
    are skipped, with a visible count ("37 of 120 lines usable at this lesson").
    **13b — ladder:** 2 → 8 words. A step up is *offered* at ≥ 90 % word accuracy over the last 10 items at
    that length; a step down is offered below 60 %; neither is ever automatic. Eligible tiers are mixed by
    default (a tier is eligible with ≥ 10 usable items at the current lesson and length); a selector pins a
    single tier. Tier A chains are generated at the exact length; authored items are filtered by word count.

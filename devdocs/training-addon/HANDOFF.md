# Handoff — M32 Trainer (the training add-on)

*Written 2026-10-29 at the end of the planning session. For a fresh session that will **build** the
trainer. Everything decided is in `LEAN_V1_SCOPE.md` §7 — this file tells you how to start, what to
read, what is waiting on whom, and which traps are already known.*

## 0. Status in one screen

| | |
|---|---|
| Phase | **M1 done: benched on Pocket + classic and committed 2026-10-31** on `training-addon` (not pushed; see "M1 — state" in §6, results in `BENCH_M1.md`). **M2 (link module) written, tested in Node and committed 2026-11-01** (not pushed) (see "M2 — state" in §6). **M3 (engine + Copy Trainer page) written, tested in Node and against a simulated M32, and committed 2026-11-01** (not pushed). **Benched 2026-11-01** on Pocket (Chrome, Firefox) and classic: all of §1–§4 pass after the fixes `2ad98fa`/`9c2a627`; prosigns added after that (`3b2d0e7`), version tag `54baaf4`. **Edge not run** (not available). **M3b (phrases) written and benched on the Pocket 2026-11-03**, including prosigns (§4.4/4.5) and VoiceOver (§5). |
| Decisions | **All 13 decided by Willi on 2026-10-29** (`LEAN_V1_SCOPE.md` §7); 9 (branch go) was answered "go" after the first version of this file was written. |
| Where the work is | Worktree `~/Documents/GitHub/Morserino-32-training-addon`, branch `training-addon`, created off master `3e8cc19` on 2026-10-29; `.pio` nosync done. These docs are its first commit. **Not pushed** — push only when Willi asks. The master checkout stays on master; never switch branches in either folder. |
| Gate for M1 | Passed: Willi confirmed on 2026-10-30 that the message to Christof is out. If Christof replies, compare his points with the M1 choices in §6 before the bench freezes the format. |
| Not yet done | M4 bench (`BENCH_M3.md` §8) and committing M4; Edge (§6 and §7 of `BENCH_M3.md` passed on both devices by 2026-11-05); a second operator's review of tier C before release (decision 12c); committing M3c; M4–M6. Protocol PDF rebuild at merge to master. |
| Size | ≈ 10–14 sessions plus bench time (estimate, not a commitment). Phrases can be cut to v1.1 after M4 if the schedule slips. |

## 1. First steps (do these in order)

1. Read §2 below, then `LEAN_V1_SCOPE.md` end to end (≈ 390 lines; §7 is authoritative).
2. **Already done (2026-10-29):** the permanent sibling worktree
   `~/Documents/GitHub/Morserino-32-training-addon` on branch `training-addon` off `master`, with
   `Software/src/.pio.nosync` and the `.pio` symlink in place (iCloud hazard, memory
   `icloud-documents-git-hazard`); the docs moved here and committed as the first commit. Work **in this
   worktree**; do not switch branches in the master checkout or in this one. A merge in a fresh worktree
   once failed with "stash failed"; a retry a few seconds later worked.
3. This session's folder access: if the session started elsewhere, request this worktree
   (`request_directory`) or have Willi start the session in it.
4. Ask Willi whether the Christof message is out. **M1 starts after "yes"** — not when Christof replies.
5. To bring master's later changes in, merge master into `training-addon` from this folder.

## 2. Read first

| What | Why |
|---|---|
| `CLAUDE.md` (all of it) | §3 rules 1, 2, 4, 9, 10; §4 NVS (the trainer stores nothing on the device); §6 UX; §7 docs EN + DE; §9 definition of done. |
| `devdocs/training-addon/LEAN_V1_SCOPE.md` | The spec. §3 firmware events, §4 trainer behaviour, §4b phrases and the keyed answer, §5 build order, §6 risks R1–R12, §7 decisions. |
| `devdocs/training-addon/RESEARCH.md` | Landscape only: §1b (OZ1THC / IW7DMH — how a keyed-phrase trainer behaves), §3 gaps. Skim the rest. |
| `Documentation/Protocol Description/M32 Protocol.md` | Syntax, versioning (additive; clients ignore unknown objects), `cw/play`, Koch lessons, Practice Set, Serial Output. |
| `Software/tests/protocol/README.md` + `m32_link.py` | The harness you extend for M1. Memory `m32-protocol-test-harness`. |
| `Software/Utilities/m32_config_tool.html` | The model for the page: `openTransport()` seam (~:1128), framer, `pagehide` goodbye, `device.edition` manual links, iOS WKWebView notes (~:963). |
| Memory (index loaded each session) | `m32-hwcdc-withheld-replies`, `protocol-session-blocks-sleep`, `device-aware-manual-links`, `utilities-autopublish-watcher`, `unified-web-installer`, `ux-decisions-need-willi-signoff`, `denied-commands-hand-back-to-willi`, `m32-build-commands`, `m32-blocking-loops-and-silent-pickers`, `ask-which-edition-before-flashing`, `wc-m-counts-bytes-in-sandbox-shell`. |

## 3. What is being built

A **host-driven Koch copy trainer** as a web page, plus two small serial-protocol events:

- **Firmware (protocol 1.6):** `{"cw":{"event":"done","reason":"end|stopped"}}` when a `PUT cw/play` has
  finished sounding or was stopped (E2), and `{"trainer":{"event":"result",…}}` once per evaluated Echo
  Trainer word with device-side `latency`/`duration` (E1). One capability string, `trainer-events`.
  Always on; no new command; no on-device text.
- **Page** `Software/Utilities/m32_trainer.html` (+ `m32_trainer_engine.js`, `m32_trainer_corpus.json`),
  titled **"M32 Trainer"**. The *host* chooses each item and has the M32 sound it with `PUT cw/play`; items are
  Koch groups, words and **phrases** (2–8 words); the answer is **typed** or **keyed on the paddle** (read
  back from the Serial Output stream); one scorer feeds a per-character Leitner model; history in
  IndexedDB with a mergeable JSON export; Echo log from E1; English UI only; desktop Web Serial browsers.

Layering that the tests depend on: **link module** (framer, command queue, event routing) → **engine**
(scheduler, scorer, mastery, store interface; no DOM, tested in Node) → **UI**.

## 4. Decisions — the short form (authoritative text: `LEAN_V1_SCOPE.md` §7)

| # | Decision |
|---|---|
| 1 | Whole slice: E1 + E2 + trainer + Echo log, one protocol bump (1.6). |
| 2 | New page `m32_trainer.html`; engine in a separate module; linked from the Config Tool. |
| 3 | Events always on; objects `cw`, `trainer`; `event` field; capability `trainer-events`. |
| 4 | Leitner 1–5, promote on ≥ 3 attempts since last change and ≥ 90 % of the last 10; miss = −2 boxes; **no latency gate**; groups 1–5 by box, weight 6 − box (newest ×2 until box 2); unscored replay after a miss, item returns after ≈ 4 others; lesson advance **offered** at all characters ≥ box 3, never automatic. |
| 5 | IndexedDB, attempt log = source of truth, merge-by-id export/import, `persist()` + export nudge, raw keystroke times pruned after 180 days, no sync. |
| 6 | Desktop Web Serial browsers; benched on Chrome + Edge; no mobile; keep the `openTransport()` seam. |
| 7 | Name "M32 Trainer"; four files in `Software/Utilities/` (the link module added 2026-11-01); links from Config Tool (hidden in the iOS wrapper), manuals EN + DE, What's-new, FAQ; English-only UI. |
| 8 | Willi writes all messages. Christof first (before M1); Oliver after 1.6 (fork question separate); Tommy + Enzo at beta. |
| 9 | Branch `training-addon` — **go**; worktree created 2026-10-29 (see §0). |
| 10–13 | Phrases in v1 (one engine); typed **and** keyed answers, one scorer; keyed end = idle 4 s (2–10 s) / early finish 1.5 s after the target's word count / Enter / `=` or `<bt>` never required; `<err>` or `*` first deletes the last word, second restarts the answer, corrected answers do not count toward promotion; tiered corpus A/B/C + user files; English first with a `lang` field; strict Koch filter; ladder 2–8 words with offers at ≥ 90 % / < 60 %. |

## 5. Working agreements and traps

- **Commit and push only when Willi asks.** The website publishes `origin/master` (memory
  `utilities-autopublish-watcher`): a push to master publishes; a feature branch does not.
- **User-facing choices need Willi's sign-off** (memory `ux-decisions-need-willi-signoff`): surface costed
  options, wait. Defaults in the scope marked "my default" are not decisions yet (e.g. the ≈ 15 s start
  timeout in 11a).
- **Denied commands:** if a tool call is blocked by `permissions.deny`, hand the command to Willi.
- **Ask which edition before flashing**; never infer standard vs Accessibility. Bench on **Pocket and
  classic**. You cannot bench for him — write the bench steps, he runs them.
- **Both variants build before anything is "done"** (`pio run -e heltec_wifi_lora_32_V2` and
  `-e pocketwroom`, from `Software/src/`). A pre-build hook removes iCloud "name 2.cpp" copies.
- Firmware hard rules that bite here: events go through `m32out`, gated by `protocolActive()`, never raw
  `Serial` (rule 9); **never anything in the keying path that can delay a dit** (a `jsonSend` inside
  `echoTrainerEval()` is fine, inside the paddle ISR path is not); the Pocket's HWCDC can withhold a reply
  until the next command (memory `m32-hwcdc-withheld-replies`) — **flush after each event** (`m32out.flushReply()`).
- The M32 Serial Output stream is a mix of origins and of JSON; the framer must split brace/quote-aware
  (R11). The decoder emits a trailing space (rule 3). Prosigns are single-character codes on the wire
  (`S`=`<as>`, `A`=`<ka>`, `N`=`<kn>`, `K`=`<sk>`, `E`=`<ve>`, `B`=`<bk>`, `H`=ch); rule 4 keeps them intact.
- The learner's real character set comes from `GET kochlesson` (`value`, `characters`), `GET customchars`
  and the Practice Set; the corpus filter must use it (R12). The default sequence is
  `mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAEB@:`.
- Do **not** copy OZ1THC's or AD0WE's text lists (no licence). tegmento is Apache-2.0 (adopt with
  attribution); Head Copy is MIT (timing defaults only).
- No new on-device text in M1, so the accessibility duty (§8) is owed nothing there; if that ever changes,
  re-run the voice extractor.
- Use Python for character counts (`wc -m` counts bytes in this shell).

## 6. Milestones

Each ends with: both variants build · tests pass · docs updated · Willi's bench where stated.

### M1 — firmware events (1–2 sessions + bench)

Branch work in `Software/src/Version 6 and newer/` (line numbers at master `3e8cc19`):

- **E2 `cw`/`done`.**
  - Natural end: `m32_v6.ino` ~:2972 (`else if (playCW)` in `fetchNewWord()`): `getM32PWord()` returns `""` →
    `playCW = false`. Emit `reason:"end"` here — **but first verify *when* this runs.** If it runs only after the
    final word's trailing space has been sounded, `done` arrives up to a word space (≈ 0.5 s at 18 WpM) after the last
    element and biases every typed-copy latency. Either emit at the last element's end, or carry the offset.
    This is risk **R13** (added to the scope).
  - Stops: `stopPlayCw()` (~:6064; called by `PUT cw/stop` and by a paddle touch at ~:1253) → `reason:"stopped"`.
    Decide and document the order relative to the `ok` reply.
  - `repeat`/`recall` loop until stopped: only `stopped` ever fires.
  - `playCW` is also set by the device's own memory playback (`preparePlay()`, ~:4582). Decide whether `done`
    fires for those too (harmless for clients; simplest is to emit for every `playCW` end) and document it.
- **E1 `trainer`/`result`.** `echoTrainerEval()` (~:3430). Stamps: end of the prompt's last element, first
  paddle closure of the answer, the word space that ends the answer (3 `millis()` stamps). Emit when the verdict is
  known (`EVAL_ANSWER` has `echoEvalCorrect`). Fields per the scope; think-time expiry → `answer:""`,
  `timeout:true`. The Pocket Practice Stats hook (`MorsePracticeStats::recordWord`) sits right there — keep both
  consistent. Check the Koch-learn branch (`KOCH_LEARN`/`KOCH_PREVIEW`, empty response) so it does not emit
  noise.
- **Capability** `trainer-events` in `MorseJSON::jsonCapabilities()` (`MorseJSON.cpp` ~:244; `StaticJsonDocument<384>` —
  check the size budget); `M32P_VERSION` → `"1.6"` (`morsedefs.h:66`).
- **Emit helper** in `MorseJSON` modelled on `jsonControl()` (~:428); gated by `protocolActive()`; flush.
- **Docs:** protocol description (new "unsolicited objects" entries; capabilities list; version history
  paragraph "Changes in protocol version 1.6 from 1.5" at the top); changelog under the current provisional
  version heading (check `morsedefs.h` version and `BETA`; follow memory `m32-master-now-v9`).
- **Tests:** extend `Software/tests/protocol/` — selftest fixtures for both events (captured from real
  devices once benched), bench steps for Pocket **and** classic; assert `cw/play` → `done` ordering and timing
  against the sounded duration.
- **Bench (Willi):** `cw/play` end/stop/paddle-interrupt; Echo Trainer correct/wrong/timeout; with Serial Output
  Nothing/Keyed/All; BLE Serial if the build has it (events reach both transports).
- **Open question to settle here:** does `PUT config` apply *Serial Output* per session, and do speed/spacing
  changes apply inside the Keyer immediately? (needed in M3/M3c — measure now, since you have the harness open.)

#### M1 — state (2026-10-31)

**Committed on `training-addon`** (on top of the master merge `4cce896`, which brought the HWCDC zero-length
packet fix, PR #226). Builds clean for `heltec_wifi_lora_32_V2` and `pocketwroom`; `selftest.py` passes.
**Benched on both variants** (standard editions), all runs pass. Results and notes are in `BENCH_M1.md`. What
the bench changed:
- **`koch` in Learn New Chr / Preview Char:** these modes leave `kochActive` off, so they now get the field by
  name in `echoTrainerEval()`.
- **Harness:** `m32_link` now reads `read(max(1, in_waiting))`; `read(4096)` with the 0.1 s timeout had
  stamped every arrival 0–100 ms late. The soak starts each item past the keyer's word space. The echo checks
  know the mode. The speed restore waits for the real reply and nudges once if it is missing.
- **Timing for M3:** `done` comes after the sounded length (counted from `ok`) by 0–80 ms on the Pocket and
  by 15–155 ms on the classic, more for longer prompts. The keyer keeps a word space after `done`. A volume
  turn stretches the sounding (~40 ms per step on the classic).

| File | Change |
|---|---|
| `MorseJSON.cpp/.h` | `jsonCwDone()`, `jsonTrainerResult()` (both flush at once); `trainer-events` in `jsonCapabilities()` |
| `m32_v6.ino` | `playCWDoneOwed` + `cwPlayDone()`; `playCWHasMore()` look-ahead so `done` `end` fires at the last element (R13) with a fallback in `fetchNewWord()`; `stopPlayCw(fromCommand)`; three Echo stamps (`echoPromptEndAt`, `echoFirstKeyAt`, `echoLastCharAt`) and the emit at `EVAL_ANSWER`; `repeatPlayCW` assigned, not only set; `setParameter()` calls `updateTimings()` for the two spacing prefs |
| `MorseMenu.cpp` | `menu_()` stops a running `cw/play` / memory |
| `morsedefs.h` | `M32P_VERSION` 1.6 |
| Protocol description | 1.6 change block, new section "Events for practice programs", capabilities, `cw/stop` note. **PDF not rebuilt** — rebuild at merge time (binary, conflicts with master) |
| `Software/README.md` | 10.0: two Bug Fixes + an *Other Changes* entry for 1.6 (kept out of the manual's What's-new on purpose). **At M5** update the New Features bullet that still says "version 1.5", together with the trainer's own bullet, then `sync_whatsnew.py` + German + manual rebuild |
| `Software/tests/protocol/` | `trainer_events.py` (validators + bench: timing, order, Serial Output, paddle, Echo, soak), selftest fixtures captured from both variants, `m32_link.py` exact-read fix, README section |

Protocol-level choices made in M1 (documented in the protocol description; none is user-facing on the device,
but tell Willi, and Christof if he asks):

1. `done` is owed only for a **client's own** `cw/play`/`repeat`/`recall` (decision 3's wording); a memory the
   operator plays on the device sends none — and if it replaces a client's text, that text gets `stopped`.
2. `PUT cw/stop`: `ok` first, then `done` `stopped`; with nothing playing, `ok` and no `done`.
3. **A paddle stop no longer sends an unrequested `ok`** (it did before 1.6; a command-queue client would take
   it for its own reply). The only behaviour removed — **Willi kept it (2026-10-31)**.
4. A `cw/play` over a running one replaces it and inherits the debt: one `done`, for the new text.
5. Leaving the Keyer (`menu_()`) stops playback and sends `stopped`; before, the buffer survived and played
   (garbled — `startFirst` was set) the next time a keyer mode started.
6. `done` `end` comes after the last character's display/Serial Output echo, so the stream order is causal.
   A trailing `\p` pause is not waited for.
7. `trainer`/`result`: prompt/answer in display form (`<sk>`, UTF-8), as `GET kochlesson` and the stream write
   them; `mode` is `echo` / `learn` / `preview`; `koch` in every Koch Trainer mode; on timeout `latency`/`duration`
   are omitted; `<err>` erases the whole answer, so `""` with timing means erased and nothing keyed after. Koch Learn/Preview repetitions without an answer emit nothing (as the device shows nothing).
   `duration` ends where the keyer registers the last character (one element space after it).
8. `repeat` = number of earlier soundings of the same word (0 first time), from the existing `repeats` counter.

### M2 — link module (1 session)
Brace/quote-aware framer, command queue (one in flight), event routing by object name, reconnect, `pagehide`
goodbye. Raw character stream vs JSON. Node tests with injected noise and partial lines. Copy the Config
Tool's `openTransport()` contract.

#### M2 — state (2026-11-01)

**Written and committed 2026-11-01 (not pushed).** `Software/Utilities/m32_trainer_link.js` (UMD: global `M32Link` in the page,
`require()` in Node) and `Software/tests/trainer/link.test.js` (49 tests, `node --test
Software/tests/trainer/*.test.js`, fake clock, no device; README beside it). No firmware change, so no build.

- **A fourth file.** Decision 7b lists three files; the link is a separate `m32_trainer_link.js` so it can be
  tested alone and reused (a later Config Tool could share it). **Willi agreed (2026-11-01)** — one more line
  in `~/sync-to-dropbox.sh` at publish time.
- **Framer:** brace/quote/escape-aware; text is handed on at once, objects when complete, each stamped with
  the arrival time (for `done` timing in M3). The protocol's stall rule (an object open with no byte for
  1.5 s is given up) **recovers what arrived behind a torn object** instead of dropping it: it resumes at the
  first `{` inside from which a whole object parses. Recovered items carry `late: true` — their time is not
  an arrival time. Unparsable (a stray `{` in boot text) and oversize (64 KB) objects are handled the same way.
- **Queue:** one command in flight; `expect` reply keys (defaults from the command: `GET_REPLY`, `ok` for a
  PUT, `control` for `PUT control`); `error` always counts as the reply. Unsolicited objects go to listeners
  by key (`cw`, `trainer`, `menu`, …); `'object'` sees everything with `meta.reply`.
- **Timeouts and recovery:** after a timeout the queue is held 250 ms so a late reply is swallowed, not taken
  for the next command; then the link re-handshakes once (`put device/protocol/on` is harmless mid-session,
  and a restarted M32 — the classic does that behind its USB bridge — ignores every command until it is
  handshaken). Event `resync` (re-read what you rely on) or `unresponsive` (queue rejected; `reconnect()`).
  Cable loss: everything settles, `lost`; with `transport.reopen()` (the Pocket re-enumerates; Web Serial
  `connect` event, same vendor/product id) a new session and `reconnected`.
- **`playCw(text)`** → `{started, done}`. `done` never rejects: `end` / `stopped` (from the device),
  `replaced` (a later play was accepted — the device owes one `done`, for that one), `refused`, `lost`.
- **Goodbye:** `addGoodbye(fn)` steps (the trainer will restore *Serial Output* there, open item 3) run on
  `disconnect()` and are written with `put device/protocol/off` in **one** write on `pagehide`.
- **BLE consent** (`CONFIRM ON DEVICE` → wait 25 s, event `confirm`) is in, as in the Config Tool, though v1
  is USB only.
- **Teeth:** each of eleven deliberate breaks (quotes, escapes, salvage, late-reply hold, re-handshake,
  `replaced`, event-vs-reply, framer reset at handshake, `late` flag, consent, pagehide steps) fails a test.
- **Not tested:** `openWebSerial()` (browser + device) — bench it with the page in M3: connect, unplug and
  replug a Pocket (`reconnected`), restart a classic (`resync`), close the tab (session ends, auto-power-off
  works again).

### M3 — engine + Copy Trainer UI (2–3 sessions)
Scheduler, scorer (word alignment then character alignment inside substituted words), Leitner model,
store interface (IndexedDB adapter + in-memory fake), stats view, keyboard-only operation and `aria-live`
feedback from day one. Pure functions, Node tests. Attempt log is the source of truth; box state derived.

#### M3 — state (2026-11-01)

**Written and committed 2026-11-01 (not pushed); bench owed.** Groups only, typed answers only (phrases are M3b, keyed answers
M3c, speed/spacing M4, Echo log M5).

| File | What |
|---|---|
| `Software/Utilities/m32_trainer_engine.js` | UMD, global `M32Engine`. Tokenizer (prosign = one token), scorer (word alignment by normalised character distance, then character alignment; one entry per expected character, extras counted apart), `Mastery` (Leitner, replayed from the log, confusions, median latency, `readyToAdvance`), `Scheduler` (weights 6 − box, newest ×2 below box 2, mixed size, retry after ≈ 4), `Session` (play → answer → score → log; unscored replay after a miss; help replay; paddle stop → play again, up to 3 times), `MemoryStore` / `IdbStore`, export/import, `lessonChars` |
| `Software/Utilities/m32_trainer.html` | The page: connect (refuses firmware without `trainer-events`), reads lesson + speed, puts the M32 in the CW Keyer (`GET menus` → number of "CW Keyer"), practice box, character table with boxes and confusions, lesson-advance offer (`PUT kochlesson`, button says it changes the device), history export/import with the 30-day / 20-session nudge, `persist()`, 180-day keystroke pruning. Keyboard only; `aria-live` status and feedback with a spelled-out version for screen readers |
| `Software/tests/trainer/engine.test.js` | 34 tests; 20 deliberate breaks each fail one or more |
| `Software/tests/trainer/fake_m32_browser.js` | A simulated M32 for the browser (paddle, leave the Keyer, unplug/replug, classic restart) |
| `devdocs/training-addon/BENCH_M3.md` | Willi's bench: Pocket + classic, Chrome + Edge |

Checked against the simulated M32 in the browser pane: connect → Keyer, right/wrong/help, paddle stop,
leaving the Keyer, unplug/replug (`reconnected`), classic restart (`resync` on the next command, ≈ 3 s),
lesson advance, keystroke capture, disconnect goodbye, no console errors.

**Prosigns over `cw/play` — they work; the earlier finding was wrong (corrected 2026-11-01).** The first
reading stopped at "`cleanUpText()` lower-cases the word". It lower-cases and *then* runs `utf8umlaut()`, which
turns `<sk>` (also `[sk]`, `\sk`, any case) into the upper-case code `K`; that passes the Koch filter, and
`generateCWword()` sounds it as SK. Only the bare one-byte code `K` is lost (it becomes the letter k), and
nobody sends that. Proved on the firmware's own code by `Software/tests/cw_play_text/` (host test, the
functions extracted from `m32_v6.ino`; a mutation without `utf8umlaut()` fails 12 of 18 checks). **No
firmware change.** The page now plays prosigns as part of the groups; the learner types them as the M32
shows them, `<sk>`, with `[sk]` and `\sk` also accepted and `<ar>`/`<bt>` read as `+`/`=` (Willi,
2026-11-01). `playable()` is gone.

**My choices, accepted by Willi on 2026-11-02:**
1. A helped item (replay before answering) moves **no** box, neither up nor down (the scope says "excluded
   from promotion"; I read that as "does not count").
2. Typing while the group sounds is allowed; Enter before the end is refused with a message.
3. Keys: Enter = start / check / next, Ctrl+Enter (Cmd+Enter on a Mac; both work everywhere) = replay, Esc = pause.
4. Mixed group size: the largest size is the rounded average box; the size is that or one less — so single
   characters until the average box reaches 1.5.
5. A paddle stop plays the group again (up to 3 times, then the learner may answer anyway); not counted as help.
6. Leaving the CW Keyer on the device pauses the page; Enter puts the M32 back into the Keyer.
7. On a miss the page shows the group with the wrong characters marked and plays it once more; Enter goes on
   (also during that replay).

### M3b — phrases (2–3 sessions + Willi's review)
Corpus format with `lang`; tier A (word chains from Koch-filtered words, generated at the exact length),
tier B (plain-language phrases), tier C (QSO lines from `MorseQsoBot.cpp` script ~:763–850 and `qso_content.h`);
coverage curve per lesson computed from the learner's real set; user files (C3) with the skipped-line count;
ladder 2–8. **Draft tiers B and C for Willi to review** before shipping; a second experienced CW operator
looks over tier C (12c). The host copy of the word list (`english_words.h`, 4,881 words) inherits the
firmware's attribution — check it (R10).

**M3b decisions (Willi, 2026-11-01):**
- **One selector** in the Copy practice box: *Practise* = Groups · Words · Phrases. With Phrases the size
  select becomes *Words per phrase* (2–8), and a second select pins a tier (Mixed / Word chains / Plain
  language / QSO lines / My file).
- **Words** is its own kind: single real words from the Koch-filtered list (tier A at length 1).
- **Corpus first:** format, tier C from the QSO Bot, a first tier B draft, and the per-lesson coverage
  curve, as a review document for Willi; the phrase mode is built while he reviews.
- **Call signs in tier C** come from a host copy of the firmware's generator and prefix table
  (`getRandomCall()`, `callsign_prefixes.h`).
- **Tier C = both** (Willi's idea): fragments of `Practice Files/RealQSOs.TXT` (G0ELZ, G3DVW (SK); Bill's
  permission covers the trainer, Willi 2026-11-01) **and** the slot templates. RealQSOs is too big to review
  line by line: Willi reviews a fixed random sample of 100, and bad fragments go into `realqsos_exclude.txt`.
- **Tommy OZ1THC's `phrases_2/3/4_words.txt` join tier B** — contributed to the Morserino repo in 2019, so
  decision 12d (his trainer's lists stay out) does not cover them. 18 sensitive ones are suggested for
  exclusion in `tier_b_exclude.txt` (Willi to confirm).

#### M3b — state (2026-11-01)
**Corpus reviewed and accepted by Willi (2026-11-01)**, including the suggested exclusions in `tier_b_exclude.txt`
and the RealQSOs sample. Still owed before release: the second operator's look at tier C (12c).
**Corpus: `devdocs/training-addon/CORPUS_REVIEW.md`** (generated). Sources in
`Software/tools/trainer-corpus/`: `tier_b_en.txt` (150 plain-language phrases, written band by band for the
M32 sequence), `tier_c_en.txt` (94 QSO lines with slots, seeded from the QSO Bot's overs cut at `=`, plus
`@pool`s for RST, power, temperature, CQ zone, serial), the two exclude files, `build_corpus.py` (validates,
reads Tommy's files and RealQSOs.TXT from `Practice Files/`, writes `Software/Utilities/m32_trainer_corpus.json`,
233 KB, and the review). Totals: 4,881 words; tier B 150 + 240 (Tommy) + 95 (Willi's list, `tier_b_willi_en.txt`, generated with
Google Gemini, added 2026-11-01); tier C 1,713 real fragments + 94 templates. Coverage, M32 sequence: tier B reaches 10 items at lesson 11, the real QSO fragments at lesson 16
(64 at 22, 1,127 at 38). The templates mostly need `de`, so they come at lesson 38 and later.
**Call sign generator ported (2026-11-01):** `randomCall()` in the engine, with the prefix table carried in the
corpus (`prefixes`, 973 rows from `callsign_prefixes.h`). Without a lesson filter it makes the firmware's
calls exactly: `Software/tests/callsign_gen/` compiles the real `getRandomCall()` on the host with the
engine's random generator, writes a fixture, and `callsign.test.js` compares (80 runs × 40 calls; three
deliberate breaks of the port each fail it). With the learner's characters it draws only from prefixes,
digits and letters already learned, or returns null. It defaults to common prefixes, every continent and any
length. The VK/ZL-only generator isn't ported. The coverage table now counts call signs exactly, and nothing
changed. **Firmware finding:** the prefix table has a row `"4U_I"` (weight 1), so with *Call Common* off the
device can generate a call like `4u_i5abc`, with an underscore. It's rare, but wrong; the fix is for master.

**Engine and page side written (2026-11-02), tested in Node and with the simulated M32; not benched.**
- Engine: `Corpus` (the strict filter per lesson and phrase length, tier counts, the eligible tiers with ≥ 10
  items, template filling with continent-matched names, QTHs and references, and your own lines as tier U);
  `Scheduler` with modes `groups`, `words` and `phrases`; `ladderOffer()`; `NoItemsError`. Attempts now carry
  `kind` (group|word|phrase) and `tier`. 95 Node tests (`phrases.test.js` is new); deliberate breaks of the
  length filter, the recent-item skip, the tier minimum, the continent match, the item weight and both ladder
  thresholds each fail a test. The ladder test found a real bug: 60 % summed as 59.99… (now counted in integers).
- Page: the Practise select (Groups · Words · Phrases); *Words per phrase* 2–8; *From* (Mixed / Word chains /
  Plain language / QSO lines / Your own phrases, each with its count at the lesson, empty ones disabled); a line
  saying what is usable; the ladder offer (like the lesson advance); a collapsible *Your own phrases* area
  (paste or load a `.txt`, kept in the browser, "3 of 5 lines have 2 to 8 words; 2 use only your lesson's
  characters"). Settings are remembered per browser.
- Robustness found on the way: the IndexedDB connection now closes on `versionchange` — an old trainer tab
  used to make a newer tab's upgrade (or a delete) wait for ever.
- Corpus clean-up found on the way: `<AR>2M0RTZ` (a call run into AR) is split instead of dropped, `<AR>VA`
  is `+ <sk>`; only 13 fragments with a prosign run into a word are still dropped.

**My M3b choices, accepted by Willi on 2026-11-02:**
1. Words are drawn leaning toward frequent ones (the list is in frequency order; r² on the position), as the
   firmware weights its Koch words — otherwise late lessons give `representation noise fulfil`.
2. An item's draw weight = 1 + the weakness of each distinct character (as for groups: 6 − box, the newest ×2
   below box 2); slots count as nothing.
3. The last 20 items are not drawn again while anything else is left.
4. *Mixed* picks each eligible tier equally often, whatever its size.
5. A missed item comes back after about 4 others (as for groups). **Changed by Willi on 2026-11-02, after the bench:** it only comes back while it fits the current settings (mode, group size, words per phrase, pinned kind), and waits otherwise.
6. A pinned tier is allowed below 10 items (it is the learner's choice); *Mixed* uses only tiers with ≥ 10.
7. Your own phrases: pasted or loaded, one per line, 2–8 words, `#` lines skipped, kept in this browser only.
8. The wording of the ladder offer and of the "nothing to practise" messages.

**Speech and Morse (Willi, 2026-11-02, after bench 5.1):** a screen reader must not talk over the sound. Everything
spoken goes through the page's `#say` live region: "Listen…" and "Still sounding" are shown, not spoken; a wrong
answer's result is shown at once and spoken after the replay; when activating Next or Replay moves the focus (the
reader then reads the field out), the sound starts 1.5 s later (`LEAD_IN_MS`, for a mouse click too), while Enter in
the field starts at once. No setting. `python3 -m http.server` can serve a cached page without asking: hard-reload
after a change, the version check only catches scripts that differ from the page.
The other three sequences are in the review. **Next:** port the call sign generator (needed for the real tier C
coverage, which is approximate for now), then the engine side: Words, word chains, phrases, the ladder, user
files (C3), the Practise selector. **Open for release (R10):** the word list's attribution — `english_words.h`
says "most of the oxford 5000", weights from hermitdave/FrequencyWords. Check both licences before the
host copy is published.

### M3c — keyed answer (1 session + bench)

#### M3c — state (2026-11-04)
**Written; tested in Node and the simulated M32. Benched 2026-11-05: §7 and `trainer_events.py` (46/46) pass on the Pocket and the classic.**
The firmware part (`protocol-1.6-practice`) is merged into master locally; the page work here is not committed yet.

- **Firmware:** `PUT cw/practice/<dits>|off` (capability `cw/practice`, still protocol 1.6, which is unreleased):
  session only, CW Keyer only: keyed characters only in the echo, the keyer's and the straight-key decoder's word gap,
  and no transmitter. Ends on protocol off, on any protocol on, on leaving the Keyer (`menu_()`; Willi, 2026-11-04, for a pulled cable), on restart; has no effect once a BLE session is gone. The page re-applies it on coming back. `practiceMode()` in `m32_v6.ino`.
- **Firmware, sounded spacing:** `PUT cw/spacing/<char>/<word>|off` (Willi, 2026-11-04: added now so the firmware merges once,
  before the next V10 beta). Same capability, same lifetime, effective only during practice. Applied where the sound is timed
  (`wordDoublerICS()`, `soundedWordSpace()`), not in `updateTimings()`, so it follows speed changes and cannot outlive
  the Keyer in the global timings. Interchar Spc does not affect how keyed characters are read, only the sound.
  The page sends the device's Interchar Spc and a word space of 7. **M4 uses this for Farnsworth instead of `PUT config`.**
- **Why not `Serial Output` and `InterWord Spc`:** both write NVS and need restoring, and InterWord Spc also sets the
  sounded word gap, which must stay as it is (Willi).
- **Finding:** Key ext TX defaults to *CW Keyer only*, and `cw/play` goes through `keyOut()` like the paddle, so until now the
  trainer's prompts keyed an attached transmitter. Practice mode is on for typed answers too, so this is fixed for both.
- **Engine:** `KeyedAnswer` (reads the echo; corrections; `check()` says what ended the answer), `Session.submit(.., {corrections,
  spacing})`, `Session.playAgain()`, `keyedTally`. Keyed attempts never reach the boxes or confusions (`Mastery.apply`).
- **Page:** Answer (Typed / Keyed on the paddle), Gaps in a word (Strict / Lenient), Finish after (2–10 s); the answer field is
  read-only and shows what arrives; a "Keyed" column in the character table; the page refuses firmware without `cw/practice`.

- **Second try (Willi, 2026-11-04, from bench 7.3):** a missed *keyed* answer, after its replay, can be keyed once more: same end rules, marked
  right or wrong, nothing kept (no log, tally or box), and the scored attempt still comes back after about 4 items. `Session.secondTry()`, state `second-try`.
  A paddle touch during that replay plays it again, up to 3 times. Enter with nothing keyed goes on; 15 s of nothing ends the offer, without a third playing. Typed answers are unchanged.
- **Next item (Willi, 2026-11-05, from the bench):** a keyed item goes on by itself, as the Echo Trainer does: 2 s after a right answer, 3 s after a
  second try. Setting "Next item: Automatically / Press Enter", keyed only, default Automatically, because the page cannot tell when a screen reader has
  finished. Page only (`scheduleNext()`). Enter skips the wait, Esc pauses, and it holds back while the focus is in a dropdown or text box. Typed answers wait for Enter.
- **M32 screen (Willi, 2026-11-05):** `PUT cw/display/off|on`, same capability and lifetime as practice mode, hides the sounded text from the screen; keyed text still shows. Page setting "M32 screen", default Hide.
- **Link fix (2026-11-05, from Willi's glitch after an error sign):** an `ok` and a `done` in one chunk (a paddle held as `cw/play` arrives) lost the `done`, because the play was registered in a `then()`, and the page stayed on "Listen…" with Enter refused. Registered synchronously now (`onReply`). Keying into the next item made this reachable; not proven to be what he saw, as he could not reproduce it.

**My M3c choices, signed off by Willi on 2026-11-05 (with the Hide default for "M32 screen"):**
1. Strict is the default; the switch is labelled "Gaps in a word: Strict (up to 6 dits) / Lenient (up to 12 dits)".
2. The trainer sets 6 dits for Strict whatever the M32's InterWord Spc says, and it refuses firmware without `cw/practice`.
3. "Finish after" is never shorter than a word gap + 1 s (at 8 WpM Lenient, 12 dits are 1.8 s).
4. Practice mode (no transmitter) also applies to typed answers.
5. The ladder offer looks at typed phrases only, so it does not appear while answering keyed.
6. After the pause for no input, Enter goes on with a **new** item, and the unanswered one is not logged.
7. The sounded word gap is 7 dits, whatever InterWord Spc says (Willi's "always 7"), through `PUT cw/spacing`. The character
   spaces stay the learner's Interchar Spc until M4. So a learner who set Interchar Spc 6 keeps it, and then gets a word
   space of 10, because a word space is never shorter than a character space + 4.
Serial Output check and warning, stream reader, end-of-answer rule (11a), corrections (11b), `<err>` vs `*`,
separate tallies (never the confusion model). Bench on both variants. Confirm by experiment whether
`cw/play` text appears in the stream with Serial Output = Keyed (by code path it should not).

### M4 — Koch integration (1 session)
`GET`/`PUT kochlesson`, `customchars`, Practice Set; recompute coverage on connect; speed control and spacing control
(`PUT cw/spacing`: session only, no NVS). No firmware change expected. The `GET kochlesson` truncation for long sequences
was already fixed by PR #209: check the page handles LICW BC2's 44 entries. Regenerate the corpus's `prefixes` from master's
rebuilt `callsign_prefixes.h`, and the `callsign_gen` fixture with it.

#### M4 — state (2026-11-06)
**Written; tested in Node (133 tests) and on the simulated M32 in the preview; not committed, not benched.** No firmware
change. Bench: `BENCH_M3.md` §8 (page version 2026-11-06).

- **Master merged in** (`cf427dc`): the branch now has master's firmware, docs and README (only the README conflicted; it
  was an older copy of the same 1.6 text, master's taken).
- **Call signs re-ported.** Master's rebuilt prefix table has a "common" flag per entry and a weight for each call-area
  digit (`pickAreaDigit()`); `getRandomCall()` now goes through `buildRandomPrefix()`. `randomCall()` in the engine follows it,
  and the corpus carries 1076 rows `[prefix, continent, zone, weight, common, ten hex digit weights]` (was 973 rows of four).
  `callsign_gen/extract.py` copies the new helpers; the fixture is rebuilt and the engine matches it exactly. With a lesson
  filter, a prefix is skipped when none of its digits in use is learned; the digit is drawn among the learned ones by the same
  weights. The coverage per lesson did not change.
- **Willi's choices (2026-11-06):** character speed = the M32's own (the page follows the knob through the unsolicited
  `control` object and never sends `PUT control/speed`); **effective speed set by hand only** (page setting "Effective speed",
  Off or 4–35 WpM, stored in the browser); **Farnsworth stretches both** character and word spaces; **the Practice Set is offered
  as a source** (setting "Characters: Koch lesson / Practice Set"; no lesson advance with it, and no character counts as the
  newest).
- `farnsworthSpacing(wpm, eff)`: the 19 dits of spacing in PARIS stretched to fit; the character space is rounded (3/19 of it),
  the word space takes the rest, so PARIS is off by half a dit at most; kept in the M32's ranges, and one dit less on the
  character space where the word space would hit its minimum (character space + 4). 20/12 → `cw/spacing/8/20`. Off, or not
  below the speed: the learner's Interchar Spc and 7, as in M3c. Sent on connect, on a change of the setting, and when the knob
  turns.
- The facts show "Koch lesson n of m (Custom Chars)" when custom characters are on (`GET customchars`), or the Practice Set's
  characters; the speed as "20 WpM, effective 12". Attempts carry `set` ('koch'|'practice'), `farnsworth` (the effective WpM
  in force), and `lesson` null for the Practice Set.
- `practiceChars()` turns `GET practicechars`' raw codes into the display form (S → `<as>` …, H → `ch`, as `GET kochlesson`
  shows them). **Known limit:** `ch` (custom set or Practice Set) is one entry in the set but sounds and reads as c + h; also
  before M4. Not fixed: rare, and a fix belongs in how the firmware shows it.
- LICW at lesson 44 (BC2) checked in a test: 44 single tokens, prosigns included.

#### §8 follow-ups (2026-11-06, page 2026-11-06c, uncommitted)

- **Stray `\` in Custom Chars — firmware bug, fixed on master (uncommitted):** `getWord()` returned the last
  word of `player.txt` uncleaned when no space/newline followed it; a file ending in `\sk` put `\` into the
  custom set. Changelog entry under V10 Bug Fixes. The page also drops any character outside `CWchars[]`
  (`E.hasMorse()`; `<err>` and `*` too, the M32 cannot sound them) and notes it on the lesson fact.
- **Koch Sequence and Lesson pickers (Willi: "Set the M32"):** with Characters = Koch lesson the page has a
  Koch sequence and a Lesson select. They write the M32 (`PUT config/Koch Sequence/<n>`, `PUT kochlesson/<n>`),
  as its menu does, then re-read the lesson. Custom Chars without a set: the M32 answers OK but stays on its
  sequence, and the page says so. LICW Carousel Start is not on the page (set it on the M32).

#### M4b — call signs and prefixes (Willi, 2026-11-06; to build after the §8 bench)
Willi asked for call sign and prefix practice outside Koch, with the M32's settings. **Decided (2026-11-06):**
- New kinds under "Practise": **Call signs** and **Prefixes**, from the engine's port of `getRandomCall()` /
  `getRandomPrefix()` (`buildRandomPrefix()`; the prefix = base + call-area digit, no suffix).
- **Settings on the page, seeded from the M32:** Length Calls, Calls Region, Call Prefixes, read with `GET config/...` on the
  first connect and then kept in the browser; changing them never touches the M32. Calls Region VK/ZL needs the VK/ZL
  generator ported too (not done in M3b).
- **Always all characters:** the Koch lesson / Practice Set setting does not apply to them.
- **Separate tally:** their own score (calls right, by length; most missed characters); the character boxes and the lesson
  advance are not touched (like keyed answers).
- Not decided yet (my defaults, to confirm at the bench): typed and keyed answers both work; a miss shows the call, replays it
  and brings it back later, as for groups.

### M5 — Echo log, stats, help, manuals (1–2 sessions)
Echo log from E1; help text; manual subsection under "Using the Serial Output of the M32" (EN + DE;
`Documentation/User Manual/Version 10.x/`, rebuild with `./build.sh`; memory `manual-build-automation`,
`manual-cross-refs-drift`); What's-new; FAQ line; Config Tool link (hidden in the iOS wrapper — check how
the wrapper treats `navigator.serial` first); publish-script lines for `~/sync-to-dropbox.sh`
**handed to Willi** (explicit file list; `m32_pref_help.json` is the precedent).

### M6 — bench with learners (calendar time)
Willi, plus a CW-school group if available. Then the courtesy notes to Tommy and Enzo (decision 8), and
the note to Oliver once 1.6 is released.

## 7. Open verification items (collected)

1. ~~When does the end-of-`cw/play` code run relative to the last element? (R13)~~ — a full inter-word space
   after it (`fetchNewWord()` runs once `genTimer` expires). M1 sends `done` at the last element instead;
   **benched**: see the M3 timing figures in §6.
2. ~~Is `cw/play` text absent from the stream with Serial Output = Keyed?~~ — yes, benched on both variants.
3. ~~May `PUT config` set Serial Output per session?~~ — **no**: every `PUT config` writes NVS. The trainer
   reads the setting on connect and restores it on goodbye/`pagehide`, and says so in the UI. Snapshots are
   untouched; no new NVS entries.
4. ~~Speed/spacing immediately?~~ — speed yes; spacing did not, **fixed in M1** (`updateTimings()` in
   `setParameter()`); benched on both variants.
5. ~~Pocket HWCDC event delivery under load (R1)~~ — soak 500/500 on both variants; 64·k-byte replies fixed by
   the zero-length packet (PR #226).
6. `cw/play` is limited to ≈ 255 characters; the longest phrase must fit with pauses.
7. Does the publish script need extra lines for the `.js` and `.json` files? (yes — hand them to Willi)
8. Attribution of the host copy of the word list (Oxford 5000 / FrequencyWords).
9. The ≈ 15 s start timeout in 11a is my default, not confirmed.
10. How does the iOS wrapper treat `navigator.serial`? (decides how the Config Tool link is hidden)

## 8. Data sketch (a starting point — the engine session may change it)

Attempt record: `id`, `t`, `device {edition, fw, protocol}`, `lesson`, `kind` (group|word|phrase), `tier`,
`source` (typed|keyed), `expected`, `answer`, `wpm`, `farnsworth`, `perWord[]`, `perChar[]` (expected,
got, ok), `latency`, `total`, `keystrokes[]` (pruned after 180 days), `corrections`, `replays`, `helped`.
Export: `{schema, exported, attempts[]}`, import merges by `id`, mastery recomputed by replay.
Keyed attempts feed separate tallies only.

## 9. People and messages

Willi writes and sends all messages; you draft on request and send nothing. Drafts so far:
`CHRISTOF_MESSAGE.md` (EN + DE). Later: Oliver (after 1.6, separate from the firmware-fork question),
Tommy and Enzo (at beta; invitation to Tommy to contribute phrases under an open licence).

## 10. Suggested opening prompt for the new session

> Work in `~/Documents/GitHub/Morserino-32-training-addon` (branch `training-addon`). Read
> `devdocs/training-addon/HANDOFF.md` and `LEAN_V1_SCOPE.md`, then follow HANDOFF §1. The M32 Trainer plan is
> decided (scope §7, all 13). The message to Christof is [sent / not yet sent]; start M1 once I confirm it is out.

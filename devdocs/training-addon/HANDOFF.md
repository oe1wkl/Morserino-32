# Handoff — M32 Trainer (the training add-on)

*Written 2026-10-29 at the end of the planning session. For a fresh session that will **build** the
trainer. Everything decided is in `LEAN_V1_SCOPE.md` §7 — this file tells you how to start, what to
read, what is waiting on whom, and which traps are already known.*

## 0. Status in one screen

| | |
|---|---|
| Phase | **M1 done: benched on Pocket + classic and committed 2026-10-31** on `training-addon` (not pushed; see "M1 — state" in §6, results in `BENCH_M1.md`). **M2 (link module) written and tested in Node 2026-11-01, not committed** (see "M2 — state" in §6). |
| Decisions | **All 13 decided by Willi on 2026-10-29** (`LEAN_V1_SCOPE.md` §7); 9 (branch go) was answered "go" after the first version of this file was written. |
| Where the work is | Worktree `~/Documents/GitHub/Morserino-32-training-addon`, branch `training-addon`, created off master `3e8cc19` on 2026-10-29; `.pio` nosync done. These docs are its first commit. **Not pushed** — push only when Willi asks. The master checkout stays on master; never switch branches in either folder. |
| Gate for M1 | Passed: Willi confirmed on 2026-10-30 that the message to Christof is out. If Christof replies, compare his points with the M1 choices in §6 before the bench freezes the format. |
| Not yet done | Corpus (tiers B and C need Willi's review); M3–M6; `openWebSerial()` on a real device (with the page, M3). Protocol PDF rebuild at merge to master. |
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

**Written, not committed.** `Software/Utilities/m32_trainer_link.js` (UMD: global `M32Link` in the page,
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

### M3b — phrases (2–3 sessions + Willi's review)
Corpus format with `lang`; tier A (word chains from Koch-filtered words, generated at the exact length),
tier B (plain-language phrases), tier C (QSO lines from `MorseQsoBot.cpp` script ~:763–850 and `qso_content.h`);
coverage curve per lesson computed from the learner's real set; user files (C3) with the skipped-line count;
ladder 2–8. **Draft tiers B and C for Willi to review** before shipping; a second experienced CW operator
looks over tier C (12c). The host copy of the word list (`english_words.h`, 4,881 words) inherits the
firmware's attribution — check it (R10).

### M3c — keyed answer (1 session + bench)
Serial Output check and warning, stream reader, end-of-answer rule (11a), corrections (11b), `<err>` vs `*`,
separate tallies (never the confusion model). Bench on both variants. Confirm by experiment whether
`cw/play` text appears in the stream with Serial Output = Keyed (by code path it should not).

### M4 — Koch integration (1 session)
`GET`/`PUT kochlesson`, `customchars`, Practice Set; recompute coverage on connect; speed/spacing control;
the `GET kochlesson` truncation follow-up for long sequences (LICW BC2, PR #209 — memory `m32-pr209-koch-lesson-filter`).

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

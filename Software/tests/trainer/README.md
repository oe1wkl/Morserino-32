# M32 Trainer tests

Host-side tests for the M32 Trainer page (`Software/Utilities/m32_trainer*`). They need
Node 18 or newer and no device:

```
node --test Software/tests/trainer/*.test.js
```

(Pass the files, not the folder: `node --test <folder>` treats a folder as a script.)

| File | What it covers |
|---|---|
| `link.test.js` | `m32_trainer_link.js`, the link to the Morserino: framing of the mixed JSON/echo stream (every split point, random chunking, noise, braces/quotes/escapes inside strings, torn and stray objects, the stall rule), the one-in-flight command queue, reply vs unsolicited routing, timeouts and the re-handshake, `cw/play` → `done` bookkeeping, cable loss and reopen, BLE consent, goodbye and `pagehide` |
| `engine.test.js` | `m32_trainer_engine.js`: tokenizer (prosigns, also `[sk]` `\sk`, `<ar>`=`+`), the scorer's word and character alignment, the Leitner rules (promotion window, miss drop, what moves no box), replay of the log in time order, confusions, lesson-advance offer, draw weights and group size, the retry after a miss, the session's play/answer/feedback states (paddle stop, help, lost link, pause), export/import merge, keystroke pruning; M4: the Practice Set reply, Farnsworth spacing, a set without a newest character, LICW at 44 characters |
| `phrases.test.js` | M3b: the corpus layer (strict filter per length, tier counts, template filling with continent-matched values), words and phrases from the scheduler, pinned tiers, recent items, retries, your own lines, the ladder offer |
| `keyed.test.js` | M3c: reading the echo (upper case, a prosign cut in two, word spaces), corrections, what ends an answer, Strict and Lenient, keyed attempts apart from the mastery model |
| `callsign.test.js` | `randomCall()`, the port of the firmware's `getRandomCall()`: the same calls as the firmware's own code for the same random numbers (fixture from `Software/tests/callsign_gen/`), call-area digits by the table's weights, and with a lesson filter only learned characters |
| `version.test.js` | page, link and engine carry one `VERSION`, and the page loads both scripts with `?v=` that version |

**On every change to the trainer, bump `VERSION`** in all three files: `m32_trainer_link.js`,
`m32_trainer_engine.js`, and in `m32_trainer.html` both the `?v=` tags and `PAGE_VERSION`. The format is a date, with a
letter for a second change on the same day (`2026-11-01b`). `version.test.js` fails if they disagree. The page refuses to
connect when a browser serves it with a cached script of another version.

Time is a fake clock (`fakeClock()` in the test file), so every timeout is exercised
exactly and the suite runs in well under a second. The device is a scripted transport
(`fakeM32()`); the objects it sends are the firmware's own, the `cw`/`trainer` events
captured on the M1 bench (`Software/tests/protocol/selftest.py`).

The link tests were checked by breaking the module on purpose (quote and escape handling, the
salvage after a stall, the late-reply hold, the re-handshake, `replaced`, event-vs-reply
matching, the framer reset at the handshake, the `late` flag, BLE consent, the `pagehide`
steps): each break makes at least one test fail. Add a test whenever a link bug is
found on the bench.

The engine tests were checked the same way, with 20 breaks (the Leitner thresholds, what moves a
box, log order, draw weights, the retry gap, word-cost normalisation, the stale-play guard, the
unscored replay, import merge, prosign handling): each makes at least one test fail.

What the M32 sounds for a group (prosigns included) is tested on the firmware's own code in
`Software/tests/cw_play_text/` (C++, `make run`).

Not covered here: `openWebSerial()` and the page itself, which need a browser and a device —
`devdocs/training-addon/BENCH_M3.md`. For trying the page without a device,
`fake_m32_browser.js` is a simulated M32 (paste it into the console before Connect; it can also
fake a paddle touch, leaving the Keyer, a cable pull and a classic's restart).

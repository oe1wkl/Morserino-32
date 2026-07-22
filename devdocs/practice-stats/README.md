# Practice Stats — Design Notes

**Status: v1.1 (2026-07-19).** M32 Pocket only, gated by `CONFIG_PRACTICE_STATS`
(set under `M32_Pocket_TFT_build_flags` in `platformio.ini`, so it covers every
Pocket/TFT env — Classic M32 does not get it). Two viewing surfaces, both
reading the same `/stats.jsonl`, not on the device's own screen (too small for
a meaningful trend view):
- WiFi-served web page (`MorseWiFi::viewStats()`, menu item **WiFi Functions
  → Practice Stats**).
- **Practice Stats** section of the **Koch** tab in the USB config tool
  (`Software/Utilities/m32_config_tool.html`), reading over the serial JSON
  protocol (`get stats/log`) — for users who mainly connect via USB rather
  than WiFi.

Tracks two things the user asked for: how long they've spent at each Koch
lesson, and their per-character error rate — both timestamped where possible.

## Data model

A **segment** is one contiguous span of practice time in a Koch-active mode
(`kochActive == true`, the same flag the existing adaptive-weighting code in
`Koch::increaseWordProbability`/`decreaseWordProbability` already uses) at a
fixed lesson (`MorsePreferences::kochFilter`). A segment starts when entering
a Koch-active mode (`MorseMenu.cpp`, the nine `kochActive = true;` sites) and
ends when leaving the menu again (the single `MorsePracticeStats::endSegment()`
call at the top of `MorseMenu::menuExec()`, right before `kochActive = false;`)
or when the device sleeps (`shutMeDown()` in `m32_v6.ino`).

Per-character attempt/error counts are accumulated into the open segment from
`echoTrainerEval()` (`m32_v6.ino`), via `MorsePracticeStats::recordWord()` — a
full index-aligned diff of `echoTrainerWord` vs. `echoResponse` (attempts++
per expected character, errors++ on mismatch or a too-short response).
Deliberately **not** reusing `Koch::getFailedCharIndex()`: that function
returns only the first mismatch (it feeds the adaptive-weighting bump), while
stats logging needs every character's outcome.

Koch *generator*-mode segments (`_kochGenRand` etc. — pure playback, no user
response) still open/close normally and contribute lesson time; they just
never get `recordWord()` calls, so their `chars` field stays empty. `Koch
Learn New Char` (`_kochLearn`) is not tracked — it doesn't set `kochActive`
either, consistent with it already being excluded from the adaptive-weighting
system.

## On-disk format

Append-only JSON-lines at `/stats.jsonl` on SPIFFS (`MorsePracticeStats::logPath`).
One record per segment:

```json
{"ts":1737262800,"m":842913,"dur":312,"lesson":12,"mode":"send","words":18,"ics":3,"iws":7,"wpm":20,"chars":{"V":[6,2],"K":[4,0]}}
```

- `ts` — Unix seconds at segment *start*, or `0` if the wall clock was never
  synced this boot (see below). Computed at flush time by subtracting the
  segment's elapsed `millis()` from `time(nullptr)`.
- `m` — raw `millis()` at flush time, always present regardless of whether
  `ts` is known. Exists solely so `backfillTimestamps()` (see "Retroactive
  backfill" below) can date this record later if a wall-clock sync arrives
  after it was written. Not shown in either UI.
- `dur` — segment duration in seconds.
- `lesson` — `kochFilter` value active during the segment.
- `mode` — `"listen"` (Koch Generator — playback only, no evaluation) or
  `"send"` (Koch Echo Trainer — response evaluated). Set explicitly at each
  of the nine `beginSegment()` call sites in `MorseMenu.cpp`, not derived.
- `words` — count of distinct words/groups presented during the segment,
  from `MorsePracticeStats::wordPresented()`, called once per fresh word in
  `fetchNewWord()`'s `randomGenerate:` block (`m32_v6.ino`) — the single
  fetch point shared by both generator and echo trainer, so it works for
  `listen` segments too (which never call `recordWord()`). A repeated word
  (echo trainer retry after a wrong answer) reuses `echoTrainerWord` without
  going through that block, so it is correctly *not* recounted. Guarded with
  a local `statsDiscardWord` flag (not `stopFlag`, see below): the
  `maxSequence` auto-stop logic generates one extra word on the call where it
  decides to stop (`wordCounter == limit+1`) that `checkStopFlag()` then
  discards before it's ever keyed or displayed (see its `lastWord` stash, only
  actually consumed by `generatorMode == PLAYER`) — the existing
  "N errs (M wds)" status line already accounts for this via `wordCounter - 2`;
  without excluding it, a "5 words" round logged as 6.

  First attempt at this exclusion gated on `!stopFlag` directly — wrong,
  and it shipped briefly: `stopFlag` is a shared variable with its own
  lifecycle (reset once per `loop()` iteration inside the `morseGenerator`/
  `echoTrainer` cases, not owned by this feature at all), so *any* reason it
  stayed `true` past that one call — a state-transition edge case, a paddle
  press landing on the wrong iteration, anything — silently zeroed out
  `wordPresented()` for the rest of that segment, not just the one phantom
  word. Reported as "Listen sessions showing 0 words despite clearly
  practicing." Fixed by setting a **local** `bool statsDiscardWord` only in
  the exact `wordCounter == limit+1` branch and checking that instead — same
  one-word exclusion, zero dependency on `stopFlag`'s broader behavior.
- `ics`, `iws`, `wpm` — `Interchar Spc` / `InterWord Spc` (both in dits) and
  the keying speed. Read **live at flush time** (`endSegment()`), not
  snapshotted at `beginSegment()`: WPM in particular is routinely adjusted via
  the encoder right after starting a session, and a start-of-segment snapshot
  logged the pre-adjustment value for the whole session (the change only
  showed up in the *next* session's record). Reading fresh at flush reflects
  the speed the session actually ended at.
- `chars` — optional; `{char: [attempts, errors]}` for every character
  actually drilled in that segment. Absent (not empty object) when the segment
  had no evaluated responses (always the case for `listen` segments).

The web page derives everything else client-side: characters-per-word
(`sum(chars[*][0]) / words`), WPS/CPS (`words / dur`, `totalChars / dur`),
and error rate (`sum(chars[*][1]) / totalChars`, blank for `listen` rows).

Near-empty segments (< 1s, no characters) are dropped rather than logged —
menu navigation churn shouldn't spam the file.

Aggregation (time per lesson, error rate per character, session list) happens
entirely client-side in the web page's JS, not on the device — keeps the
firmware side to "append records," consistent with the project's general
"avoid String churn / large buffers" guidance.

### Retention

Capped at 128 KB (`MorsePracticeStats::MAX_LOG_BYTES`) — small relative to the
1.5 MB SPIFFS partition (leaves room for `player.txt` and future growth), and
small enough that the trim pass never needs to hold more than one line in RAM.
When a pending append would exceed the cap, the oldest ~25% of records (by
byte offset) are dropped via a streaming line-by-line copy to `/stats.jsonl.tmp`,
then `SPIFFS.rename()` over the original.

## Serial protocol (M32 config tool)

Two commands on the existing `m32Get`/`m32Put` JSON-over-serial protocol
(`Documentation/Protocol Description/`), gated `CONFIG_PRACTICE_STATS`:

- `get stats/log` → `MorseJSON::jsonStatsLog()` → `{"stats":{"log":"<base64>","used":N,"total":N}}`.
  The log is **base64-encoded**, not escaped-and-brace-stripped the way
  `jsonFileText()` sends `/player.txt`: the config tool's serial reader
  (`waitForResponse()` in `m32_config_tool.html`) finds the end of a response
  by counting `{`/`}` in the raw stream, and the JSONL payload is itself full
  of `{`/`}` — `jsonFileText()` already strips braces from file content for
  exactly this reason, which would corrupt JSON records beyond recognition.
  Base64 sidesteps it (same idea as the existing chunked-upload path's
  `put file/data/<base64>`). Streamed straight to `Serial` in small chunks
  while reading the file — `jsonStatsLog()` never holds more than ~48 bytes
  of the log in RAM, matching `jsonFileText()`'s byte-at-a-time approach.
  Undefined `type == "stats"` (Classic M32, flag not built in) falls through
  to the existing `"GET stats: UNKNOWN COMMAND"` error path — the config
  tool's `loadPracticeStats()` treats a missing `data.stats` as "not
  available" and leaves the box hidden, no special-casing needed.
- `put stats/clear` → `MorsePracticeStats::clearLog()` → the config tool's
  Clear Log button.

The config tool (`Software/Utilities/m32_config_tool.html`) duplicates the
WiFi page's client-side aggregation logic (time-per-lesson, error rate,
derived WPS/CPS/chars-per-word) in its own `renderPracticeStats()` — two
independent renderers over the same JSONL format, not a shared module (they're
separate HTML documents with no build step tying them together).

## Wall-clock sync — why, and its real-world limits

The Morserino has no RTC chip. Three sync paths feed the same
`MorsePracticeStats::setWallClock(time_t)`/`settimeofday()`:

1. **NTP** (`MorsePracticeStats::tryNtpSync()`) — called from
   `MorseWiFi::viewStats()` right after `wifiConnect()` succeeds, only if
   `!hasWallClock()`. Best-effort: starts SNTP (`configTime(0, 0,
   "pool.ntp.org", "time.nist.gov")`, UTC — only the epoch matters, local time
   is never used) and polls for up to ~4s, then gives up silently. Only useful
   if the configured WiFi network (whichever `WiFi Select` slot is active) has
   real internet access; harmless no-op on an internet-less LAN or AP, where
   the browser sync below still covers it.
2. **Web UI** (`POST /api/time`) — the stats page posts `Date.now()` once on
   load, after (and regardless of) the NTP attempt above. `viewStats()` reuses
   `wifiConnect()` (the same station-mode connect as Upload File/Update
   Firmware), so the Morserino joins the user's configured WiFi network — it
   does *not* run its own AP the way `startAP()` (Config WiFi) does. The
   browser just needs to be on that same network; no internet access is
   required for this path either.
3. **Serial/USB** (`put time/set/<epoch>`, `m32_v6.ino`'s `m32Put()`) — the
   config tool's Koch tab sends this automatically (best-effort, errors
   ignored) each time its Practice Stats section loads, so USB-only users get
   dated sessions too without needing WiFi at all.

Using `settimeofday()`/`time()` (ESP-IDF's system time, backed by the
RTC-domain slow clock) rather than a hand-rolled `epoch - millis()` offset
matters: it keeps counting correctly across `esp_deep_sleep_start()` (the
firmware's "Power off" — see `shutMeDown()`), so **one sync survives any
number of subsequent FN-button sleep/wake cycles**, not just the current boot.

It does **not** survive the physical slide switch (a real battery disconnect,
confirmed in `Documentation/Hardware/Technical_Overview.md` §"Battery ON/OFF
switch") or a full battery drain — both zero the RTC domain outright. Since
the user manual explicitly recommends the slide switch for anything beyond a
few days of non-use (deep sleep alone still drains the battery over days),
**undated segments (`ts:0`) are an expected, routine occurrence**, not an edge
case — the web UI must display them gracefully (sorted last, labelled
"(undated)").

### Retroactive backfill

The common case — practice for a bit with no clock yet, *then* open Practice
Stats a minute later and get one of the syncs above — would otherwise leave
those just-completed sessions permanently undated, since `endSegment()` writes
`ts` once, at flush time. `MorsePracticeStats::backfillTimestamps()` fixes
this: every record also carries `"m"`, the raw `millis()` at flush time. The
first time a sync succeeds after having no wall clock (checked in both
`setWallClock()` and `tryNtpSync()`'s success path — `configTime()`'s SNTP
client calls `settimeofday()` directly, bypassing `setWallClock()`, so it
needs its own trigger), a rewrite pass walks the whole log: for any record
with `ts==0`, if the *current* `millis()` is still `>=` that record's `m`,
no reboot/deep-sleep has happened since it was written (`millis()` resets to
0 on wake, unlike the RTC-backed wall clock), so the gap is a reliable
elapsed-time measurement and `ts` gets computed and rewritten. Records that
fail that check (or predate this feature and have no `"m"` at all) are left
at `ts:0` — permanently undated, exactly as before.

## Known limitations (phase 1)

- Exiting a Koch-active mode via long-press back to the top-level menu screen
  doesn't itself flush the segment (only the *next* `menuExec()` call, or
  sleep, does) — a few seconds of menu-browsing time can end up attributed to
  the last-practiced lesson. Not corrected; considered acceptable imprecision.
- An abrupt power loss (battery pulled, not via the slide switch or FN sleep)
  loses at most the one currently-open segment. No crash-recovery/write-ahead
  log — deliberately out of scope.
- No on-device viewer; the OLED classic M32 doesn't get this feature at all
  (phase 1 is Pocket-only per product decision, not a hardware limitation of
  Classic — it also has WiFi and SPIFFS).

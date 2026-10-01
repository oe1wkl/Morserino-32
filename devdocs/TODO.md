# Morserino-32 — open items (loose-ends survey)

Compiled 2026-09-30 from all ~60 Claude Code sessions and the memory notes.
Section A was checked against the live repository the same day; sections B–H come
from session transcripts and memory notes, and items marked *(verify)* may already
be done.

Branches: `master` = V9.0.1 released, now provisional **9.1.0 BETA**. `m32kip` = the
**V10** line (10.0.0 BETA), pushed to `origin/m32kip`. When merging master into
`m32kip`, `morsedefs.h` will conflict on the version lines — **keep 10.0.0**.

---

## Working order (adopted 2026-09-30)

**Wave 1 — protect the work: DONE 2026-09-30.**
`m32kip` pushed · master merged into `m32kip` · master bumped to 9.1.0 BETA (decision:
the next master release is 9.1) · FAQ "First Steps" section committed and pushed
(EN + DE, Phillips screws, Configuration Tool bullet) · V9.0.1 confirmed released.

**Wave 2 — one batched bench session on the classic M32** (and the Pocket where noted).
Several debts need the classic/OLED unit, so clear them together:
- ~~BLE Serial scripted matrix on the classic V2~~ — **done 2026-09-30**: steps 1–10 and
  14 pass (factory reset skipped by decision); results in `devdocs/ble-serial/DESIGN.md`
  ("Third hardware run"), scripts in `devdocs/ble-serial/README.md`. Found and **fixed**
  a 3.7 KB heap leak per WiFi suspend/resume that rebooted the classic on the 12th trip
  (00ee378, on master, changelog `V. 9.1`). Still open from C1: steps 11–13 (see C1).
- Ultimatic on the OLED and with a mechanical paddle (C4).
- M32KIP Rig on the classic (already staged) + the Willi-only items of B6: WiFi pulled
  mid-mark, straight key and bug, 12 s tune, direct two-device soak.
- Separate slots, not part of the session: the two-hour drift test, the real-Internet run,
  and the ≥35 WpM soak (**needs another operator — ask someone now**).

**Wave 3 — V10 release path** (can run alongside wave 2):
1. Editorial passes on the changelog draft and the V10 manual (B1, B2) — only Willi can do this;
   it is the real critical path.
2. `PUT menu/start` error reply (B3) — small, one sitting.
3. Burst-loss §13.2(e) (B4) — **decision needed, see below**.
4. Keyer a11y voicing (B8); clear the TRX Peer fields (B7).

**Wave 4 — cheap goodwill, any spare 10 minutes:** replies to Rodjo and the pause/resume
claimant (F1, F2); quick cleanup (A6–A9: merged branches, `git branch -D scratch/umlaut-probe`,
inspect the three stashes, stale line in `RELEASE_AUTOMATION_DESIGN.md`).

**Wave 5 — after V10, each in its own session:** the I2S-sidetone and generated-CW 6 ms
fixes (C6, B5 — related); snapshot bundles, sidetone upstreaming, a11y roadmap, QSO Bot
leftovers (H1–H4); rewrite the stale `devdocs/HANDOFF.md` (H5); CI gates for the FAQ and
protocol PDFs (E2).

### Decisions still open
1. **V10 target date / event?** With none, wave 2 leads; with one, wave 3 leads and the
   burst-loss decision comes first.
2. **Burst loss (B4):** ship V10 with the documented workaround (Rig Delay ≥ 250 ms), or fix
   first? Recommendation: ship with the workaround (one loss pattern, documented, 250 ms
   floor rests on one test run).
3. **FAQ header** says "Answers reflect **firmware version 9.0**" — change to 9.0.1, or does
   9.0 mean "the 9.0 line"?
4. ~~Next master release is 9.1~~ (decided) — the `### CHANGES V. 9.1` section now exists,
   opened with the BLE heap-leak fix.

### Device state (end of 2026-09-30 session)
Both units on `m32kip` c22d8d8 (10.0 beta: BLE leak fix, pass-phrase entry, Rig exit fix, reset log),
Bluetooth Use 0 on the classic; the classic has Quick Start ON into Remote Rig (B14).
Pair re-staged and tested 2026-10-01 (B6); the classic has since left Remote Rig. TRX Peer
fields kept for now (B7); pass phrase see B11.

**Mac toolchain (2026-10-01):** Rosetta installed again, so PlatformIO's x86-only `mkspiffs`
(a11y voice image) works. Piper for voice clips: the repo's `.venv` is an old Intel build — a
working arm64 Piper 1.4.2 needs a venv on a SHORT path (espeak-ng truncates its data path at
~160 characters), e.g. `PIPER_BIN=/private/tmp/claude-501/pv/bin/piper ./generate_audio.sh`.
`xcode-select` points at the Command Line Tools, not Xcode 27 (`devicectl` missing on the
command line).

---

## A. Git / release housekeeping

1. ~~`m32kip` unpushed~~ — **done 2026-09-30**, `origin/m32kip` = 91ce169.
2. ~~Four FAQ files uncommitted~~ — **done**, committed and pushed to master as 8e0b9f1.
3. ~~`m32kip` lacks master's recovery-mode docs~~ — stale (they came in with the V9.0.1
   merge aa0188e); master is now fully merged into `m32kip`.
4. ~~`master` claims to be 9.0.1~~ — **done**, bumped to 9.1.0 BETA (5ff863c).
5. ~~V9.0.1 draft release~~ — **done**, released (confirmed by Willi 2026-09-30).
6. **Three stashes** (verified): `stash@{0}` and `{1}` are auto-stashes from branch
   checkouts on a detached HEAD, `stash@{2}` is a GitHub Desktop stash on `test1`.
   Contents not inspected — look before dropping.
7. ~~`Software/tests/kip/` untracked~~ — resolved, it is tracked on `m32kip`.
8. **`devdocs/RELEASE_AUTOMATION_DESIGN.md:162`** says the FAQ has no `build.sh` — stale.
9. **Branch graveyard** (verified 2026-09-30; "unmerged" means not an ancestor of master —
   squash-merged work also shows up that way, so check the PR before deleting):
   - Merged, safe to delete: `claude/cranky-bardeen-dcddba`, `pr-222`, `pr-223`.
   - Old diagnostic spikes (May–July, likely dead ends): `heap-diagnostics`, `-2`, `-3`,
     `nvs-diagnostics` (8 commits), `static-sprite-buffer`, `reboot-on-wifi-exit`, and the
     eight `warmup-*` branches.
   - `scratch/umlaut-probe` (2 commits) — needs `git branch -D`; not on master.
   - Probably squash-merged, confirm against PRs: `BLE_MIDI`, `ble-output-single-selector`
     (#189), `koch-preview-encoder-role` (#210/#211), `practice-stats-review-followups`,
     `pocket-erase-setup-screen` (also on origin), `review207`, `claude/optimistic-jones-457246`.
   - `a11y-speed-battery` (6 commits incl. a fixup, on origin) and `tmp-squash` (5 commits,
     checked out in worktree `.claude/worktrees/sad-kepler-c33841`) look like the same
     work before/after the history rewrite — decide which survives.
   - Local `protocol-1.4` is already gone.

## B. M32KIP / V10

1. ~~Editorial pass on the `### CHANGES V. 10.0` draft~~ — Willi: the entries are fine as they are (2026-10-01).
2. Editorial pass on the **V10 manual** (currently V9 text + M32KIP sections).
3. **`PUT menu/start` answers nothing** when the device is not at the menu (any mode, any
   client). Should return an error; optionally latch the request. Not a blocker — but on
   2026-09-30 it bit the BLE bench harness again (cycles that "lost" their notice); it has
   now posed as a firmware fault in two separate test campaigns. Worth doing soon.
4. **Burst-loss failure, test-plan §13.2(e):** a burst shortened one mark by 138 ms. Manual
   only carries the workaround (Rig Delay ≥ 250 ms).
5. **Generated CW is 6 ms short per element** on the key line (dah/dit 3.21) — to-do note only.
6. **Bench tests owed by Willi.** Done 2026-10-01 (two devices, no Mac in the path — see
   `devdocs/m32kip/TEST_REPORT.md`): direct keying, WiFi pulled mid-mark (key up within the
   1 s keepalive timeout, `End: timeout` after the 5 s session drop, per spec), straight key
   and bug, 12 s tune, the Rig's `End: <reason>` display. **Still owed:** real-Internet run,
   two-hour drift session (cannot be done hands-free), a long soak, and the Keyer's paddle
   text and black-knob memories by eye.
7. **Cleanup:** clear both **TRX Peer** fields — Willi: leave them for the time being
   (Pocket's points at the classic, 192.168.1.237).
8. **A11y voicing of the Keyer's messages** still owed.
9. **Pocket polled-Keyer 1.7 ms floor** — Willi's nod still owed.
11. ~~Setting the pass phrase~~ — **done 2026-10-01 on `m32kip`** (4af1827, 39c2d3f, 39b8cb7):
    preference **Pass Phrase** on the device (MorseTextEntry, write-only, voiced), a **Remote
    Keying** card on the Configuration Tool's WiFi tab, and the Config WiFi web form — all three
    with one rule, 12–32 characters from a-z 0-9 . , : - / = ? @ + (`kipPskValid()`; upper case
    would be voiced as prosigns in the a11y edition). Web form verified by Willi. `PUT kip/psk`
    keeps only its 12-character minimum (programmatic clients). Still to do: re-enter the
    intended pass phrase on both units (Claude had re-set the bench key, B11 history).
12. **Preferences heading for the five "Rig:" items** shows "Player & Scores:" — they fall through
    the heading chain in `displayKeyerPreferencesMenu()`. Cosmetic, one line.
13. ~~Leaving Remote Rig rebooted the device when BLE Serial was on~~ — **fixed 2026-10-01 on `m32kip`**
    (07b9425): the Rig left WiFi on (modem sleep off) when returning into the menu loop, and the BLE Serial
    restart then hit ESP-IDF's WiFi/BT coexistence abort. The Rig now switches its radio off on exit, like the
    games. Reproduced and verified on the classic.
14. **Quick Start into Remote Rig rebooted the classic 2–3 times** on one cold start (USB power, BLE Serial
    off), then worked; not reproducible on demand, and no core dump was written, so not a crash — a brown-out
    or a hardware watchdog. **`GET resets`** (c22d8d8, protocol 1.5) now records why each boot happened: when it
    recurs, read it before anything else.
10. Parked for after V10: first dit-*off* delay compensation, Rig-only device, auto-start
    into Remote Rig after power loss, WinKeyer protocol idea.

## C. Hardware verification debts

1. **BLE Serial (PR #194).** Classic V2 steps 1–10 and 14 **done 2026-09-30** (see wave 2).
   Still open: step 11 (suspended-session auto-sleep + remote self-disable), step 12
   (Bluetooth keyboard interplay), step 13 (WiFi upload/OTA after a BLE session, LoRa+BLE,
   30-minute keyer soak); ≥35 WpM stalled-client CW soak (needs a fast operator); a Pocket
   re-run under access control, including **measuring the leak fix on the Pocket** (only the
   classic was measured). Doc debt: `M32 Protocol.pdf` (hand-exported from MacDown) not
   regenerated.
2. **iOS app vs. a11y Pocket over BLE** — Files tab with a long `player.txt` (from the
   "Installer problems" session, commit 9c82391) *(verify pushed)*.
3. **Protocol 1.4:** `GET capabilities` and paginated `GET configs/details` **pass over BLE**
   (2026-09-30, classic). `PUT game/scores/clear` still never run against real scores.
4. **Ultimatic keyer:** never keyed on classic/OLED or with a mechanical paddle; touch-dropout
   spurious-element risk unchecked (mitigation if needed: minimum-open time).
5. **`pocketwroom-lora`** compiles, never bench-tested (shared-SPI question in its commit message).
6. **CW timing** (`devdocs/cw-timing-audit/FINDINGS.md`):
   - I2S sidetone ~6 ms heavier than the key line — fix proposed, ours to do; verify on a
     2-channel scope, not a microphone.
   - Re-measure after the `MorseCwEngine` gap fix (Fox Hunt / Pileup / Radio Cave) never confirmed.
7. **ClickButton latch fix:** only the CW Generator long-press exit is bench-confirmed;
   per-site `clicks = 0` cleanup pass still open.
8. **BLE consent — one unexplained admission.** On 2026-09-30 an unanswered BLE handshake was
   admitted after 9.5 s with nobody touching the classic; 7 later hands-off trials (5 from a
   hard reset) were all correctly declined after 20.2 s. Only an FN click can grant consent in
   the code, so either a spurious button event or something not yet understood. Re-run
   `devdocs/ble-serial/consent_trials.py` now and then; if it ever reproduces, look at the
   FN button's ClickButton state at prompt entry (relates to C7).
9. **Classic: BLE Serial with more than ~1000 WiFi trips per power-on.** After the leak fix
   37 B per cycle remain inside the library/Bluedroid. Harmless in practice; noted so a
   future heap investigation does not rediscover it.

## D. Small firmware follow-ups

1. **Snapshot recall** applies the theme without `setTheme` / font geometry / `writePreferences` — Willi's call.
2. **`audioLevelAdjust()`** can re-enter on a long-press exit; Morsel's internal `-1`
   transitions have the same latch issue.
3. **A11y gaps:** decoder char-by-char voicing; battery "3980 mV" readout (needs digit spelling).
4. **A11y voice store:** ~172 KB headroom (~14 clips). Compose numbers; never per-value clips;
   grep `buildfs` output for "full".
5. **A11y very-late freeze** (decoder-reuse leak) — dead end, unresolved.
6. **BLE transient splashes** ("BLE Ser. susp.", "BLE init fail") are silent in the a11y edition (§8 case 2).
7. **Protocol C15** (input-length cap) — last open protocol item.

## E. Docs and pipeline

1. EPUBs still carry the pre-recovery-mode text; Memory Chain HTML/PDF rebuild was open in earlier notes.
2. FAQ and protocol PDFs have **no CI freshness gate** (only the manuals do) — can drift silently.
3. V9.0-beta.2 shipped via a **partly manual** path; the next release is the first end-to-end
   test of the workflow timeout (30→60) and beta-asset changes.
4. `Software/iOS/M32Config/store-listing.md` had an uncommitted edit of Willi's as of 8/24 *(verify)*.
5. **Book repo:** decide §5.7 — drop the "baseline" snapshot 1, keep only snapshot 2, add one
   sentence pointing to Reset Defaults (chapter 6's snapshot 1 overwrites the baseline).
6. `m32p_assembly.odt` (Documentation/Assembly Instructions/M32Pocket) still says **TORX T8**
   for the case screws; Willi says production units use **Phillips** — the FAQ is corrected,
   the assembly document probably needs it too.

## F. People and community

1. Reply to **Rodjo** (Echo Trainer summary eating button presses) — drafted, unsent.
2. Reply to the **pause/resume claimant** — drafted; ask firmware version and hold-vs-tap.
3. PR #194 follow-ups (Andrew's leak note; minor).
4. cdaller/morserino32-trainer#9 (two bugs in the Graz snapshot set) — *(verify posted)*.

## G. iOS app

- Back in App Store review (build 1, resubmitted); next move is Apple's.
- Re-encoded demo videos are in `~/Documents/My Videos`; the 207 MB master screen recording
  is still in Downloads (Willi's call whether to keep).


## I. New requests and reports from testers (added 2026-10-01)

1. **A11y edition: Call Sign / Op Name entry is mostly silent — bug, quick fix.** `MorseTextEntry` voices the
   *raw* character, and these two fields use upper case, which the voice pack treats as prosign codes: checked
   against `voice_manifest.json`, **C D F G I J L M O P Q R T U V W X Y Z have no clip (silent)** and **A B E K N S
   are spoken as prosigns**; the space in Op Name has no clip either. Fix: for the call-sign and name character
   sets, voice the lower-case letter (and say "space"); Practice Set must keep voicing upper case as prosigns
   (there it IS a prosign code). Shipped bug in V9 → fix on `master` (9.1), merge into `m32kip`.
2. **BLE keyboard (vBand): link drops after ~30 min, possible stuck key-down — investigate.** Not reported by
   everybody. What the code does today (`MorseBluetooth.cpp`): on disconnect, `onDisconnect` waits a fixed
   `delay(5000)` *on the BLE event task*, then re-advertises — so reconnecting relies on the host coming back by
   itself; and nothing sends an "all keys released" report on (re)connect, so a disconnect between a key-down
   and its key-up report can leave vBand (in the browser) keying. Plan: send a release report on every connect
   and before/after any stop; find out why the link drops (host power management, supervision timeout,
   connection parameters — a long soak with a central that logs disconnect reasons); check whether the 5 s
   blocking delay on the event task delays or breaks the reconnect.
3. **Decoder: ITU-R M.1677-1 characters — decision needed.** The decoder already knows `. , : ? ' - / " = + @`
   (and `; !`, the prosigns, ä ö ü and ch). Against M.1677-1 only three are missing: **é** (`..-..`, decodes as
   `*` today), **`)`** (`-.--.-`, `*` today) and **`(`** (`-.--.`), which is **`<kn>`** in amateur practice — the
   conflict Willi pointed out. Options: decode `)` and é in any case (no conflict); keep `-.--.` as `<kn>` (ham
   practice) unless a new decoder preference selects ITU; further national characters (French à ç è, Spanish ñ,
   Scandinavian å æ ø, …) depend on the glyphs existing in the OLED and TFT fonts — to be checked per character
   (see the small-font umlaut episode). Willi: **Koch character sets stay unchanged** (established in schools).
4. **Minimum word length for the Echo Trainer (and the generator) — feature.** Today only **Length Words**
   exists (a maximum). A user wants a minimum too, to train long words. A new `prefPos` (three parallel arrays,
   CLAUDE.md §3 rule 10), word-source filtering for min ≤ length ≤ max (with a sane fallback when the filter
   leaves too few words for the current Koch lesson), snapshot inclusion, a11y clip(s) for the new label,
   EN+DE manuals. Possibly the same for Abbreviations and Calls for symmetry — Willi's call.

## H. Follow-up projects with their own handoffs

1. **Snapshot bundles:** `devdocs/snapshot-bundles/HANDOFF.md` (+ `DESIGN_NOTES.md`).
2. **Sidetone upstreaming to Hari:** `devdocs/sidetone-upstreaming/HANDOFF.md`.
3. **QSO Bot:** only optional items left (T3.2 descriptors, T3.3 LLM/WiFi companion). Parked.
4. **A11y roadmap:** blind-user feedback, high-contrast themes for partially sighted users, Phase 4 flash site.
5. **`devdocs/HANDOFF.md`** still says "current focus: Fight the Pileup P6" — stale; rewrite.

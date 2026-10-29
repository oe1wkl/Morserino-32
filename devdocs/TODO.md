# Morserino-32 — open items (loose-ends survey)

Compiled 2026-09-30 from all ~60 Claude Code sessions and the memory notes.
Section A was checked against the live repository the same day; sections B–H come
from session transcripts and memory notes, and items marked *(verify)* may already
be done.

Branches: **`master` = the V10 line (10.0.0 BETA) since 2026-10-22** — `m32kip` was merged into it (4011dee), with
V9.2 folded into 10.0 (9.2 never ships on its own). `m32kip` is retired. **`V9.1` released 2026-10-11** (published,
in the web installer, binaries archived); a V9 hotfix, if ever needed, goes on a branch from the `V9.1` tag.

## ▶ Where we are — start here (2026-10-22)

**Next: tag `V10.0-beta.1` on master** (Willi). Prepared 2026-10-22: master merged into m32kip and m32kip into
master; README 10.0 complete; release.yml no longer strips the manual freshness stamp; every CI check, all 10
envs, the a11y filesystem image, host tests and the protocol selftest pass. Then the outstanding bench tests run
from the beta (BENCH_REMOTE.md, winkeyer/BENCH.md, B6 below).

**Release plan (Willi), as of 2026-10-11:** 9.1 is out; next is the **first V10 beta** (m32kip).

1. ~~**V9.1 stable**~~ — **released 2026-10-11** (tag `V9.1`, draft published the same day; installer shows 9.1
   for classic, Pocket and the Accessibility Edition). It is V9.1-beta.1 plus one firmware change, the I2S sidetone
   alignment fix (C6), shipped **without its scope test** by Willi's decision — the test is still owed (C6 below).
   iOS app 1.1 (build 2) was approved and is in the App Store.
2. ~~**Next master change**~~ — **merged + pushed 2026-10-14** (f5c853b): master is **9.2 beta** (Willi: "9.2
   for the time being — we might soon jump to a 10.0 beta with m32kip included"). The a11y speed/battery speech
   (rebased from `origin/a11y-speed-battery` as `a11y-speed-battery-v91`), plus "keying cuts an announcement"
   added after the bench test; all bench-tested by Willi on an a11y Pocket. Ext-Tx keying during an
   announcement was not bench-tested. The **QSO Bot** gap (speed changes silent there) is fixed too, merged
   2026-10-14 (`qsobot-speed-voice`, bench-tested): voice tick in the bot loop, keying cuts speech, bot CW
   pauses while a clip plays, and battery line + menu path after leaving the bot.
3. **Then V10:** ~~merge master into `m32kip` once more~~ — **done 2026-10-14** (433b03a, master up to cc8230d:
   9.1 release, 9.2 a11y speed/battery speech; V9 manual edits ported to the V10 manual; voice store regenerated,
   SPIFFS headroom now only **~36 KB**). The 9.2 changelog bullets are folded into 10.0 on m32kip (Willi 2026-10-14:
   9.2 unlikely to ship, next release likely 10.0 beta); master still carries `### CHANGES V. 9.2`, so expect
   that README conflict on the next master merge (resolve by keeping 10.0 only). Then the B items below
   (B2 editorial pass, B4 decision, B9 nod, B15 pronunciation, B16/B17 preference review), then tag
   `V10.0-beta.1`.

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
inspect the three stashes).

**Wave 5 — after V10, each in its own session:** the generated-CW 6 ms fix (B5; the related
I2S-sidetone fix C6 shipped in 9.1, scope test owed); snapshot bundles, sidetone upstreaming, a11y roadmap (incl. Morsel in the a11y edition, I.5), QSO Bot
leftovers (H1–H4); rewrite the stale `devdocs/HANDOFF.md` (H5); CI gates for the FAQ and
protocol PDFs (E2).

### Decisions still open
1. **V10 target date / event?** With none, wave 2 leads; with one, wave 3 leads and the
   burst-loss decision comes first.
2. **Burst loss (B4):** ship V10 with the documented workaround (Rig Delay ≥ 250 ms), or fix
   first? Recommendation: ship with the workaround (one loss pattern, documented, 250 ms
   floor rests on one test run).
3. ~~**FAQ header**~~ — decided 2026-10-05: "firmware version **9.x**" (EN + DE, PDFs rebuilt).
4. ~~Next master release is 9.1~~ (decided) — the `### CHANGES V. 9.1` section now exists,
   opened with the BLE heap-leak fix.

### Device state (2026-10-05)
All run current master firmware (9.1 beta code; later master commits are docs/tools only):
**classic** (flashed 2026-10-05, protocol harness 122/122), **a11y Pocket** (connected now; firmware + voice image,
Bluetooth Use 0), **standard Pocket** (swapped out; C15 build cb32593, Bluetooth Use 5, Word Length "up to 4").
`/player.txt` may be Willi's File Builder test (`devdocs/language-support/file-builder-test.txt`) or the I3 bench file.
**For M32KIP work (wave 2, B6, B14) re-flash both from the `m32kip` folder.** Earlier staging (Quick Start into
Remote Rig, pass phrase) is NVS and survives, but check it. TRX Peer fields kept for now (B7).

**Repo layout (2026-10-03, iCloud):** `~/Documents/GitHub/Morserino-32` = master, `~/Documents/GitHub/Morserino-32-m32kip`
= m32kip — never switch branches in either folder (iCloud makes "name 2" copies). `.pio`/`.venv`/`.claude/worktrees`
are `*.nosync` folders behind symlinks; a new checkout needs `mkdir Software/src/.pio.nosync && ln -s .pio.nosync
Software/src/.pio` before its first build.

**Mac toolchain (2026-10-01):** Rosetta installed again, so PlatformIO's x86-only `mkspiffs`
(a11y voice image) works. Piper for voice clips: the repo's `.venv` is an old Intel build — a
working arm64 Piper 1.4.2 needs a venv on a SHORT path (espeak-ng truncates its data path at
~160 characters), e.g. `PIPER_BIN=/private/tmp/claude-501/pv/bin/piper ./generate_audio.sh`.
`xcode-select` points at Xcode 27 (`/Applications/Xcode.app`, checked 2026-10-05). JavaScript can be tested
without Node via `/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc`.

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
8. ~~`devdocs/RELEASE_AUTOMATION_DESIGN.md` stale claims~~ — the "FAQ has no `build.sh`" line fixed
   2026-10-03; the §6 Phase-2 plan, the P1 decision row and the "draft for review — no code written
   yet" status header fixed 2026-10-11. Left alone: the dead mtime warning in `release.yml` step 8
   (noted in §6, to go when that file is next touched).
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
   - ~~`a11y-speed-battery` vs `tmp-squash`~~ — **checked 2026-10-11:** identical trees, so the
     `tmp-squash` worktree and branch were removed (2026-10-14);
     **`origin/a11y-speed-battery` survives.** Its work — the a11y edition speaking the new speed
     and the battery state on return to the menu, reported by a blind operator 2026-09-11 — was
     **never merged and is not in 9.1** — rebased as `a11y-speed-battery-v91` and **merged 2026-10-14**; both branches deleted (local and origin) 2026-10-14.
   - Local `protocol-1.4` is already gone.

## B. M32KIP / V10

1. ~~Editorial pass on the `### CHANGES V. 10.0` draft~~ — Willi: the entries are fine as they are (2026-10-01).
2. ~~Editorial pass on the **V10 manual**~~ — **done 2026-10-14** for the V10-only text (the remote-keying
   section, its preferences, What's New; the V9 text had shipped already). Checked against the firmware; five
   edits approved by Willi and applied EN+DE: snapshot exclusions now list the remote-keying settings; start-up
   table gains *Out of memory* and the Rig's own *No key set*; *End: timeout* explained correctly (key lifted after
   1 s, session ends 5 s later); port note (`PUT kip/port`, same on both ends); one wording fix.
   Noticed, not changed (V9 text, also on master): the snapshot list calls the Bluetooth selector **BLT Kbd
   Output**; on the device it is **Bluetooth Use**.
3. ~~`PUT menu/start` answers nothing when not at the menu~~ — **done 2026-10-01 on `m32kip`** (0b6b1c3): it
   now answers `NOT AT MENU - stop the running mode first` and changes nothing; verified on the classic;
   protocol description updated (1.5). V9/`master` keeps the old silence.
4. **Burst-loss failure, test-plan §13.2(e):** a burst shortened one mark by 138 ms. Manual
   only carries the workaround (Rig Delay ≥ 250 ms). **Willi 2026-10-14: ship the V10 beta, investigate after.**
   Reading the code does not explain it (edges go oldest-first, a late key-down grows `d_` while the key is up,
   and `poll()` recomputes every emit time from the current `d_`, so the key-up should move with it). Next:
   reproduce in the host simulation with a loss pattern aimed at exactly one key-down's quick copies; then a
   bench re-run of profile (e) with the Rig logging edge arrival times.
5. ~~Generated CW is 6 ms short per element~~ — **fixed 2026-10-02 on `master`** (06acf11, changelog V. 9.1;
   merged into `m32kip`): dit 48.0 / dah 144.0 / space 48.0 ms at 25 WPM, dah/dit 3.002 (FINDINGS on `m32kip`).
6. **Bench tests owed by Willi.** Done 2026-10-01 (two devices, no Mac in the path — see
   `devdocs/m32kip/TEST_REPORT.md`): direct keying, WiFi pulled mid-mark (key up within the
   1 s keepalive timeout, `End: timeout` after the 5 s session drop, per spec), straight key
   and bug, 12 s tune, the Rig's `End: <reason>` display. **Still owed:** real-Internet run,
   two-hour drift session (cannot be done hands-free), a long soak, and the Keyer's paddle
   text and black-knob memories by eye.
7. **Cleanup:** clear both **TRX Peer** fields — Willi: leave them for the time being
   (Pocket's points at the classic, 192.168.1.237).
8. ~~A11y voicing of the Keyer's messages~~ — **done 2026-10-01 on `m32kip`** (6354662): connect, call, link and
   all five error screens are spoken; **link lost / restored too** (81b9f70: voice driven from the idle branch only,
   clips cut the moment CW starts). Heard by Willi on an a11y Pocket; timing benched unchanged (TEST_REPORT).
9. ~~**Pocket polled-Keyer 1.7 ms floor**~~ — **accepted by Willi 2026-10-14** (recorded under D2 in `DECISIONS.md`).
11. ~~Setting the pass phrase~~ — **done 2026-10-01 on `m32kip`** (4af1827, 39c2d3f, 39b8cb7):
    preference **Pass Phrase** on the device (MorseTextEntry, write-only, voiced), a **Remote
    Keying** card on the Configuration Tool's WiFi tab, and the Config WiFi web form — all three
    with one rule, 12–32 characters from a-z 0-9 . , : - / = ? @ + (`kipPskValid()`; upper case
    would be voiced as prosigns in the a11y edition). Web form verified by Willi. `PUT kip/psk`
    keeps only its 12-character minimum (programmatic clients). Still to do: re-enter the
    intended pass phrase on both units (Claude had re-set the bench key, B11 history).
12. ~~Preferences heading for the five "Rig:" items~~ — **done 2026-10-01 on `m32kip`** (6354662).
13. ~~Leaving Remote Rig rebooted the device when BLE Serial was on~~ — **fixed 2026-10-01 on `m32kip`**
    (07b9425): the Rig left WiFi on (modem sleep off) when returning into the menu loop, and the BLE Serial
    restart then hit ESP-IDF's WiFi/BT coexistence abort. The Rig now switches its radio off on exit, like the
    games. Reproduced and verified on the classic.
14. **Quick Start into Remote Rig rebooted the classic 2–3 times** on one cold start (USB power, BLE Serial
    off), then worked; not reproducible on demand, and no core dump was written, so not a crash — a brown-out
    or a hardware watchdog. **`GET resets`** (c22d8d8, protocol 1.5) now records why each boot happened: when it
    recurs, read it before anything else.
15. ~~**A11y pronunciation, for V10 (Willi, 2026-10-05):**~~ **Done 2026-10-14** (override changed, clip reused, old one pruned). "Disp MAC Addr" is spoken "Disp M-A-C address"; he wants
    "Display MAC address". Cause: `Software/tools/audio-accessibility/spoken_overrides.tsv` line 64 (`Disp MAC Addr` →
    `Disp MAC Address`) overrides the extractor's own `MENU_SPOKEN` entry. Fix: change that line to
    `Display MAC address` and re-run extractor + generate_audio.sh (the clip 43f7eeee already exists; the old one
    becomes an orphan to prune). Everything else voiced in 9.1 was heard and approved on an a11y Pocket.
16. **Review the order of the preferences — on the device and in the manual (Willi, 2026-10-05, for V10).** Willi
    wants to go through the order himself. Inputs: `pliste[]`/`prefPos` order and the per-mode option lists
    (`MorsePreferences.cpp`, the `posXxx` arrays passed to `setCurrentOptions`) vs the preference tables in the
    V10 manual (EN + DE). Remember CLAUDE.md §3 rule 10: `prefPos`, `pliste[]` and `prefName[]` are positional and
    must move together; NVS keys and snapshots go by name, so reordering is storage-safe, but the a11y voice
    tables and the protocol's config order follow (re-run the extractor).
17. **Check that the preferences offered inside each mode are correct and meaningful (Willi, 2026-10-05, for V10).**
    Each mode shows only a subset (its option list in `MorsePreferences.cpp`); verify per mode that nothing
    irrelevant is offered and nothing needed is missing — e.g. Word Length / Decoder Chars only where they act.
    Best done together with 16, with a mode × preference table as the working document.
    **Prepared 2026-10-14:** `devdocs/preference-review/` — `PREFERENCE_MATRIX.md` (generated from the
    preprocessed firmware tables and the V10 manual by `make_matrix.py`) and `REVIEW_NOTES.md` (six B17
    findings, e.g. Headphone Output missing in CW Generator, Key ext TX missing in CW Trx; three manual/device
    order mismatches; one order question). **Willi's first decisions implemented 2026-10-14** (mode-specific
    settings first, Bluetooth Use general, BLT <AR> CW Keyer only, B17 fixes 1–5; see the Decisions section of
    `REVIEW_NOTES.md`); second round the same day (QSO Bot and Echo order, Decoder Chars general, manual follows
    the device); B17 item 6 fixed 2026-10-15 (Bluetooth Use changed in CW Keyer now starts/stops the keyboard at once —
    bench check owed, esp. BLE Serial → keyboard). **B16/B17 done** apart from that bench check.
10. Parked for after V10: first dit-*off* delay compensation, Rig-only device, auto-start
    into Remote Rig after power loss, WinKeyer protocol idea.

## C. Hardware verification debts

1. **BLE Serial (PR #194).** Classic V2 steps 1–10 and 14 **done 2026-09-30** (see wave 2).
   Still open: step 11 (suspended-session auto-sleep + remote self-disable), step 12
   (Bluetooth keyboard interplay), step 13 (WiFi upload/OTA after a BLE session, LoRa+BLE,
   30-minute keyer soak); ≥35 WpM stalled-client CW soak (needs a fast operator); a Pocket
   re-run under access control, including **measuring the leak fix on the Pocket** (only the
   classic was measured). Doc debt: ~~`M32 Protocol.pdf` not regenerated~~ — rebuilt and CI-gated 2026-10-03 (E2).
2. **iOS app vs. a11y Pocket over BLE** — Files tab with a long `player.txt` (from the
   "Installer problems" session, commit 9c82391) *(verify pushed)*.
3. **Protocol 1.4:** `GET capabilities` and paginated `GET configs/details` **pass over BLE**
   (2026-09-30, classic). `PUT game/scores/clear` still never run against real scores.
4. **Ultimatic keyer:** never keyed on classic/OLED or with a mechanical paddle; touch-dropout
   spurious-element risk unchecked (mitigation if needed: minimum-open time).
5. **`pocketwroom-lora`** compiles, never bench-tested (shared-SPI question in its commit message).
6. **CW timing** (`devdocs/cw-timing-audit/FINDINGS.md`):
   - I2S sidetone ~6 ms heavier than the key line — **fix merged to master for V9.1 on 2026-10-11** (cherry-picked
     from branch `sidetone-alignment`, 104e6b9; branch deleted) and **released in V9.1** **without the scope test**,
     by Willi's decision; the 2-channel scope test (FINDINGS.md "How to verify") is still owed as confirmation.
   - Re-measure after the `MorseCwEngine` gap fix (Fox Hunt / Pileup / Radio Cave) never confirmed.
7. **ClickButton latch fix:** only the CW Generator long-press exit is bench-confirmed (host tests cover the rest).
   The per-site `clicks = 0` cleanup pass is **deliberately not done** (2026-10-05): with the root cause fixed those
   lines are harmless, and several also discard a *legitimate* pending click (the short press that opened a screen),
   so removing them risks behaviour changes for no gain.
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
2. ~~**`audioLevelAdjust()`** can re-enter on a long-press exit; Morsel's internal `-1` transitions~~ — **resolved
   by the ClickButton root-cause fix** (2b0b3ac, one-shot long click): the host tests' "the caller it returns to does
   NOT see it a second time" is exactly this case (re-checked 2026-10-05, all pass).
3. **A11y gaps:** decoder char-by-char voicing (now has a first consumer: Morsel in the a11y edition, I.5). ~~Battery "3980 mV" calibration readout~~ — **done 2026-10-05 on `master`**
   ("3 point 9 8 0 volts", composed) — **moot in practice**: the Pocket's Hardware Config skips Calibr. Batt.
   (`if (hwConf == 1) hwConf = 2`), so no a11y device can reach it; kept as harmless, correct code.
   **Small old bug found there:** turning the encoder *backwards* from Flip Screen maps 1 → 2 again, so the Pocket
   cannot step back past it (forward wraps round, so Cancel is still reachable). Fix: `hwConf = (t > 0 ? 2 : 0)`.
4. **A11y voice store:** **~164 KB headroom (~13 clips) on `m32kip` as of 2026-10-02**, after the extractor fix (was ~61 KB). Compose numbers; never per-value clips;
   grep `buildfs` output for "full".
5. **A11y very-late freeze** (decoder-reuse leak) — dead end, unresolved.
6. ~~**BLE transient splashes** silent in the a11y edition~~ — **done 2026-10-05 on `master`**: "Bluetooth serial suspended
   for wireless mode" / "Bluetooth serial failed to start" (2 clips; not yet heard on an a11y Pocket).
7. ~~**Protocol C15** (input-length cap)~~ — **done 2026-10-05 on `master`** (USB 1024 / `LINE TOO LONG`, documented).
9. ~~**Config Tool, File Builder: accented letters**~~ — **done 2026-10-05**: Latin-1 letters are kept (9.1 plays
   them); the option "Convert accented letters to plain ASCII" does German ä→ae ö→oe ü→ue ß→ss, Nordic å→aa æ→ae
   ø→oe, Icelandic þ→th ð→d, the rest via NFD (é→e, č→c); þ/ð and letters beyond Latin-1 are folded in both modes.
   Preset from the connected firmware (ticked below 9.1). Manuals EN/DE (File Builder + the outdated "German
   characters are converted" upload passage). The iOS app copies the tool at build time, so its next build has it.
8. **Pocket USB replies still stall now and then (HWCDC, core 2.0.17).** Found while testing C15, present in 9.0.1:
   a reply stops after its first part (always 6–7 + 64·k bytes, i.e. after a full 64-byte packet that followed a
   short first one) and the rest goes out only when the host sends again. Measured with a set/get-lesson loop:
   ~1 in 35–150 replies with the default 256-byte TX ring; **~1 in 600–900 with the 1 KB ring now set in setup()**
   (4 KB no better). Stalls recur at the same loop position across runs → deterministic timing, not noise. Kicking
   the TX interrupt from `flushReply()` did not help (tried, reverted). Candidates: core 3.x HWCDC, or TinyUSB CDC
   (`ARDUINO_USB_MODE=0`). Also fixed alongside: the 256-byte RX queue dropped bytes of longer commands (now 2 KB).
   The protocol harness now keeps a stalled reply's first part, so such a stall shows up as "withheld" instead of
   aborting the run.

## E. Docs and pipeline

1. EPUBs still carry the pre-recovery-mode text; Memory Chain HTML/PDF rebuild was open in earlier notes.
2. ~~FAQ and protocol PDFs have no CI freshness gate~~ — **done 2026-10-03**: `Documentation/check_doc_pdfs.py`
   (source fingerprint in the PDF metadata, written by both `build.sh`), CI step in `pio-ci.yml`; all three PDFs
   rebuilt (the protocol PDF had missed the 2026-10-02 sections).
3. V9.0-beta.2 shipped via a **partly manual** path; the next release is the first end-to-end
   test of the workflow timeout (30→60) and beta-asset changes.
4. `Software/iOS/M32Config/store-listing.md` had an uncommitted edit of Willi's as of 8/24 *(verify)*.
5. **Book repo:** decide §5.7 — drop the "baseline" snapshot 1, keep only snapshot 2, add one
   sentence pointing to Reset Defaults (chapter 6's snapshot 1 overwrites the baseline).
6. ~~`m32p_assembly.odt` says **TORX T8**~~ — **done 2026-10-05**: "a small Phillips (cross-head) screwdriver", ODT
   edited and PDF re-exported (LibreOffice, 300 dpi images; 0.8 MB instead of 2.9 MB). Willi, 2026-10-14: only the
   **prototypes** had TORX, every production unit is Phillips — so the wording is now right, not a compromise.
   ~~Its own "Version 1.0, 14.08.2025" line is left for Willi~~ — bumped 2026-10-14 to **"Version 1.1, 05.10.2026"**
   (the date of the content change), PDF re-exported. The export recipe, reverse-engineered from the committed PDF
   (same image dimensions, and the same size to within the text delta — the default quality of 90 gives 1.07 MB):

   ```
   /Applications/LibreOffice.app/Contents/MacOS/soffice --headless --norestore \
     -env:UserInstallation=file:///private/tmp/lo-profile \
     --convert-to 'pdf:writer_pdf_Export:{"UseLosslessCompression":{"type":"boolean","value":"false"},"Quality":{"type":"long","value":"85"},"ReduceImageResolution":{"type":"boolean","value":"true"},"MaxImageResolution":{"type":"long","value":"300"}}' \
     --outdir . m32p_assembly.odt
   ```

   Editing the ODT itself means rewriting `content.xml` inside the zip with `mimetype` kept first and STORED.

## F. People and community

1. Reply to **Rodjo** (Echo Trainer summary eating button presses) — drafted, unsent.
2. Reply to the **pause/resume claimant** — drafted; ask firmware version and hold-vs-tap.
3. PR #194 follow-ups (Andrew's leak note; minor).
4. cdaller/morserino32-trainer#9 (two bugs in the Graz snapshot set) — *(verify posted)*.

## G. iOS app

- ~~App Store review~~ — **1.0 live in the App Store** (Willi, 2026-10-05).
- **1.1 (build 2)** archived, uploaded and tested via TestFlight 2026-10-05 (File Builder accents; BLE text upload
  from the app works — previously untested). **Submitted for review 2026-10-09, automatic release; 9.1 stable
  follows once 1.1 is in the App Store.** Fixed on the way:
  `project.yml` now generates a shared scheme and takes the plist version from `MARKETING_VERSION` /
  `CURRENT_PROJECT_VERSION` (XcodeGen had hard-coded 1.0 / 1); the Team ID is the certificate's **OU**
  (F44LJ6Y4F9), not the code in parentheses — `Local.xcconfig` had the personal ID. "Xcode Cloud" prompt: declined.
- Re-encoded demo videos are in `~/Documents/My Videos`; the 207 MB master screen recording
  is still in Downloads (Willi's call whether to keep).


## I. New requests and reports from testers (added 2026-10-01)

1. ~~**A11y edition: Call Sign / Op Name entry is mostly silent**~~ — **done 2026-10-01 on `master`** (1004aab,
   changelog V. 9.1; merged into `m32kip`). Not yet heard on an a11y device. `MorseTextEntry` voices the
   *raw* character, and these two fields use upper case, which the voice pack treats as prosign codes: checked
   against `voice_manifest.json`, **C D F G I J L M O P Q R T U V W X Y Z have no clip (silent)** and **A B E K N S
   are spoken as prosigns**; the space in Op Name has no clip either. Fix: for the call-sign and name character
   sets, voice the lower-case letter (and say "space"); Practice Set must keep voicing upper case as prosigns
   (there it IS a prosign code). Shipped bug in V9 → fix on `master` (9.1), merge into `m32kip`.
2. ~~**BLE keyboard (vBand): link drops after ~30 min, possible stuck key-down**~~ — **done: merged to `master`
   2026-10-04 (ce83d2a) after Willi's bench and a real vBand test** (Mac sleep/wake, out of range); changelog V. 9.1,
   EN+DE manuals. **`m32kip`: merge master in** (from the m32kip folder). An automated rig
   (`Software/tests/ble-kbd/`: the classic as BLE test host + `soak.py`) found and confirmed, 2026-10-03/04:
   - **stuck key** after a dropout: master leaves the host with the key DOWN in 16 of 16 forced drops; fixed (the
     real Ctrl state is re-sent 1 s after every (re)connect; released before the keyboard stops);
   - **13 KB leaked per CW Keyer visit** (BLEHIDDevice's GATT objects never freed): after ~5 visits the keyboard
     could not start until power-off — fixed (own HID table, freed after deinit; 0 B/cycle measured);
   - advertising UUIDs re-added on every start (fixed); onDisconnect's 5 s block on the BLE task (gone);
   - **"the Mac ignores the keyboard" (2026-10-04, fixed 9dbac24):** after a Mac Bluetooth off/on the link was up and
     encrypted, reports were sent, and macOS dropped them all. Cause: the Arduino BLE library creates characteristics
     and descriptors in heap-*address* order, so the attribute layout was luck (on master too); the bonded Mac kept
     master's layout, wrote "notify on" into the input report's report reference, and from then on read it as report
     type 0. Fix: the table is one object (fixed order = master's), report references read-only, and a marker service
     started after encryption makes Bluedroid indicate Service Changed, so a stale host rediscovers by itself
     (verified: the stuck Mac recovered without re-pairing; 5/5 Mac Bluetooth power-cycles keyed afterwards).
     Diagnosis method (bluetoothd "statedump" in `log show`, `ioreg` HID DebugState) is in `Software/tests/ble-kbd/`.
   - **added on the branch:** Bluetooth symbol in CW Keyer's top line while a host is connected (35b243d, 08db6dc —
     also moves the BLE Serial symbol out of the volume bar inside modes); battery level reported to the host
     (0f55b91); change log + EN/DE manuals rebuilt (b098f91). `Software/tests/ble-kbd/mac_soak.py` = the Mac as host.
   - "Half-open" connections were only ever seen with the ESP32 test host (master too); the 10 s encryption
     watchdog stays as a safety net, never seen firing.
   Devices restored 2026-10-04: classic back on 9.1 (master ce83d2a), Pocket on the same build, Bluetooth Use 5.
3. ~~**Decoder: ITU brackets and national characters**~~ — **done, bench-tested by Willi on the classic and the
   Pocket (standard edition) and merged to `master` 2026-10-03** (a3e9d5a + 60fa931, changelog V. 9.1, EN+DE manuals).
   Preference **Decoder Chars** (Standard / ITU / Fr/Es/Pt / Sv/Fi / Da/No; not in the a11y edition, not in snapshots),
   Bluetooth keyboard transliteration, and the letters in user-defined training (file player, Custom Chars, Practice
   Set; Echo Trainer judges by the target word's letter). Details: `devdocs/language-support/INVESTIGATION.md`.
   **`m32kip`:** merged there 2026-10-03 (c5f492d); the Remote Keyer uses Decoder Chars (Willi), the Rig is in the
   list but decodes nothing, so its option list stays without the preference. V10 manual updated to match. Also shipped with it on master: the voice extractor
   models the a11y build (c2a13e7), `[ka]` `[p]` `[t]` in player files and `GET practicechars` over BLE (50ad66b),
   and the protocol harness no longer reboots a device by opening its port (ada42c5).
4. ~~**Minimum word length**~~ — **done: bench-tested by Willi (classic + Pocket) and merged to `master`
   2026-10-04** (f2de99e, e1d21e4). Willi's decisions (2026-10-04): words only; one ring, renamed
   **Word Length**: Unlimited / up to 2 … up to 6 / **4 or more**; if nothing fits, any length. Also the Koch Trainer
   now draws from the Oxford list (it used the 373-word list: 46 words at M32 lesson 12, now 229), frequency-weighted,
   via a 1-bit-per-word filter (613 B RAM). Protocol name changed ("Length Words" → "Word Length"; NVS/snapshots keep
   `wordLength`); config-tool help has both names. Verified on the Pocket over the protocol (generator + Koch).
   **`m32kip`:** port the manual edits to the V10 manual after merging.

5. **A11y edition: enable and voice Morsel** — [issue #225](https://github.com/oe1wkl/Morserino-32/issues/225),
   Scott N3BYY (LICW student, a11y Pocket on order, no hardware yet). **Willi's answer 2026-10-29 (posted): yes in
   principle, after V10.0, single player only; per-game config flags; spoken wrong positions.** The games were
   stripped from the a11y build with a blanket "unusable blind" (2026-06-23) that never weighed Morsel on its own.
   Checked: flash fine (~680 KB app slot left, all seven games ≈ 120 KB), voice store fine (~1.2 MB usable), the
   QSO Bot is the pattern (tick the voice engine in the mode loop, keying cuts speech, CW waits for speech).
   **Order of work:** (a) **RAM measurement first**, on the a11y Pocket: free heap / largest block at Morsel start
   with the voice pipeline resident (8-bpp sprite ≈ 52 KB; mainline measured 110 KB free after it; MP3 pipeline
   ≈ 30–40 KB) — no voicing code before this; (b) **per-game flags** (`CONFIG_GAME_MORSEL` etc., all on wherever
   `CONFIG_CW_GAME` is on; the games ring is a positional neighbour table, so each omitted game needs the
   `_wifi_beforeSelect`-style neighbour macro; the CI ring check catches mistakes); (c) **decoder-echo voicing
   design, to be settled in the thread** — the player keys the guess and cannot hear what the decoder made of it: a
   misdecode is indistinguishable from a wrong guess, and a dropped/merged character leaves the buffer short of the
   word length so the guess never submits (no replay, no feedback, lobby after 60 s). Preferred answer: voice each
   decoded character as it lands (phonetic), which is D.3's open "decoder char-by-char" gap — Morsel would be its
   first user. Also owed: Morsel's `delay()` feedback waits (0.9–1.5 s, no speech during a `delay()`) become
   tick-driven; the "clue waits for speech" gate goes into Morsel's own non-blocking CW player (not the shared
   engine); the Single/Multiplayer chooser and the "word pool too small" screen need voicing; the a11y manual
   variant has no games chapter at all (EN+DE). Multiplayer stays out. Memory Chain (CW mode) is the natural second
   game, same decoder-echo problem.
   **Scott's reply 2026-10-29 + Willi's answer (posted):** Scott is a **blind operator** himself and does accessibility
   QA part-time (the issue's analysis was Claude's, the idea his). He accepts per-character voicing. Two ideas:
   (1) **stall prompt — accepted, part of the design:** when the buffer is short of the word length, the 12 s idle
   timer speaks the count and the letters ("3 of 5 letters: Hotel, Echo, Lima") instead of replaying the clue,
   repeated until he keys on, backspaces or skips — never the silent return to the lobby; (2) **read the buffer on
   an encoder click — declined as a gesture:** black single-click in Morsel play already means "skip word", and
   UX_CONVENTIONS §13 says speech never adds a gesture or changes what an input means; per-character voicing plus
   the stall prompt cover it, revisit only with hardware in hand. First-version design as posted: revealed letter
   before the clue, each decoded character spoken as it lands, stall prompt, spoken wrong positions, spoken
   results, nothing new on the buttons. **Beta tester:** Willi asked him to test the Accessibility Edition as a whole
   once he has lived with it (PRODUCT_PLAN WS5 wants 2–3 more blind testers; the missing-clip serial log is the
   telemetry) — answer pending. When Morsel comes in, reword UX_CONVENTIONS §13 "Games are exempt — compiled out"
   and CLAUDE.md §8 case 3 (exempt only while compiled out of the a11y build).

## H. Follow-up projects with their own handoffs

1. **Snapshot bundles:** `devdocs/snapshot-bundles/HANDOFF.md` (+ `DESIGN_NOTES.md`).
2. **Sidetone upstreaming to Hari:** `devdocs/sidetone-upstreaming/HANDOFF.md`.
3. **QSO Bot:** only optional items left (T3.2 descriptors, T3.3 LLM/WiFi companion). Parked.
4. **A11y roadmap:** blind-user feedback, high-contrast themes for partially sighted users, Phase 4 flash site.
5. **`devdocs/HANDOFF.md`** still says "current focus: Fight the Pileup P6" — stale; rewrite.
6. **WinKeyer emulation (idea, 2026-10-15):** `devdocs/winkeyer/DESIGN.md` — menu mode + always-on Pocket
   auto-detect, USB only, PTT ignored, branch `winkeyer` off master (D1–D5 decided 2026-10-15); every WK2/WK3
   command walked through (§7); D8 decided (messages → keyer memories, Vcc from battery). ~5–7 sessions + bench.
   D6 (reply WK2 v23, try WK3 on the bench) and D7 (M32 owns paddle + pitch) decided after the logger research
   in §8 — all decisions taken. **Implemented on branch `winkeyer`, merged to master and pushed 2026-10-16** (b87eabc):
   bench-tested with `Software/tests/winkeyer/wk_bench.py` on the Pocket and a classic 2nd edition, results
   and the open checks (loggers, key-line timing, rev 31) in `devdocs/winkeyer/BENCH.md`. Found on the bench
   and fixed: the classic's FN is GPIO0, held down by loggers with DTR on / RTS off (now ignored).
   **Still open:** N1MM (Windows) and the revision 23-vs-31 call, key-line timing with a logic analyser.
   **Follow-up: Remote WinKeyer** with M32KIP (V10) — DESIGN.md §9.1: decisions R1–R8 taken 2026-10-17
   (WinKeyer Tx preference, Pocket only; menu "Remote WinK" on the classic). **Implemented, merged into
   m32kip and pushed 2026-10-18** (5034014); builds all 10 envs, EN+DE V10 manuals and voice clips done. The five
   implementation interpretations were decided the same day (one change: text sent during the start is dropped
   with a break-in, a926857). Link-up bench done (Pocket + classic rig, results in
   `devdocs/winkeyer/BENCH_REMOTE.md`). **Still owed:** screens/voice, paddle and encoder, the start from the
   device's menu, a real logger, an Internet path, the classic as Remote WinKeyer, the a11y edition.
   Idea for the open key-line timing checks of BENCH.md §3 (no logic analyser): the classic as Remote Rig with a
   `-D KIP_MEASURE=1` build reproduced the Pocket's key line within ~45 µs, so Remote WinKeyer can measure
   WinKeyer's element timing.

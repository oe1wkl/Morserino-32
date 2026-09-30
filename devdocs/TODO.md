# Morserino-32 — open items (loose-ends survey)

Compiled 2026-09-30 from all ~60 Claude Code sessions and the memory notes.
Section A was then checked against the live repository (same day); sections B–H
come from session transcripts and memory notes, and items marked *(verify)* may
already be done. Not yet prioritised — the tiers at the end are a proposal only.

Working branch at time of writing: `m32kip` (the V10 line). `master` = 4af466a
(= origin/master) before this file was added; claims V9.0.1, `BETA false`.

---

## A. Git / release housekeeping

1. **`m32kip` (V10) is unpushed** — 68 commits ahead of master, no `origin/m32kip` (verified).
2. **Four FAQ files uncommitted on `m32kip`** (EN + DE, `.md` + `.pdf`): new section
   "First Steps with a New M32 Pocket". Belongs on `master` (site publishes
   `origin/master`); waiting for Willi's word.
3. **`m32kip` lacks master's recovery-mode docs** (FAQ + manuals) — merge master into `m32kip`.
4. **`master` claims to be 9.0.1, not beta.** Before the next change lands: bump to a
   provisional version with `BETA true` + new changelog section (9.0.2 vs 9.1 is Willi's call).
5. **V9.0.1 draft GitHub release** — Willi publishes it in the web UI *(verify done; tag
   `V9.0.1` exists)*.
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

1. Editorial pass on the **`### CHANGES V. 10.0`** draft in `Software/README.md` (EN; DE what's-new translated).
2. Editorial pass on the **V10 manual** (currently V9 text + M32KIP sections).
3. **`PUT menu/start` answers nothing** when the device is not at the menu (any mode, any
   client). Should return an error; optionally latch the request. Not a blocker.
4. **Burst-loss failure, test-plan §13.2(e):** a burst shortened one mark by 138 ms. Manual
   only carries the workaround (Rig Delay ≥ 250 ms).
5. **Generated CW is 6 ms short per element** on the key line (dah/dit 3.21) — to-do note only.
6. **Bench tests owed by Willi:** WiFi pulled mid-mark; real-Internet run; straight key / bug;
   12 s tune; two-hour drift session (cannot be done hands-free); two devices driving each
   other directly over a long soak; Rig's "End: <reason>" display by eye; Keyer paddle text
   and black-knob memories by eye.
7. **Cleanup:** clear both **TRX Peer** fields (pair left linked; Pocket's pointed at the Mac).
8. **A11y voicing of the Keyer's messages** still owed.
9. **Pocket polled-Keyer 1.7 ms floor** — Willi's nod still owed.
10. Parked for after V10: first dit-*off* delay compensation, Rig-only device, auto-start
    into Remote Rig after power loss, WinKeyer protocol idea.

## C. Hardware verification debts

1. **BLE Serial (PR #194)**, carried to the V9 beta: classic V2 scripted tests 1–8;
   BT-keyboard interplay; LoRa+BLE coexistence; ≥35 WpM stalled-client CW soak (needs a fast
   operator); WiFi suspend/resume ×5 heap deltas; snapshot store/recall + factory reset with
   BLE Serial set. Doc debt: `M32 Protocol.pdf` (hand-exported from MacDown) not regenerated.
2. **iOS app vs. a11y Pocket over BLE** — Files tab with a long `player.txt` (from the
   "Installer problems" session, commit 9c82391) *(verify pushed)*.
3. **Protocol 1.4:** untested over BLE; `PUT game/scores/clear` never run against real scores.
4. **Ultimatic keyer:** never keyed on classic/OLED or with a mechanical paddle; touch-dropout
   spurious-element risk unchecked (mitigation if needed: minimum-open time).
5. **`pocketwroom-lora`** compiles, never bench-tested (shared-SPI question in its commit message).
6. **CW timing** (`devdocs/cw-timing-audit/FINDINGS.md`):
   - I2S sidetone ~6 ms heavier than the key line — fix proposed, ours to do; verify on a
     2-channel scope, not a microphone.
   - Re-measure after the `MorseCwEngine` gap fix (Fox Hunt / Pileup / Radio Cave) never confirmed.
7. **ClickButton latch fix:** only the CW Generator long-press exit is bench-confirmed;
   per-site `clicks = 0` cleanup pass still open.

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

## F. People and community

1. Reply to **Rodjo** (Echo Trainer summary eating button presses) — drafted, unsent.
2. Reply to the **pause/resume claimant** — drafted; ask firmware version and hold-vs-tap.
3. PR #194 follow-ups (Andrew's leak note; minor).
4. cdaller/morserino32-trainer#9 (two bugs in the Graz snapshot set) — *(verify posted)*.

## G. iOS app

- Back in App Store review (build 1, resubmitted); next move is Apple's.
- Re-encoded demo videos are in `~/Documents/My Videos`; the 207 MB master screen recording
  is still in Downloads (Willi's call whether to keep).

## H. Follow-up projects with their own handoffs

1. **Snapshot bundles:** `devdocs/snapshot-bundles/HANDOFF.md` (+ `DESIGN_NOTES.md`).
2. **Sidetone upstreaming to Hari:** `devdocs/sidetone-upstreaming/HANDOFF.md`.
3. **QSO Bot:** only optional items left (T3.2 descriptors, T3.3 LLM/WiFi companion). Parked.
4. **A11y roadmap:** blind-user feedback, high-contrast themes for partially sighted users, Phase 4 flash site.
5. **`devdocs/HANDOFF.md`** still says "current focus: Fight the Pileup P6" — stale; rewrite.

---

## Proposed tiers (for discussion)

- **Tier 1 — protect the work:** A1–A5 (push `m32kip`, FAQ commit, master→m32kip merge,
  confirm V9.0.1 published, provisional version bump).
- **Tier 2 — V10 release blockers:** B1–B3, B6 (bench tests).
- **Tier 3 — known defects:** B4, C6, C1.
- **Tier 4 — tidy and respond:** A9, E-items, D-items, F1–F2, H5.

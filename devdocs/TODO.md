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
- BLE Serial (C1): classic V2 scripted tests 1–8, Bluetooth-keyboard interplay, LoRa+BLE
  coexistence. V9 has already shipped without this gate, so it outranks the other
  defects.
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
4. Next master release is 9.1 (decided) — the `### Changes V9.1.0` changelog section is
   only needed when a 9.1.0 tag is cut; add it with the first real entry.

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

## H. Follow-up projects with their own handoffs

1. **Snapshot bundles:** `devdocs/snapshot-bundles/HANDOFF.md` (+ `DESIGN_NOTES.md`).
2. **Sidetone upstreaming to Hari:** `devdocs/sidetone-upstreaming/HANDOFF.md`.
3. **QSO Bot:** only optional items left (T3.2 descriptors, T3.3 LLM/WiFi companion). Parked.
4. **A11y roadmap:** blind-user feedback, high-contrast themes for partially sighted users, Phase 4 flash site.
5. **`devdocs/HANDOFF.md`** still says "current focus: Fight the Pileup P6" — stale; rewrite.

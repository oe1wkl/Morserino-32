# Morserino-32 — session handoff

This file is here so the **next Claude Code session** can pick up cold.
Read this first.

Upstream: `oe1wkl/Morserino-32` (`origin`). The user **is** OE1WKL — pushes
to their own repo, no fork dance. `master` is the integration branch.

Worktrees for agent work live under
`Software/src/Version 6 and newer/.claude/worktrees/<name>` and are created
fresh per task off `master` (see the git crib below).

---

## Current focus: "Fight the Pileup" multiplayer

A CW pileup-training game for the **M32 Pocket only** (`pocketwroom`,
`#ifdef CONFIG_CW_GAME`). Single-player was already complete; we are adding
multiplayer over **ESP-NOW broadcast** using a plain-text `/ftp/` protocol.

Design doc (the authoritative spec, NOT in the repo — it's in the user's
private notes):
`/Users/wkraml/Documents/Privat/Claude/HANDOFF_Pileup_Multiplayer.md`
Read it before doing protocol/gameplay work — its "Critical Constraints"
and "Networking Primitives" sections are hard-won.

### Status

| Phase | What | State |
|------|------|-------|
| P1 | ESP-NOW lifecycle + single-player default + MULTIPLAYER lobby label | **merged** (#178) |
| P2 | Beacon TX + `/ftp/` RX transport (ISR-safe ring) | **merged** (#179) |
| P3 | Beacon RX → live **MAC-keyed** roster | **merged** (#180) |
| P4 | Attacks: send `/ftp/A` on correct copy; RX enqueues a network caller; render distinctly | **in progress** |
| P5 | Network-attack defend + life cost (wire into existing defend path) | planned |
| P6 | Elimination (`/ftp/X`) + winner (`/ftp/W`), last-player-standing derived per client | planned |
| P7 | Polish: waiting-for-players UX, channel-mismatch hint, score compare, optional spectate | planned |

The MP **lobby** is fully working on `master` today: ESP-NOW comes up only
when the user toggles into multiplayer, beacons broadcast every 5 s, and
each device shows a live roster of the others. Single-player is the default
and is unchanged. Exit/re-entry is reboot-clean.

### Architecture (what a new implementer needs)

- **Files:** `MorsePileup.h` / `MorsePileup.cpp`. The ESP-NOW receive hook
  lives in `m32_v6.ino` → `onEspnowRecv()`.
- **Protocol:** human-readable, pipe-delimited ASCII over ESP-NOW broadcast
  (Option A from the design doc). Magic prefix `/ftp/` (`FTP_MAGIC`).
  - `/ftp/B|<ident>|<lives>|<score>|<inPileup>` — Beacon (every 5 s). ✅
  - `/ftp/A|<fromIdent>|<toMac>|<callsign>` — Attack. (P4, MAC-addressed.)
  - `/ftp/X|<ident>` — Eliminated. (P6)
  - `/ftp/W|<ident>|<score>` — Winner. (P6)
- **RX path (ISR-safe):** `onEspnowRecv()` checks `pileupMode` + the `/ftp/`
  prefix and calls `MorsePileup::ftpNetOnRecv()`, which only copies the
  packet (+sender MAC) into a `volatile` ring and returns — no parsing in
  the callback. The game loop drains the ring in `checkReceivedMessages()`
  and dispatches each line through `ftpHandleMessage(mac, line)`
  (`ftpSplit()` splits in place, preserving empty fields).
- **Roster is keyed by MAC**, not ident — two of the user's own Pockets can
  both be `OE1WKL` and still show as distinct rows. `FtpPlayer.mac[6]` is
  the key; ident is display-only. See `rosterUpdateBeacon()`.
- **Lobby-scoped so far:** through P3, all send/drain happens only in
  `stateLobby()`. **P4 is the first phase to touch `statePileup()`** (the
  timing-critical keyer loop) — see constraints below.

### Hard constraints when touching `statePileup()` (P4+)

- **Keyer timing is sacred.** The tight inner loop runs only
  `checkPaddles()` + `doPaddleIambic()` while keying — no `delay()`,
  no `yield()`, no drawing, no network. ESP-NOW send/receive must happen
  **only in the idle frame section** (treat exactly like `pushFrame()`):
  when `keyerState == IDLE_STATE`, no paddles pressed, input buffer empty.
- **Re-verify keyer timing after every networking change** using the
  Audacity WAV-capture method (record sidetone, confirm dit/dah lengths and
  the ~3.0 dah/dit ratio match the standalone keyer). Network code is the
  single most likely thing to reintroduce keyer jitter.
- **Never broadcast during active keying** — queue outbound packets and
  flush on an idle frame.

---

## Done earlier (Morsel multiplayer) — reference implementation

Morsel was the **first** multiplayer game on the M32 and is a good template.
It uses a binary `MSL` packet format (vs. Pileup's ASCII `/ftp/`) but the
ESP-NOW lifecycle and the ISR-safe RX-ring pattern are the same. Worth
reading `MorseMorsel.cpp` (`mslNetOnRecv`, `mslSend`, `mslNetPump`,
`mslRosterAdd`) when in doubt about the house style.

Key Morsel learning that Pileup inherits (memory + PR #172): a game must
**tear down ESP-NOW itself on exit** (`quickEspNow.stop()` + `WiFi.mode(OFF)`),
because `menu_()` only re-runs its WiFi teardown on menu *entry*, not
between two game launches. Pileup's `run()` does this (landed in P1).

---

## Dark-screen regression (DO NOT re-introduce)

On an M32 Pocket **without a battery** (USB-only power), the TFT panel stays
dark while CPU/encoder/audio/keyer all work. Battery-fitted Pockets are
unaffected, which masks the bug.

Root cause was bisected to swapping `DisplayWrapper` (a `lib_dep` LovyanGFX
wrapper) for an in-tree `LGFX` class. ~19 fix-forward attempts failed; only
restoring the `oe1wkl/DisplayWrapper` `lib_dep` path fixes it.

**DO NOT** remove `oe1wkl/DisplayWrapper` from `platformio.ini` or replace
it with an in-tree LGFX wrapper for the TFT Pocket path. All game display
goes through `display.enterGameMode()` / `pushGameFrame()` /
`exitGameMode()`. Never call `lcd.begin()` (it reconfigures the SPI pins
38/39 shared with the encoder and kills encoder interrupts).
(See `memory/tft-pocket-displaywrapper-required.md`.)

---

## Heap budget (M32 Pocket / `pocketwroom`)

| State | Free heap | Largest contiguous block |
|---|---:|---:|
| Boot complete / pre-game | ~163 KB | ~106 KB |
| After 8-bpp game sprite | comfortable | comfortable |
| After `setupESPNow` (no sprite) | ~113 KB | ~102 KB |

The 8-bpp palette sprite cut the sprite footprint from ~104 KB (16-bpp) to
~52 KB, so ESP-NOW + sprite coexist with margin — that is what unblocked the
MP gameplay path. The Pileup `/ftp/` RX ring is tiny (8 × 81 B).

---

## Conventions / quirks worth remembering

- **`m32_v6.ino N.cpp` debris.** PlatformIO occasionally leaves stale
  `Software/src/Version 6 and newer/m32_v6.ino 2.cpp` (`3.cpp`, …) from
  interrupted builds → "multiple definition of …" linker errors. Delete:
  ```bash
  rm -f "Software/src/Version 6 and newer/"m32_v6.ino\ [0-9]*.cpp
  ```

- **PlatformIO project root is `Software/src/`**, NOT the source dir
  `Software/src/Version 6 and newer/`. Running `pio` from the wrong place
  fails with `NotPlatformIOProjectError`.

- **CI.** A GitHub Actions `build` check runs on push (~2m45s). It is a
  non-required check (PRs show `UNSTABLE`, not `BLOCKED`, while it runs), but
  land PRs on green anyway. `gh pr checks <n> --watch` blocks until done.

- **Hardware available.** Multiple M32 Pockets (TFT, env `pocketwroom`,
  typically `/dev/cu.usbmodem12301`) and at least one legacy OLED
  (`/dev/cu.usbserial-0001`, env `heltec_wifi_lora_32_V2`). macOS reuses
  `usbmodem12301` for whichever Pocket is plugged in — `pio device list`
  (or `lsof`) to be sure which is on the bus. MP testing needs **two**
  Pockets on the **same ESP-NOW channel** (`posLoraChannel` pref:
  0 → ch 6, 1 → ch 1; they must match).

- **Build-broken envs on master (pre-existing, don't burn time):**
  `pocketwroom-lora` (missing `RadioLib` lib_dep) and
  `heltec_wifi_kit_32_V3` (`MorsePreferences.cpp` calls a
  `CONFIG_TLV320AIC3100`-gated function under `CONFIG_SOUND_I2S`).

- **Port locks.** The user often has a Chrome webserial tool open; a
  PlatformIO IDE extension may also hold the port via `pio device monitor`.
  `lsof /dev/cu.usbmodem12301` reveals the holder.

---

## Git crib

```bash
cd /Users/wkraml/Documents/GitHub/Morserino-32
git fetch origin && git log --oneline origin/master -5

# fresh worktree off master for a new phase:
git worktree add -b claude/pileup-mp-p4 \
  "Software/src/Version 6 and newer/.claude/worktrees/pileup-mp-p4" master

# PlatformIO (run from Software/src):
cd "Software/src/Version 6 and newer/.claude/worktrees/pileup-mp-p4/Software/src"
~/.platformio/penv/bin/pio run -e pocketwroom                      # TFT build
~/.platformio/penv/bin/pio run -e pocketwroom -t upload --upload-port /dev/cu.usbmodem12301
~/.platformio/penv/bin/pio device monitor -b 115200 --port /dev/cu.usbmodem12301
```

**Stacked-PR merge recipe** (used for #178→#179→#180): merge bottom-up,
squash each, and after each merge rebase the next branch onto the new master
to drop the already-merged commit, so every squash stays single-purpose:
```bash
gh pr merge <child> --squash
# in the next worktree:
git fetch origin && git rebase --onto origin/master <old-parent-sha>
git push --force-with-lease
gh pr edit <next> --base master      # if auto-retarget lags
```

---

## Working style preferences

The user is OE1WKL (Willi Kraml), Austrian, very technically engaged.
Across sessions they consistently:

- Want **incremental on-hardware validation** before merging — each phase is
  built, flashed, smoke-tested on real Pockets, then committed and PR'd.
- Prefer **one squashed commit per PR** with a descriptive title; PRs are
  often **stacked** (each phase branches off the previous).
- Are fine with the
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` trailer and the
  `🤖 Generated with [Claude Code](https://claude.com/claude-code)` PR-body
  footer.
- Sometimes dismiss the "Yes, flash" prompt and then say "yes" next message
  — interpret that as "flash now".
- Use spawned side-task chips happily — when an out-of-scope issue surfaces,
  spawn one rather than bloating the current task.

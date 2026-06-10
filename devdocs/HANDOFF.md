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
`#ifdef CONFIG_CW_GAME`). Single-player and multiplayer gameplay are now
both working on `master`; the remaining big piece is **P6 — elimination +
winner** (the cross-device win/lose result).

Design doc (the authoritative original spec, NOT in the repo — it's in the
user's private notes):
`/Users/wkraml/Documents/Privat/Claude/HANDOFF_Pileup_Multiplayer.md`
Worth reading for the "Critical Constraints" and "Networking Primitives"
sections. NOTE: gameplay has since diverged from that doc by explicit user
choice (revived queued UI, keyed earn-an-attack, backlog give-up) — this
HANDOFF reflects the as-built reality.

### Status

| Phase | What | State |
|------|------|-------|
| P1 | ESP-NOW lifecycle + single-player default + MULTIPLAYER lobby label | **merged** (#178) |
| P2 | Beacon TX + `/ftp/` RX transport (ISR-safe ring) | **merged** (#179) |
| P3 | Beacon RX → live **MAC-keyed** roster | **merged** (#180) |
| P4a | Revive queued DEFEND/ATTACK gameplay (single-player) | **merged** (#181) |
| P4b | MP attacks `/ftp/A` + queue/timer/reveal fixes | **merged** (#182) |
| P5 | Backlog give-up pressure (makes it losable) + 1-char submit fix | **merged** (#183) |
| UI | In-game volume control + inactivity deep-sleep | **merged** (#184) |
| **P6** | **Elimination (`/ftp/X`) + winner (`/ftp/W`), last-player-standing** | **next** |
| P7 | Polish: waiting-for-players UX, channel-mismatch hint, score compare, spectate | planned |
| — | EN/DE user-manual section for Fight-the-Pileup MP | planned |

### How the game plays today (as-built)

- **Lobby:** single-player is the default; click (mode button) toggles to
  MULTIPLAYER, which brings ESP-NOW up and shows a live MAC-keyed roster of
  peers (beacons every 5 s). Encoder picks difficulty; paddles start.
- **Pileup:** bot callers spawn on a timer and **queue up**; you DEFEND the
  oldest (CW plays, callsign revealed after `diff().playsBeforeReveal`
  plays). Each **correct defend earns one keyed ATTACK**: the pileup pauses
  (frozen) and you key a shown callsign, which in MP broadcasts `/ftp/A` to
  a random peer; that peer gets it as a magenta "FROM <you>" caller in their
  own queue. **Backlog pressure:** queued callers that wait longer than
  their patience give up ("MISSED!") and count as drops; enough drops cost a
  life. Lives 0 → GAME OVER.
- **Two per-caller timers (P5):** `FtpCaller.spawnTime` = the defend-window
  start, **reset when the caller becomes active**, so every caller gets a
  full fair window. `FtpCaller.queuedSince` = arrival time, used for the
  queue patience; **frozen during the earned-attack pause** (shifted by the
  pause duration on resume). Only the active caller and over-patience queued
  callers time out.

### Architecture (what a new implementer needs)

- **Files:** `MorsePileup.h` / `MorsePileup.cpp`. The ESP-NOW receive hook
  lives in `m32_v6.ino` → `onEspnowRecv()` (calls `MorsePileup::ftpNetOnRecv`).
- **Protocol:** human-readable, pipe-delimited ASCII over ESP-NOW broadcast.
  Magic prefix `/ftp/` (`FTP_MAGIC`).
  - `/ftp/B|<ident>|<lives>|<score>|<inPileup>` — Beacon (every 5 s). ✅
  - `/ftp/A|<fromIdent>|<toMacHex>|<callsign>` — Attack, MAC-addressed. ✅
  - `/ftp/X|<ident>` — Eliminated. **P6 — stub in `ftpHandleMessage`.**
  - `/ftp/W|<ident>|<score>` — Winner. **P6 — stub in `ftpHandleMessage`.**
- **RX path (ISR-safe):** `onEspnowRecv()` checks `pileupMode` + the `/ftp/`
  prefix, hands off to `ftpNetOnRecv()` which only copies the packet (+sender
  MAC) into a `volatile` ring and returns. The game loop drains the ring in
  `checkReceivedMessages()` and dispatches each line via
  `ftpHandleMessage(mac, line)` (`ftpSplit()` splits in place, keeping empty
  fields). **The drain runs in both `stateLobby()` and `statePileup()`'s
  idle frame** (the latter gated `!ftp.singlePlayer && !attackMode`).
- **Roster is keyed by MAC**, not ident (two Pockets can both be `OE1WKL`).
  `FtpPlayer.mac[6]` is the key; ident is display-only. `rosterUpdateBeacon()`
  also sets `eliminated = (lives == 0)`. The roster is populated in the lobby
  and is **a snapshot that is not aged during the pileup** (no beacons fire
  during a game) — keep that in mind for P6.
- **Attacks:** `sendAttack(victimIdx, call)` (broadcast `/ftp/A` to that
  peer's MAC); `ftpPickVictim()` (random active, non-eliminated peer);
  `spawnNetworkAttack(call, senderIdx)` enqueues an incoming attack.
  `ftpMyMac()` caches this device's STA MAC; `ftpMacToHex`/`ftpHexToMac`.

### P6 plan (elimination + winner)

Goal: a cross-device result. Today each device just shows its own GAME OVER.

- **On death** (lives hit 0 in `statePileup`, before going to `FTP_GAME_OVER`):
  in MP, broadcast `/ftp/X|<myIdent>` (consider a couple of repeats, packets
  can drop), and mark self eliminated. Keep ESP-NOW up.
- **On `/ftp/X` RX:** mark that peer eliminated in `players[]` (match by the
  packet's **source MAC**, like attacks do).
- **Last-player-standing:** each client derives it independently — when every
  *other* roster peer is eliminated and this device is **not**, it has won →
  broadcast `/ftp/W|<myIdent>|<score>` and show a winner screen.
- **On `/ftp/W` RX:** everyone shows the same "WINNER: <ident>" screen.
- **`stateGameOver` must keep draining** `/ftp/` messages in MP (it does not
  today) so an eliminated player still sees the eventual winner. Add the
  drain + a winner-watch there.
- **Decide the source of truth carefully:** simplest is last-player-standing
  computed per client from the `eliminated` flags (no central authority).
  Watch the edge cases: the roster is a lobby snapshot (players who never
  entered the pileup, or left, still count); a 1-human game has no peers (no
  winner broadcast — just the normal solo GAME OVER). Gate all of it behind
  `!ftp.singlePlayer`.

### Hard constraints when touching `statePileup()`

- **Keyer timing is sacred.** The tight inner loop runs only `checkPaddles()`
  + `doPaddleIambic()` while keying — no `delay()`/`yield()`/drawing/network.
  It is preserved verbatim (a never-true `waitingForResult` keeps the tuned
  guards byte-identical). ESP-NOW send/receive happens **only in the idle
  frame section** (keyer idle, no keys, like `pushFrame()`).
- **Re-verify keyer timing after networking changes** via the Audacity WAV
  capture (dit/dah lengths + ~3.0 dah/dit ratio vs the standalone keyer).
- **Never broadcast during active keying** — send only on idle frames.
- **Inactivity sleep:** every state loop calls `checkShutDown(false)` and
  `MorseOutput::resetTOT()` on input; `run()` resets the timer on entry.
  Keep new loops consistent.
- **Sprite is a few rows shorter than the panel:** keep bottom-of-screen
  content at y ≲ 305 or it clips (the status line is a single row for this
  reason).

---

## Done earlier (Morsel multiplayer) — reference implementation

Morsel was the **first** multiplayer game on the M32 and is a good template.
It uses a binary `MSL` packet format (vs. Pileup's ASCII `/ftp/`) but the
ESP-NOW lifecycle and the ISR-safe RX-ring pattern are the same. Worth
reading `MorseMorsel.cpp` (`mslNetOnRecv`, `mslSend`, `mslNetPump`,
`mslRosterAdd`) when in doubt about the house style.

Key Morsel learning Pileup inherits (memory + PR #172): a game must **tear
down ESP-NOW itself on exit** (`quickEspNow.stop()` + `WiFi.mode(OFF)`),
because `menu_()` only re-runs its WiFi teardown on menu *entry*, not between
two game launches. Pileup's `run()` does this (landed in P1).

---

## Dark-screen regression (DO NOT re-introduce)

On an M32 Pocket **without a battery** (USB-only power), the TFT panel stays
dark while CPU/encoder/audio/keyer all work. Battery-fitted Pockets are
unaffected, which masks the bug.

Root cause was bisected to swapping `DisplayWrapper` (a `lib_dep` LovyanGFX
wrapper) for an in-tree `LGFX` class. ~19 fix-forward attempts failed; only
restoring the `oe1wkl/DisplayWrapper` `lib_dep` path fixes it.

**DO NOT** remove `oe1wkl/DisplayWrapper` from `platformio.ini` or replace it
with an in-tree LGFX wrapper for the TFT Pocket path. All game display goes
through `display.enterGameMode()` / `pushGameFrame()` / `exitGameMode()`.
Never call `lcd.begin()` (it reconfigures the SPI pins 38/39 shared with the
encoder and kills encoder interrupts).
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

- **CI.** A GitHub Actions `build` check runs on push (~2m45s). Non-required
  (PRs show `UNSTABLE`, not `BLOCKED`, while it runs), but land PRs on green.
  `gh pr checks <n> --watch` blocks until done. (gh's GraphQL API briefly
  returned 401 once mid-session — a transient token hiccup; retrying worked.)

- **Hardware available.** Multiple M32 Pockets (TFT, env `pocketwroom`,
  typically `/dev/cu.usbmodem12301`) and at least one legacy OLED
  (`/dev/cu.usbserial-0001`, env `heltec_wifi_lora_32_V2`). macOS reuses
  `usbmodem12301` for whichever Pocket is plugged in — `pio device list`
  (or `lsof`) to be sure which is on the bus. MP testing needs **two**
  Pockets on the **same ESP-NOW channel** (`posLoraChannel` pref:
  0 → ch 6, 1 → ch 1; they must match). They must also see each other in the
  **lobby** first so each has the other in its (snapshot) roster.

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
git worktree add -b claude/pileup-mp-p6 \
  "Software/src/Version 6 and newer/.claude/worktrees/pileup-mp-p6" master

# PlatformIO (run from Software/src):
cd "Software/src/Version 6 and newer/.claude/worktrees/pileup-mp-p6/Software/src"
~/.platformio/penv/bin/pio run -e pocketwroom                      # TFT build
~/.platformio/penv/bin/pio run -e pocketwroom -t upload --upload-port /dev/cu.usbmodem12301
~/.platformio/penv/bin/pio device monitor -b 115200 --port /dev/cu.usbmodem12301
```

**Stacked-PR merge recipe** (used for #178→…→#184): merge bottom-up, squash
each, and after each merge rebase the next branch onto the new master to drop
the already-merged commit, waiting for the child's CI to go green after the
force-push before merging it:
```bash
gh pr merge <child> --squash
# in the next worktree:
git fetch origin && git rebase --onto origin/master <old-parent-sha>
git push --force-with-lease
gh pr edit <next> --base master          # if auto-retarget lags
gh pr checks <next> --watch              # wait green, then merge
```
After merging, clean up: `git worktree remove …` + `git branch -D …` +
`git push origin --delete …`, then `git checkout master && git merge --ff-only
origin/master`.

---

## Working style preferences

The user is OE1WKL (Willi Kraml), Austrian, very technically engaged.
Across sessions they consistently:

- Want **incremental on-hardware validation** before merging — each phase is
  built, flashed, smoke-tested on real Pockets, then committed and PR'd. They
  test each step and report back UI/gameplay issues precisely.
- Prefer **one squashed commit per PR** with a descriptive title; PRs are
  often **stacked** (each phase branches off the previous).
- Are fine with the
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` trailer and the
  `🤖 Generated with [Claude Code](https://claude.com/claude-code)` PR footer.
- Are happy to be consulted on gameplay/design forks — use `AskUserQuestion`
  for genuine design choices (addressing scheme, attack rhythm, balance) and
  proceed with sensible defaults elsewhere.
- Sometimes dismiss the "Yes, flash" prompt and then say "yes" next message
  — interpret that as "flash now".

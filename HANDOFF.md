# Morserino-32 — session handoff

This file is here so the **next Claude Code session in this worktree** can pick
up cold. Read this first.

Worktree path:
`/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6 and newer/.claude/worktrees/optimistic-jones-457246`

(The main Morserino-32 repo at `/Users/wkraml/Documents/GitHub/Morserino-32`
has `master` checked out; this worktree is a separate working copy reserved
for the agent.)

Upstream: `oe1wkl/Morserino-32` (`origin`). The user **is** OE1WKL — pushes to
their own repo, no fork dance.

## What landed in master

| PR  | Title | Notes |
|-----|-------|-------|
| #155 | Add Morsel: single-player word-guessing game | Full S0–S4. New M32 Pocket TFT game; per-word time + 5 s/guess scoring; persistent top-7 high-score table; word-length setting persisted to NVS `"morsel"`. |
| #156 | Shrink MorseOutput textBuffer from 512 to 24 chars/line | +17 KB persistent RAM on every TFT board. |
| #169 | Pin QuickEspNow to fixed fork | Resolves crash + leak across WiFi cycles. |
| #170 | Remove WiFi-cycle reboot guards; add morseGame keyer state | Simplifies WiFi mode-switching now that the QuickEspNow fork survives cycles. |
| #171 | Morsel multiplayer P2.1 lobby | Mode select, server/client role pick, MAC-keyed roster, ESP-NOW plumbing (magic-`MSL` packet format). |
| #172 | Morsel MP: tear down ESP-NOW on exit | Avoids a re-entry reboot. |
| #173 | Morsel multiplayer P2.2: synced game | Server picks 10 words from its pool and broadcasts them; clients run the unchanged single-player engine on the server's word; each device reports its final score to the server. |

## Open PRs (review-ready)

| PR  | Branch | Title | State |
|-----|--------|-------|-------|
| #174 | `diag/tft-backlight` | Fix: TFT-Pocket dark on USB-only power (revert PR #157 source path) | **Important** — see "Dark-screen regression" below. Hand-tested on a no-battery Pocket; restores the panel. |
| #175 | `claude/morsel-mp-p23` | Morsel multiplayer P2.3: ranked score table broadcast to all devices | Final P2.x deliverable. Server sorts all reported entries (incl. itself) by adjusted time and broadcasts an `MSL_PKT_SCORES` packet; both server and client render the same "Ranking" screen with the own row highlighted yellow. +200 B RAM, +916 B flash. Tested on two M32 Pockets. |
| #176 | `claude/morsel-manual-mp` | Documentation: add Morsel multiplayer section to EN and DE manuals | Source-only `.md` updates; PDFs regenerate via `Documentation/User Manual/Version 8.x/build.sh`. |

## Morsel multiplayer status — effectively complete

The streamlined-first scope from the GDD is delivered:

- **P2.1 lobby** ✓ (merged as #171)
- **P2.2 synced game** ✓ (merged as #173)
- **P2.3 ranked score broadcast** ✓ (PR #175)
- **EN + DE manual coverage** ✓ (PR #176)

What's intentionally **out of scope** for v1 (per GDD §12 and earlier
session decisions):

- Auto server discovery (server is picked by explicit choice in the lobby).
- Formal registration phase with a 3-2-1-GO countdown.
- Disconnect-mid-game polish beyond the 8 s peer-TTL drop.

A larger user-testing cohort is scheduled for early June. Issues that
surface there get folded into a follow-up PR; the current PR set is
believed sufficient for that test.

## Dark-screen regression (resolved by #174, root cause not pinned down)

Symptom: on M32 Pocket without a battery installed (USB-powered only), the
TFT panel stays dark while CPU/encoder/audio/keyer all work. Battery-fitted
Pockets are unaffected. Confirmed reproducible on a no-battery Pocket
(MAC `58:E6:C5:5A:E0:34`) by an earlier diagnostic session.

Bisected to PR #157 ("Replace DisplayWrapper with direct LovyanGFX on
TFT"). The previous commit (#156) works on the no-battery device; #157
is dark. PR #157's only substantive change was swapping the `display`
global from `DisplayWrapper` (a thin LovyanGFX wrapper pulled in as a
library archive) to an in-tree `LGFX` class. Functional examination of
both `init()` implementations shows them to be equivalent — yet the
in-tree variant breaks on USB-only-powered devices.

~19 fix-forward attempts in the side session failed (pinning LovyanGFX
to older versions; moving the LGFX global to its own TU; deferring all
panel/bus/light config from constructor to init(); re-init after WiFi
warmup; disabling all `MorseGameMode/RadioCave/Morsel::warmup()` calls;
inserting 200 ms delays after every plausible spike point; toggling
`cfg.bus_shared`; SPI clock 40 MHz → 10 MHz; rewriting the in-tree
class as a full V8.0-style wrapper). Only restoring the literal V8.0
source path (DisplayWrapper as `lib_dep`) brings the panel back.

**Memory hint** (persistent across sessions, see
`memory/tft-pocket-displaywrapper-required.md`): do NOT remove
`oe1wkl/DisplayWrapper` from `platformio.ini` or replace it with an
in-tree LGFX wrapper for the TFT Pocket path. Battery masks the bug,
so it is easy to re-introduce blindly. The proper fix-forward (back
to direct LovyanGFX, with the right magic to also work on USB-only
power) remains open work.

## Heap budget (M32 Pocket / `pocketwroom`, master)

| State | Free heap | Largest contiguous block |
|---|---:|---:|
| Boot complete / pre-game | ~163 KB | ~106 KB |
| After 8-bpp game sprite | comfortable | comfortable |
| After `setupESPNow` (no sprite) | ~113 KB | ~102 KB |

The 8-bpp palette sprite (PR #165, #168) cut the sprite footprint from
~104 KB (16-bpp) to ~52 KB. ESP-NOW + sprite now coexist with margin,
which unblocked the MP gameplay path.

## Conventions / quirks worth remembering

- **`m32_v6.ino N.cpp` debris.** PlatformIO occasionally leaves stale
  `Software/src/Version 6 and newer/m32_v6.ino 2.cpp` (and `3.cpp`,
  `4.cpp` …) in the source tree from interrupted builds. They cause
  "multiple definition of …" linker errors. **Always delete them** if
  a build mysteriously fails:

  ```bash
  rm -f "Software/src/Version 6 and newer/"m32_v6.ino\ [0-9]*.cpp
  ```

  (PR #164 added an auto-clean step but stale duplicates from before
  that change can still appear after editor-interrupted builds.)

- **Hardware available.** Multiple M32 Pockets (TFT, `/dev/cu.usbmodem12301`,
  env `pocketwroom`) and at least one legacy OLED (`/dev/cu.usbserial-0001`,
  env `heltec_wifi_lora_32_V2`). macOS reuses `usbmodem12301` for whichever
  Pocket is currently plugged in — check the MAC via `pio device list` if
  you need to be sure which device is on the bus.

- **`pocketwroom-lora` is build-broken on master** independently — missing
  `RadioLib` lib_dep. Pre-existing config issue.

- **`heltec_wifi_kit_32_V3` is also build-broken on master** —
  `MorsePreferences.cpp:1109` calls `MorseOutput::soundEventHandler()`
  under `CONFIG_SOUND_I2S`, but the function is gated by
  `CONFIG_TLV320AIC3100` in `MorseOutput.h:102`. Pre-existing
  firmware-config bug. Don't burn time on it.

- **PlatformIO project root is `Software/src/`**, NOT the source dir
  `Software/src/Version 6 and newer/`. Running `pio` from the wrong
  directory fails with `NotPlatformIOProjectError`.

- **Port locks.** The user often has a Chrome webserial tool open; a
  PlatformIO IDE extension may also auto-start `pio device monitor`
  and hold the port. `lsof /dev/cu.usbmodem12301` reveals the holder.

## Quick git crib

```bash
cd "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6 and newer/.claude/worktrees/optimistic-jones-457246"

git fetch origin master
git log --oneline -5

# branches (open PRs):
git branch | grep claude/
#   claude/morsel-mp-p23     ← PR #175 (P2.3 ranking)
#   claude/morsel-manual-mp  ← PR #176 (manuals MP section)
#
# also: diag/tft-backlight   ← PR #174 (no-battery dark-screen fix)

# PlatformIO:
cd Software/src
~/.platformio/penv/bin/pio run -e pocketwroom                              # TFT
~/.platformio/penv/bin/pio run -e heltec_wifi_lora_32_V2                   # OLED
~/.platformio/penv/bin/pio run -e pocketwroom -t upload --upload-port /dev/cu.usbmodem12301
```

## Working style preferences

The user is OE1WKL (Willi Kraml), Austrian, very technically engaged.
Across sessions they consistently:

- Want incremental on-hardware validation before merging — each
  milestone is built, flashed, smoke-tested, then committed and PR'd.
- Prefer one squashed commit per PR with a descriptive title.
- Are fine with the `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`
  trailer and the `🤖 Generated with [Claude Code](https://claude.com/claude-code)`
  PR-body footer.
- Sometimes dismiss the "Yes, flash" question and then say "yes" in
  the next message — interpret that as "flash now".
- Use spawned side-task chips happily once they know how — when an
  out-of-scope issue surfaces, spawn one rather than letting it bloat
  the current task.

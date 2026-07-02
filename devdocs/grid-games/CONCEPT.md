# Grid-Maze CW Games — Concept & Design

**Status: v1.0 (2026-07-02).** Concept for two new M32 Pocket games
proposed by Willi (OE1WKL). Done and confirmed on real hardware: the on-device
layout prototype (§10 step 0), the shared grid+path engine (§10 step 1), both
games — Trailblazer (`MorseTrailblazer.*`, step 2) and Fox Hunt
(`MorseFoxHunt.*`, step 3) — **scoring + high-score persistence**
(`MorseGridScore.*`, step 4: chars/min metric, `gridgame` NVS), and the
menu/UX conformance pass (step 6). **Multiplayer (§6, step 5) is implemented**
— shared `MorseGridNet.*`, same-maze race over ESP-NOW mirroring Morsel —
and builds on both variants; on-device testing with two Pockets is the next
verification step. **Remaining: manuals (step 8).**

**Decisions so far:** names = **Trailblazer** (A) + **Fox Hunt** (B); scoring =
**combined adjusted-time**; grid = **fixed 12 × 4** (revised from 12×5 after
on-device testing, see §2.2); Game B directions = **N/E/S/W**, legend
**always shown**; trail colour = **cyan** (§2.7); prosign cell rendering
confirmed legible at 12×4 (§2.6); multiplayer = **same-maze race**, with the
**maze itself broadcast**, not a seed (§6). **Still open (non-blocking):**
whether trail colour should be theme-derived rather than a fixed cyan (Q5).

---

## 1. The two games at a glance

Both games share one stage: a rectangular **grid** of random characters with a
**start** cell on the left edge and an **end** cell on the right edge, and a
**hidden, self-avoiding path** the device generates between them. The player
travels the path one cell at a time, from start to end. The two games differ
only in *what the player must do to take each step*:

| | **Game A — "Trailblazer"** | **Game B — "Fox Hunt"** |
|---|---|---|
| Trains | **Sending** (sight → key) | **Receiving** (sound → identify) + spatial reasoning |
| The next step is… | **shown**: the next path cell lights up (bold/colour) | **sounded**: the device plays the next cell's letter in CW |
| Player must… | key that **letter** to advance into it | key a **direction** (up/down/left/right) to the matching neighbour |
| Wrong input | error tone, stay put | error tone, stay put; sound auto-repeats after a few seconds |
| Core skill | clean, correct character sending | character recognition by ear + locating it among neighbours |

The grid + path is the same engine for both; only the per-step interaction and
the feedback differ. That makes them natural siblings to build together.

---

## 2. Shared foundations

### 2.1 Hardware scope

- **M32 Pocket / TFT only**, gated by `CONFIG_CW_GAME` (the same flag as the
  existing four games). They become games #5 and #6. The classic OLED M32 does
  **not** get them — consistent with Morsel, Invaders, Pileup, Radio Cave.
- They run under the dedicated **`morseGame`** state (hard rule §3.8), so they
  are local-sidetone-only and never key a real rig, and they are excluded from
  every LoRa/WiFi/external-TX gate.
- Display uses the **`MorseGameMode`** sprite infrastructure: allocate a fresh
  game sprite on entry (`enterLandscape`, 8-bpp palette via `GamePalette.h`),
  `pushFrame()` each frame, `exit()` on leave. Landscape orientation (the maze
  needs width).

### 2.2 The grid

- Random characters drawn from the **current Koch lesson set**
  (`koch.getCharSet()`), exactly like Morsel builds its word pool. A character
  may repeat freely across the grid.
- **Baseline: playable from Koch lesson 6** (6 distinct characters — enough to
  fill a grid that doesn't look monotonous).
- **Dimensions — FIXED at 12 × 4.** Not player-adjustable. The desktop layout
  prototype had picked 12×5 (see the struck-through reasoning below); on-device
  testing of Fox Hunt's real layout (HUD band + grid + always-on legend, §2.5)
  showed 5 grid rows plus the HUD and legend band came to 7 visual "lines" on
  a 170px-tall panel — workable but tight, and set to get *worse* once prosign
  cells (§2.6) need more row height for the shrink-font-plus-overbar treatment
  at higher Koch lessons. Dropped to **4 rows** (HUD + 4 grid rows + legend =
  6 lines) to give every row more breathing room. Column count (12) was not
  implicated and is unchanged.
  <br>*(Original desktop-prototype reasoning, superseded by the above: 12×5 was
  picked as the column count that keeps glyphs and path lines clearly legible
  while still reading as a proper maze — 15×5, the reference image's size,
  felt cramped once path lines crowd the cells.)*
- **Prosign cells need extra width — see §2.6.** A 12-wide grid sized for
  single characters does not, as-is, have room for prosign cells; this needs a
  resolved layout before implementation (not before the concept, see §2.6).
- Neighbours are **orthogonal only** (4-neighbourhood: up / down / left /
  right), matching the reference image and Game B's four direction signals. No
  diagonals.

### 2.3 The hidden path

- A **self-avoiding walk** from the start cell (a random row on column 0) to the
  end cell (right edge). It may turn any number of times but never touches
  itself. Generation: randomised depth-first search with backtracking, biased
  to reach the right edge; on a ≤ 75-cell grid this is trivial for the ESP32.
- Only the path is "correct"; every off-path cell is a dead end for scoring
  purposes. In Game A the whole grid of letters is visible but only the **next**
  path cell is highlighted (you don't see the whole path in advance). In Game B
  nothing is highlighted — the path is revealed only as you walk it.
- **The path ahead is NEVER drawn** in either game — `drawBoard()`
  (`MorseGridEngine.cpp`) renders only the *walked* trail, behind the player.
  This is load-bearing, not cosmetic: an early build previewed the unwalked
  path as a dim band (carried over from the layout prototype, where seeing the
  whole path was the point), and a test user could then follow the Fox Hunt
  maze by eye **without decoding any CW at all** — defeating the game. Do not
  reintroduce a path preview.
- The path is generated once per game; in **multiplayer** the server
  broadcasts the **generated maze itself** (grid + path, ~100 bytes) so every
  device races the identical one. *(The original idea of broadcasting a path
  seed was dropped during implementation: `koch.getCharSet()` depends on the
  Koch **sequence** preference — M32 native / LCWO / CW Academy / LICW /
  custom — as well as the lesson number, so seed-based regeneration would
  silently produce different grids on devices with different sequence
  settings. Broadcasting the maze verbatim is immune to any local-preference
  divergence — the same reason Morsel broadcasts its actual words rather than
  pool indices. See §6.)*

### 2.4 Sound feedback (both games)

All audio through **`MorseOutput::pwmTone`** (hard rule §3.1). Proposed scheme,
reusing the conventions players already know from the classic modes:

- **Correct entry:** short confirmation blip (or a brief rising two-tone) +
  advance animation along the path.
- **Wrong entry:** low error tone (the same "you got it wrong" feel as Echo
  Trainer / Morsel's `<err>`).
- Game B additionally **plays the target character** in CW at the player's
  current keyer speed, and **auto-repeats** it after a few seconds of inactivity
  (and on demand — see §4.2).
- `MorseOutput::resetTOT()` on every paddle/button/encoder event (hard rule
  §3.2) so the timeout watchdog never fires mid-maze.

### 2.5 Speed and volume controls — required, per UX_CONVENTIONS.md

Both games **must** expose the standard global live-adjustment gesture
(`devdocs/UX_CONVENTIONS.md` §2), not a bespoke one:

- **Encoder owns speed by default** (turn → `changeSpeed()` / the games' shared
  value-core `changeSpeedValue()`), displayed in the game's own HUD rather than
  the classic top-bar slot.
- **Red button single-click toggles the encoder to volume** (→ `changeVolume()`
  / `changeVolumeValue()`); a second red click returns it to speed and persists
  the value (`writeVolume()`).
- Neither control may be trapped or repurposed by the game. In Game B this
  matters in particular: the red-button click that toggles speed/volume must
  stay free for that role — any on-demand CW-replay control (§4.2) needs a
  *different* input (e.g. the black knob's button-press, or a long-press,
  following whatever UX_CONVENTIONS already defines for "repeat" elsewhere —
  check before improvising; see §6.2/UX_CONVENTIONS guidance to propose an
  addition if nothing fits).
- Sidetone pitch always follows the global preference, as in every other mode.

This was missed in the first draft and is corrected here — it is **not**
optional polish, it's the same hard requirement every classic mode and
existing game already follows.

### 2.6 Prosign cells — extra room needed

Later Koch lessons introduce **prosigns** (`S A N K E B +` → `<as> <ka> <kn>
<sk> <ve> <bk> <ar>`), and the grid-fill pool (§2.2) draws from
`koch.getCharSet()` without distinction — so once the lesson reaches them,
prosigns **will** appear as grid cells, including possibly on the path itself.
A single grid cell sized for one character is too small for a two-letter
prosign abbreviation legibly.

**Morse Invaders' existing solution** (`MorseGame.cpp`, `getDisplayStr()` /
`drawInvader()`) is the precedent to reuse rather than reinvent:

- Prosigns are **never spoken as their raw letter** — they're rendered as the
  conventional **two-letter abbreviation** (`AS`, `KA`, `KN`, `SK`, `VE`, `BK`,
  `AR`), upper/lower-cased per the `posOutputCase` preference, exactly like the
  classic modes display them. It is only the **on-screen glyph** that needs 2
  chars of room — the underlying prosign character used for keyed-input
  matching and CW generation stays the single uppercase code, never
  lowercased or split before reaching CW generation (hard rule §3.4).
- The invader box draws the 2-letter form in a **smaller font**
  (`DejaVuSansMono_Bold11pt7b` vs `15pt7b` for a single char) plus a drawn
  **overbar** above it (`drawFastHLine`), the standard written notation for a
  prosign — `strlen(dispBuf) > 1` is the existing `isProsign` test to copy.
  Both rotated and unrotated draw paths handle it already.
- Invaders affords this because its invader "cells" are sized independently of
  a fixed grid. **Our grid cells are fixed-size by definition** (§2.2), so the
  same shrink-font-and-overbar treatment needs to fit *inside* a 12×5 grid
  cell rather than a bespoke box — this is a real layout constraint to test in
  the on-device prototype (§10), not assumed to just work. A worst case fallback
  (e.g. excluding prosigns from the grid-fill pool while still allowing them on
  the path, or biasing path generation away from them) should be kept as a
  fallback if the shrink-and-overbar treatment doesn't fit legibly at 12×5.

### 2.7 Path/trail colour — **cyan decided; theme-derived still open (§9-Q5)**

Reference-image trail look is an amber/tan line; there is no true amber in the
8bpp game palette (`GamePalette.h`) yet, so the prototype offered a straight
choice between the closest actual palette entries: cyan (Morsel's accent) or
yellow. **On-device comparison: cyan preferred over yellow** — set as the
prototype's default.

Separately, Willi raised making the path colour **follow the device's current
theme** (the classic modes and games already support theme switching) rather
than a hardcoded colour. Attractive but adds complexity: themes are tuned for
text/background legibility, not necessarily for a path-trail accent that must
stay visually distinct from grid-letter colours and from the Game A highlight
/ Game B "current cell" token in every theme. **Still deferred** — needs
either (a) a per-theme accent colour already defined that this can borrow, or
(b) a new themed accent added to the theme table. Worth a quick look at how
Radio Cave / Morsel pick their accent colours per theme before deciding. If
theming turns out not to be worth the complexity, cyan is the fallback
default. See §9-Q5.

---

## 3. Game A — "Trailblazer" — *see & send*

### 3.1 Mechanic

The full grid is shown. The cell adjacent to the player's current position that
lies next on the hidden path is **highlighted** (bold / accent colour). The
player **keys that visible letter** in Morse. Correct → the player moves into
that cell, it becomes the new "current", and the next path neighbour lights up.
Wrong → error tone, no move, try again.

Because the target letter is *visible*, this is fundamentally a **sending /
keying drill** — the maze supplies motivation (a journey with visible progress)
and a natural difficulty/scoring axis (path length, time, send accuracy). It is
the gentlest of the two and a good first game from lesson 6.

### 3.2 Screen

- Grid centred, current cell marked (e.g. a token/dot like the reference
  image's pale circle), next path cell highlighted.
- HUD band: Koch lesson, keyer speed, volume (encoder target, Morsel-style),
  plus progress (steps done / total) and a running timer or error count.
- Travelled trail drawn behind the player (so progress is visible), like the
  brown line in the reference image.

### 3.3 Difficulty knobs

Path length (grid size), and optionally a **per-step time pressure** (a soft
countdown that costs score, not a hard fail) for advanced players.

---

## 4. Game B — "Fox Hunt" — *hear & navigate*

### 4.1 Mechanic

The device **plays the CW** of the next cell on the path (a neighbour of the
current cell). The player must (a) recognise the character by ear, (b) find
which of the (up to 4) neighbours holds it, and (c) key the **direction** toward
it. Correct direction → advance and the next character is played. Wrong → error
tone, stay put.

This trains **receiving** with a spatial twist. A player *could* brute-force the
four directions, but every wrong key costs score (§5), so decoding is the
efficient route — same philosophy as Morsel (you can guess, but it hurts your
score).

**Neighbour-uniqueness constraint — CONFIRMED essential, not a theoretical
edge case.** For the heard character to be unambiguous, the target cell's
character must be **distinct from the other neighbours** of the current cell
— otherwise the player can decode the sound correctly and *still* key the
wrong direction, because two neighbours matched. The first on-device
prototype build did **not** yet implement this fix-up (grid fill was plain
independent-random per cell) and Willi caught the problem immediately by eye:
at Koch 6 the pool is only 6 characters against up to 4 neighbours, so by
birthday-paradox odds a collision is the *likely* outcome of independent
random fill, not a rare exception. Confirms this constraint is load-bearing
for Fox Hunt to be a fair game at low lessons, not a nice-to-have.

**Fix (`fixNeighbourCollisions()` in `MorseGridEngine.cpp`; first proven in the
since-removed prototype, confirmed working on-device):** after filling the grid,
for every path cell's set of up-to-4 orthogonal neighbours, re-roll any
character that duplicates another neighbour's character in the same set,
picking a replacement that clashes with none of the others. Runs a few passes
over all path cells since a fix at one cell can occasionally reintroduce a
collision at a neighbour-sharing cell processed earlier in the same pass;
cheap and converges reliably at this grid size — re-testing at Koch 6 no
longer showed colliding neighbours. **The real Trailblazer/Fox Hunt
implementation must carry this forward** — it belongs in the shared grid-fill
step (§2.2), not just the prototype.

### 4.2 Replay / pacing

- The target plays once on arrival at a cell.
- If the player is idle for **N seconds** (proposed ~3–4 s, possibly tied to
  speed), it **auto-repeats**.
- An **on-demand replay** control lets the player re-hear without waiting. Per
  §2.5, this **cannot** be the red button (reserved for speed/volume toggle) —
  candidates are the black knob's push-button, or a dedicated gesture. Needs a
  concrete proposal checked against `UX_CONVENTIONS.md` before implementation;
  if nothing already-defined fits, propose an addition to that doc rather than
  improvising (per CLAUDE.md §6).

### 4.3 Direction signalling + Koch substitution — **N/E/S/W, decided**

Directions are keyed as single letters, but the letters are learned at very
different points and *which* Koch sequence the user follows matters. Two
candidate alphabets were considered:

- **(i) U / D / L / R** (up / down / left / right) — the "D-pad" convention.
- **(ii) N / E / S / W** (north / east / south / west) — compass directions.
  **Already used by Radio Cave**, where the player keys single-letter `n e s w`
  to move between rooms — so it's an *established M32 convention*, and it fits
  Fox Hunt thematically (direction finding = taking a bearing). Map convention
  is north-up, so N=up, E=right, S=down, W=left.

**Decided: N/E/S/W** — it matches the existing Radio Cave convention, fits the
game's theme, and (per the table below) becomes fully native far earlier in
the default Koch order than U/D/L/R does.

Lesson positions in the **default M32 order** (`mkrsuaptlowi…njef…`):

| Maps to | U/D/L/R letter | learned | N/E/S/W letter | learned |
|---|---|---|---|---|
| Up    | U | 5  | N | 14 |
| Right | R | 3  | E | 16 |
| Down  | D | **38** | S | 4  |
| Left  | L | 9  | W | 11 |
| **Fully native at** | | **lesson 38** | | **lesson 16** |

**Takeaways:** (1) at lesson 6 *both* alphabets need substitution (U/R native
for U-D-L-R; only S native for N-E-S-W). (2) But **N/E/S/W becomes fully native
much earlier** — lesson 16 vs lesson 38 — because `D` is taught nearly last in
the M32 order; so a mid-level player keys real compass letters with no
substitution far sooner. (3) The mix differs again for LCWO / CW Academy / LICW,
so substitution must be computed live against `koch.getCharSet()` either way.

**Assignment rule (agreed):**

1. For each direction, if its canonical letter (U/D/L/R) is in the current Koch
   set and not already taken, use it.
2. Otherwise substitute the first not-yet-assigned character from the current
   Koch set (in learning order), preferring short/aurally-distinct codes.
3. Compute against `koch.getCharSet()` so it is **sequence-agnostic** and adapts
   as the player advances lessons.

**When to show the legend — decided: always show.** A persistent four-arrow
legend (e.g. `↑N  ↓S  ←W  →E`) is shown at all times, substituted or not —
zero memorisation, maximum consistency. Confirmed workable in the desktop
layout prototype as a fixed bottom strip under the 12×5 grid; to be
double-checked on the real panel (§10) alongside the grid-size confirmation.

Note: the four direction-input letters may coincide with grid letters — that's
fine, because in Game B *all keyed input is interpreted as a direction* via the
legend; the player never keys the heard letter itself. Keying a char that isn't
in the legend → "not a direction" error.

---

## 5. Scoring — **effective speed (chars/min)** — DONE

Shipped in `MorseGridScore.cpp` (`record()`), one maze = one scored game:

```
steps = pathLength() - 1                         // correct entries = moves
adjMs = elapsed + wrong × 5000ms                 // penalty per wrong entry (tunable)
cpm   = round(steps × 60000 / adjMs)             // effective chars/min, HIGHER better
```

**Why chars/min, not raw adjusted-time:** the concept originally said "combined
adjusted-time" (elapsed + penalty × wrong, lower better) — the same idea, but
it assumed a fixed amount of work per game like Morsel's 10 words. Our mazes
have **random path lengths** (a 12×4 self-avoiding path runs ~12–40+ cells), so
raw solve-time isn't comparable between a lucky short maze and a long one.
Dividing by path length turns it into a length-normalised *rate* that's fair
across random mazes — and it's a meaningful CW metric (your effective
send/receive speed). Decided with Willi 2026-07-01. `adjMs` still carries the
per-error penalty, so accuracy still counts. High-score table sorted by `cpm`
descending, per game.

The end-of-game results screen (`showResultsAndHiscores()`) follows the
Title-Case result-screen convention (consistency-audit Phase D): game title +
"Solved!" + the CPM, then steps/time/wrong/Koch and rank, then the high-score
table.

---

## 6. Multiplayer — same-maze race, mirroring Morsel — DONE

Shipped in **`MorseGridNet.cpp/.h`**, one module shared verbatim by both games
(only the title string and a game id differ). It reuses Morsel's ESP-NOW model
almost 1:1 — pure broadcast, no pairing; protocol magic **"GRD"** (routed in
`onEspnowRecv()` next to Morsel's "MSL" branch); receive ring filled in
callback context, drained in the game loop; server roster keyed by sender MAC
with a TTL; beacon/join/start/result/scores packet types:

1. Each game now opens with a **mode select** (Single player / Multiplayer,
   Morsel's picker grammar). On Multiplayer, one device becomes **server**
   (role pick → roster lobby), the rest **join** (client wait screen). The
   server's session starts at the **full-alphabet Koch default** (41,
   encoder-adjustable in the lobby; the operator's real lesson is restored on
   leaving the server role and again in `teardown()`), exactly like Morsel's
   server lobby.
2. On START the server `generate()`s under its Koch set and broadcasts the
   **serialised maze** (`MorseGridEngine::exportState()` — grid chars + path
   cells, ≤ 97 bytes payload; clients validate and `importState()` it).
   Everyone then races the **identical maze**. *Why the maze and not a seed:
   see §2.3 — the Koch sequence preference makes seed-regeneration unsafe.*
   Clients also **adopt the server's Koch lesson** for the race (restored on
   leaving the multiplayer flow): the maze travels in the packet, but Fox
   Hunt's direction legend is computed against the local `koch.getCharSet()`,
   and a client left at a low lesson would otherwise race with shorter
   substituted direction letters than the server's canonical N/E/S/W.
3. Each device reports **elapsed time + wrong entries** when it finishes
   (resent every second until it appears in the ranking); the server ranks
   everyone — itself included, competing as a regular player — by **adjusted
   time** (`MorseGridScore::adjustedMs`, which on an identical maze orders
   exactly like descending CPM) and broadcasts the table; both sides render
   it with the **CPM** each result works out to (`MorseGridScore::cpm`, the
   identical formula as the solo tables — a multiplayer CPM always matches
   what the same run would have scored solo).

Multiplayer results are **transient** — never written to the `gridgame` NVS
tables (§7), as in Morsel. The ranking screen's black-click returns to the
multiplayer lobby with the role kept ("play again"); black-long exits, per
the §12 games grammar. **Q7 resolved: plain same-maze race** — no turn-based
mode, no live "ghost" progress (nothing is transmitted during play; the maze
stays a solo effort until the finish times meet on the ranking screen).

---

## 7. Persistence / NVS — DONE

Per CLAUDE.md §4 (one-byte version field; treat an absent stamp as empty):

- **Shipped:** one namespace `gridgame` shared by both games, holding two
  versioned high-score tables — keys `tb`/`tbv` (Trailblazer) and `fh`/`fhv`
  (Fox Hunt), each 7 rows of the `GridScore` struct (`MorseGridScore.cpp`).
  An absent or mismatched version stamp yields an empty table, so the format
  can evolve safely. Now the 5th NVS namespace (CLAUDE.md §4 updated).
- Multiplayer results are transient (not persisted), as in Morsel (§6).

---

## 8. Menu & state integration

- Two new entries in the games submenu, behind `CONFIG_CW_GAME`.
- Each game is a self-contained module (`MorseTrailblazer.cpp/.h`,
  `MorseFoxHunt.cpp/.h` — names provisional) exposing `run()` + `warmup()`,
  dispatched from `MorseMenu.cpp` under the `morseGame` state (hard rule §3.8).
- Lobby UX (single vs multiplayer pick, Koch lesson adjust — grid size is fixed,
  §2.2 — start gesture = paddle/key per the unified Phase-D gesture) mirrors
  Morsel so the controls are already familiar.
- Conforms to `devdocs/UX_CONVENTIONS.md` (global speed/volume/exit/preferences
  mechanisms unchanged; games may differ only in screen layout, never in
  control grammar). Any genuinely new interaction (e.g. on-demand CW replay in
  Game B) is proposed as an addition to that doc, not improvised.

---

## 9. Decisions

**Settled:**

- **Names** — Game A = **Trailblazer**, Game B = **Fox Hunt**.
- **Scoring** — **combined adjusted-time** (§5).
- **Grid dimensions** — **fixed 12 × 4** (§2.2). Started at 12×5 from the
  desktop prototype; dropped to 4 rows after on-device testing showed Fox
  Hunt's HUD+grid+legend too tight at 5, worse once prosign cells need more
  row height. Confirmed on real hardware.
- **Game B direction alphabet** — **N/E/S/W** (§4.3): matches the existing
  Radio Cave convention, fits Fox Hunt thematically, native by lesson 16
  (vs. lesson 38 for U/D/L/R).
- **Game B legend behaviour** — **always shown** (§4.3).
- **Speed/volume controls** — both games **must** use the standard
  encoder-speed / red-click-volume gesture per `UX_CONVENTIONS.md` §2; not
  optional, see §2.5. (Corrects an omission in the first draft.)
- **Path/trail colour** — **cyan** (§2.7), confirmed over yellow on-device.
  Whether it should instead be theme-derived is separately still open (Q5).
- **Fox Hunt neighbour-uniqueness fix-up is mandatory** (§4.1): confirmed
  on-device to be a common-case bug, not a rare edge case, at low Koch
  lessons. Must carry into the real implementation's shared grid-fill step.
- **Prosign cell layout** (§2.6): the Invaders-style shrink-font-plus-overbar
  treatment, tested at the new 12×4 size up to the MAX Koch lesson (all
  prosigns visible), confirmed legible on real hardware. No fallback needed.
- **Q6 — Game B on-demand replay control** (§4.2), resolved in step 6:
  black-knob single-click during play (§1a context split — the game has no
  pause); documented in `UX_CONVENTIONS.md` §12.
- **Q7 — Multiplayer format:** plain **same-maze race** (§6) — no turn-based
  mode, no live-progress "ghosts". **The maze travels in the START packet**
  (engine export/import), not a path seed — seed-regeneration is unsafe
  across differing Koch *sequence* preferences (§2.3).

**Open:**

- **Q5 — Should path/trail colour be theme-derived** rather than the fixed
  cyan above (§2.7)? Needs a look at how Radio Cave / Morsel pick per-theme
  accents before deciding; could add real complexity if no suitable theme
  hook exists.

---

## 10. Implementation sketch (after sign-off)

0. **On-device layout prototype — DONE.** `MorseGridProto.cpp`, tested on real
   hardware, confirmed the 12×4 grid, always-on Game B legend, prosign cell
   legibility up to the MAX Koch lesson, cyan trail colour, and fixed the
   neighbour-uniqueness bug (§4.1). Only Q5 (theme-derived colour, lower
   priority — cyan is a working default) remains open from this phase.
1. **Shared grid+path module — DONE.** `MorseGridEngine.h`/`.cpp`: generation
   (path + Koch-filtered fill + the neighbour-uniqueness fix-up, carried
   forward exactly as confirmed in step 0), `drawBoard()`/`drawLegend()`
   rendering, and the `directionMatchesNext()` correctness check Fox Hunt's
   input loop will need (new — the prototype only ever displayed, never
   checked correctness). `MorseGridProto.cpp` was refactored into a thin
   wrapper around it (own HUD/lesson-cycling/toggles only) as the extraction's
   on-device proof; re-tested pixel-for-pixel identical to the step-0 baseline.
   *(That prototype has since been removed — step 6 — now both real games
   exercise the engine.)*
2. **Game A, Trailblazer — DONE and playable.** `MorseTrailblazer.h`/`.cpp`,
   real permanent menu entry (Games, after Morsel). Highlight-next +
   key-letter loop on the real CW keying/decode pipeline (`gameCharBuffer` +
   `doPaddleIambic()`, the same mechanism every other game uses), OK/ERR
   audio feedback (`MorseOutput::soundSignalOK()`/`soundSignalERR()`),
   mandatory speed/volume gesture, solve-and-continue loop. Confirmed
   end-to-end on real hardware. Hit and fixed a real bug along the way: the
   decoder's trailing inter-word space arrives as its own `gameCharBuffer`
   event after every completed letter and was being scored as a second wrong
   answer, doubling every result into `<result>, ERR` — the single-character
   form of the "decoder emits a trailing space" hard rule; fixed by filtering
   a bare space out of the poll, the same way Morsel's own accumulator only
   accepts alphanumerics. **Fox Hunt (step 3) will hit the exact same
   gameCharBuffer mechanism and must carry this filter forward too.**
   Deliberately not in this step: no lobby/Koch-lesson override (plays at the
   player's live setting), no scoring/NVS (step 4 — "Solved!" is a
   placeholder with no score shown yet, by design), no multiplayer (step 5).
3. **Game B, Fox Hunt — DONE and playable.** `MorseFoxHunt.h`/`.cpp`, real
   permanent menu entry (Games, after Trailblazer). Plays the next cell's CW
   via the shared `MorseCwEngine` (case preserved so prosigns sound correctly,
   confirmed on-device), N/E/S/W legend + `directionMatchesNext()` correctness
   check, idle auto-replay + black-short-click on-demand replay (resolving Q6),
   OK/ERR feedback, speed/volume gesture. Carried forward the gameCharBuffer
   trailing-space filter from step 2. Polished on-device: clue plays at a
   half-tone-shifted pitch (`posEchoToneShift`, the Echo-Trainer/Trx
   mechanism) to distinguish device CW from the player's sidetone; clue
   cancels (not pause-resumes) the instant the player keys; OK-tone→next-clue
   breath; tuned idle-replay interval. Play-testing here surfaced a CW timing
   asymmetry now fixed in the shared engine + both games' loops — see
   `devdocs/cw-timing-audit/`. Deliberately not in this step: no scoring/NVS
   (step 4 — "Solved!" is still a placeholder), no multiplayer (step 5).
4. **Scoring + `gridgame` NVS high-score tables — DONE.**
   `MorseGridScore.h`/`.cpp`: shared `GridScore` struct + two versioned NVS
   tables (§7), `record()` computing chars/min (§5), and
   `showResultsAndHiscores()` — the whole end-of-game UI (results screen →
   high-score table → play-again / exit), shared by both games so it's not
   duplicated. Each game now tracks start-time + wrong-count per maze and, on
   solve, records the score and shows results instead of the old
   auto-continue. Both variants build clean.
5. **Multiplayer — DONE (implementation + both-variant builds; two-device
   on-device test pending).** `MorseGridNet.h`/`.cpp`, one module shared by
   both games — Morsel's netcode cloned (roster, beacons, result reporting,
   ranked SCORES; magic "GRD" routed in `onEspnowRecv()`), with two grid-game
   twists: the START packet carries the **serialised maze**
   (`MorseGridEngine::exportState()`/`importState()`, replacing the planned
   seed broadcast — see §2.3/§6 for why) and the ranking uses the shared
   scoring formula (`MorseGridScore::adjustedMs`/`cpm`, now exposed from the
   step-4 module so solo and multiplayer can never drift apart). Each game
   gained a Morsel-style mode select (Single player / Multiplayer) ahead of
   its ready screen, an `FH_MP_SOLVED`/`TB_MP_SOLVED` end state (transient
   ranking screen instead of the NVS tables), and a `MorseGridNet::teardown()`
   call at `run()` exit (ESP-NOW down + Koch lesson restore — same
   between-game-launches reasoning as Morsel's own teardown).
6. **Menu / UX conformance — DONE.** Throwaway Grid Proto entry removed
   (2026-07-01). High-score viewer on each game's ready screen via
   **black-knob single-click** (not red-hold — red stays volume-only per §2;
   red-long overload is the H2 divergence). Fox Hunt's on-demand CW replay
   (Q6, resolved) is **black-knob single-click during play** (§1a context
   split — the game has no pause). Both gestures now documented in
   `UX_CONVENTIONS.md` §12; result-screen wording is Title-Case per that spec.
7. Build **both** variants (TFT-only, but confirm the OLED env compiles with
   the feature `#ifdef`-ed out) — done every step so far.
8. **Manual (EN + DE) sections — still TODO**, deliberately deferred until the
   feature is complete so it's written once against the final shape rather
   than churned. With multiplayer (step 5) implemented, the feature set is now
   final — the manual sections are unblocked and are the **last remaining
   step** (after the two-device multiplayer test confirms step 5 on real
   hardware). Tracked here per CLAUDE.md §7.

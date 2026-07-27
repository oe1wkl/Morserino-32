# Memory Chain — concept and design

Designed with Willi 2026-07-11 (conversation-driven; this file records the
settled decisions). Implementation: `MorseMemoryChain.cpp/.h`, menu wiring in
`morsedefs.h` / `MorseMenu.cpp`. TFT/Pocket only (`CONFIG_CW_GAME`); the OLED
build compiles it out.

## 1. Concept

A memory game — "I packed my suitcase" in CW. The device presents **one new
character per round** (never replaying the earlier chain); the player keys the
**whole chain** from memory, untimed between characters. The player's own
keying of the full chain each round *is* the rehearsal — every round doubles
as keying practice, and the device only ever sends a single character.

Deliberately **no OK/ERR sounds**: audible feedback between letters would
wreck the keying flow of the chain (Willi's call — this also means the game is
*not* an accessibility-mode candidate; it stays strictly in the game domain).
All feedback is the box row.

## 2. Content modes (lobby choice)

| | Characters | Call Signs |
|---|---|---|
| Chain content | random chars from the Koch lesson set (uppercase prosign markers filtered out — no single glyph fits a box); no immediate repeats | ONE call *is* the chain, revealed letter by letter (`O → OE → OE1 → … → OE1WKL/P`); on completion a fresh call starts, endlessly |
| Error rule | **one tolerated error per round** — box turns red showing the correct char, play continues; the **second error in the same round** ends the game | **no tolerated error** — first wrong character ends the game |
| Koch | lobby-selectable (writes the global `kochFilter`, like Morsel's lobby; persisted via `writePreferences` on exit) | ignored — calls use the full set (letters, digits, `/`) |
| Score | completed chain length (tiebreak: boxes reached in the failed round) | completed calls (tiebreak: letters banked in the failed call) |

Call generation: `getRandomCall(0)` **unchanged** (hard rule CLAUDE.md §3.7) —
honours the user's continent/common-prefix prefs, caps at 12 chars, appends a
rare `/p` or `/m` (~1 in 8). A call therefore always fits one 12-box row.
Training point: the mode punishes keying ahead of what was actually revealed.

## 3. Prompt (lobby choice: Display / Sound)

- **Display**: the new character is shown big, and stays until the first
  answer character of the round arrives.
- **Sound**: played once, blocking, via `MorseCwEngine` at the player's live
  keyer speed (wpm=0) with the Echo-Trainer half-tone pitch shift (same as Fox
  Hunt's clue). **No replay** — in Characters mode the tolerated error covers a
  mishearing; in Call Signs mode that's the game.

## 4. UI

304×170 landscape 8-bpp game sprite (shared game infra). HUD as in
Trailblazer (title + wpm/vol, yellow on the encoder's current owner).

Box row: **12 boxes per row** (20 px boxes, 25 px pitch), up to 4 rows =
**48-char cap** (a "Perfect!" game; unreachable in practice). States: grey
outline = pending, **yellow frame = current position** (you can't lose your
place), green fill = keyed correctly (**kept empty** — the row must not become
a crib sheet), red fill + correct char = error. Row fully coloured → short
pause → chain grows → row resets grey. Completed call: shown in full
(green, letters) for a moment. Game over: full reveal — every box shows its
character, fatal position red; in Call Signs mode the unrevealed rest of the
call is shown dimmed.

Lobby (grammar precedent: Morsel's lobby + UX_CONVENTIONS §12): encoder turn
changes the **highlighted** setting; FN short press highlights the next
setting (Mode → Prompt → Koch, Koch skipped in Call Signs mode); black click =
high scores; paddle = start; black long = exit. Mode/Prompt persisted
immediately (byte `mcopt`). In play: encoder = speed, FN toggles to volume
(§2 value-cores); black long = exit. End-of-game flow mirrors MorseGridScore:
reveal (`Click: High Scores / Long press: Exit`) → table (`press to
continue` → **lobby**, not straight into a round — starting a round must stay
a paddle gesture).

## 5. Persistence

NVS namespace `gridgame` (reused per CLAUDE.md §4 — no new namespace), keys:

- `mc` / `mcv` — Characters high-score table + version stamp
- `mcc` / `mccv` — Call Signs table + version stamp (separate tables: the two
  modes score incomparable things)
- `mcopt` — lobby settings byte (bit0 = Call Signs mode, bit1 = Sound prompt)

Tables are 7 rows of `ChainScore {primary, secondary, errs, koch, prompt}`,
serialised verbatim, version-stamped (`MC_HI_VER`), rank by primary then
secondary. **Reset Scores** (MorsePreferences.cpp) removes the four
score keys but keeps `mcopt` — same settings-survive-reset intent as Morsel's
`wlen` (the previous blanket `p.clear()` on `gridgame` was narrowed to
targeted removes for this).

## 6. Names considered

**Memory Chain** (chosen). Rejected: *Chainletters / Chain Letters*
(spam/Kettenbrief connotation), *Letter Chain(s)* (flat), *Echo Chain*,
*Daisy Chain*. German manual introduces it via "Kofferpacken".

#ifndef MORSEGRIDENGINE_H_
#define MORSEGRIDENGINE_H_

/******************************************************************************************************************************
 *  MorseGridEngine — shared grid+path engine for the grid-maze games (Trailblazer, Fox Hunt).
 *
 *  Generates a fixed-size grid of Koch-filtered characters with a hidden,
 *  self-avoiding path from the left edge to the right edge, and renders it
 *  (grid, travelled trail, direction legend). Both games walk the same path
 *  forward one cell at a time; they differ only in how a step is triggered
 *  (Trailblazer: key the visible next letter; Fox Hunt: key the direction
 *  toward a heard letter) — that per-game input/scoring logic is NOT here.
 *
 *  Design and validation history: devdocs/grid-games/CONCEPT.md. The logic
 *  here was first proven on real M32 Pocket hardware via a throwaway layout
 *  prototype (since removed, once Trailblazer and Fox Hunt superseded it).
 *****************************************************************************************************************************/

#ifdef CONFIG_CW_GAME

#include <LovyanGFX.hpp>

// Grid shape is fixed (concept doc §2.2 — confirmed on-device, not a
// player-adjustable difficulty knob). Legend band height is exposed so a
// caller can size its own HUD/board split; screen width and the grid's
// internal horizontal margin are not exposed — every game on this hardware
// draws into the same fixed 304-wide landscape sprite (MorseGameMode).
#define GRIDENG_COLS       12
#define GRIDENG_ROWS        4
#define GRIDENG_LEGEND_H   26

namespace MorseGridEngine {

  // Compass direction, indexed to match the canonical N/E/S/W substitution
  // order (concept doc §4.3).
  enum Dir : uint8_t { DIR_N = 0, DIR_S, DIR_W, DIR_E };

  // One legend slot: the key letter assigned to a direction, whether it's a
  // Koch-set substitute for the canonical compass letter, and (if so) which
  // canonical letter it stands in for.
  struct DirInfo {
    char ltr;
    bool substituted;
    char canonical;
  };

  // Generate a fresh grid + hidden path, filling from the current
  // koch.getCharSet() and using the ambient random() stream. Resets the
  // current position to the start of the path.
  void generate();

  // Multiplayer maze transfer: the server generate()s once and broadcasts the
  // serialised maze; every client rebuilds the identical one via importState().
  // The maze itself travels (not a seed + lesson): koch.getCharSet() depends on
  // the Koch *sequence* preference (M32 native / LCWO / CW Academy / LICW /
  // custom) as well as the lesson, so seed-based regeneration would silently
  // produce different grids on devices with different sequence settings — the
  // same reason Morsel broadcasts its actual words rather than pool indices.
  // Worst-case size: 1 + GRIDENG_CELLS + GRIDENG_CELLS = 97 bytes.
  #define GRIDENG_STATE_MAX (1 + 2 * GRIDENG_COLS * GRIDENG_ROWS)
  int  exportState(uint8_t *buf, int maxLen);   // bytes written, or 0 if maxLen too small
  bool importState(const uint8_t *buf, int len); // validates; resets position to start

  int  pathLength();
  int  currentIndex();
  bool atEnd();                    // currentIndex() == pathLength() - 1

  // Jumps to an arbitrary path index, clamped to [0, pathLength()-1]. Real
  // gameplay only ever moves forward one step at a time (advance(), below), so
  // this currently has no caller — kept as part of a coherent position API for
  // diagnostic/test use (the removed layout prototype used it to scrub freely).
  void setCurrentIndex(int idx);

  // The character at the next path cell — what Trailblazer highlights and
  // Fox Hunt plays in CW. Undefined if atEnd().
  char nextChar();

  // Assigns the four direction legend slots against the current Koch set
  // (canonical N/E/S/W letter if already learned, otherwise the first
  // not-yet-assigned learned letter). Recompute whenever the Koch lesson
  // could have changed; cheap.
  void directionLegend(DirInfo out[4]);

  // Does moving one step in direction d from the current cell land on the
  // next path cell? Fox Hunt calls this after mapping a keyed/decoded letter
  // back to a direction via directionLegend().
  bool directionMatchesNext(Dir d);

  // Advances the current position to the next path cell (call on a correct
  // answer). No-op if already atEnd().
  void advance();

  // Draws the grid, travelled trail, current-position token, and start/end
  // markers into the vertical band [top, bottom) of the given sprite.
  // highlightNext draws the Trailblazer-style highlight box on the next path
  // cell; Fox Hunt passes false (its next cell is audio-revealed only).
  // trailColor is a GamePaletteIndex value (see GamePalette.h) for the walked
  // segment of the trail. Only the walked segment is ever drawn — the path
  // ahead is never previewed (concept doc §2.3); this matters most for Fox
  // Hunt, where a visible path would let a player skip decoding entirely.
  //
  // Neither this nor drawLegend() clears the sprite first — each draws only
  // its own band's content, so the caller composes a frame by clearing the
  // canvas once, then drawing its own HUD plus these calls, then pushing.
  // (The prototype's original single gpDraw() cleared once because it drew
  // the whole screen itself; that no longer holds now that the board, the
  // legend, and each game's own HUD are independently callable pieces.)
  //
  // Only ever draws with solid fills (fillRect/fillCircle/fillRoundRect) —
  // deliberately never drawWideLine/drawSmoothLine, which crash on an 8bpp
  // indexed sprite (their antialiased caps blend in RGB space, meaningless
  // for a palette index — confirmed by an on-device crash+backtrace during
  // this engine's own prototyping). Do not "simplify" the trail back to a
  // line primitive.
  void drawBoard(LGFX_Sprite *canvas, int top, int bottom, bool highlightNext,
                 uint8_t trailColor);

  // Draws the always-on N/E/S/W legend strip into [top, top+height). Same
  // no-clear contract as drawBoard() above.
  void drawLegend(LGFX_Sprite *canvas, int top, int height);

} // namespace MorseGridEngine

#endif  // CONFIG_CW_GAME
#endif  // MORSEGRIDENGINE_H_

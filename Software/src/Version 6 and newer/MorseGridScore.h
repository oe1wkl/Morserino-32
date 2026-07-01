#ifndef MORSEGRIDSCORE_H_
#define MORSEGRIDSCORE_H_

/******************************************************************************************************************************
 *  MorseGridScore — shared scoring + high-score persistence + end-of-game UI
 *  for the grid-maze games (Trailblazer, Fox Hunt).
 *
 *  Score is effective speed in characters per minute (cpm): the path length
 *  (correct entries) over the adjusted time (elapsed + a penalty per wrong
 *  entry). Higher is better — this normalises for the games' random maze
 *  lengths so scores are comparable. Each game keeps its own high-score table
 *  in the "gridgame" NVS namespace. See devdocs/grid-games/CONCEPT.md §5/§7.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

#include <LovyanGFX.hpp>

namespace MorseGridScore {

  enum Game   : uint8_t { TRAILBLAZER = 0, FOXHUNT = 1 };
  enum Action : uint8_t { PLAY_AGAIN, EXIT };

  // Persisted per row. Layout is serialised verbatim to NVS — bump the
  // version stamp in the .cpp if this ever changes.
  struct GridScore {
    uint32_t elapsedMs;   // raw solve time (shown on the results screen)
    uint16_t cpm;         // effective chars/min — the ranking key (higher better)
    uint16_t wrong;       // wrong entries
    uint8_t  steps;       // path length (moves = correct entries)
    uint8_t  koch;        // Koch lesson played at
  };

  // Compute cpm from the just-finished maze, insert into game g's table if it
  // qualifies, persist on qualification, and return the 0-based rank (or -1
  // if it didn't make the table). Fills `out` with the played score for the
  // results screen regardless of rank.
  int record(Game g, unsigned long elapsedMs, int steps, int wrong, int koch, GridScore &out);

  // Blocking end-of-game UI: results screen -> (paddle or black short-click)
  // high-score table -> (press) returns PLAY_AGAIN; a black long-press on
  // either screen returns EXIT. `title` is the game name shown as the header.
  Action showResultsAndHiscores(LGFX_Sprite *canvas, const char *title, Game g,
                                const GridScore &sc, int rank);

  // Blocking standalone high-score table viewer (no game just played) — for
  // opening the table from a game's ready screen. Any press/paddle returns.
  void viewHiscores(LGFX_Sprite *canvas, Game g);

} // namespace MorseGridScore

#endif  // CONFIG_CW_GAME
#endif  // MORSEGRIDSCORE_H_

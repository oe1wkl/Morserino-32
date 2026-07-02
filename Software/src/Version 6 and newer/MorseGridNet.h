#ifndef MORSEGRIDNET_H_
#define MORSEGRIDNET_H_

/******************************************************************************************************************************
 *  MorseGridNet — shared multiplayer (ESP-NOW) for the grid-maze games (Trailblazer, Fox Hunt).
 *
 *  Same-maze race, mirroring Morsel's proven netcode: one device is the
 *  server (picks the Koch lesson, generates the maze, broadcasts it), the
 *  others join in a lobby; on start every device plays the IDENTICAL maze
 *  and reports its result (time + wrong entries) when it finishes; the
 *  server ranks all finishers by effective CPM (MorseGridScore::cpm — the
 *  same metric as the solo high-score tables) and broadcasts the table.
 *  Pure broadcast, no pairing; protocol magic "GRD". All packets carry the
 *  game id, so a Trailblazer session and a Fox Hunt session don't mix.
 *
 *  Both games share this whole flow (mode select -> multiplayer lobby ->
 *  ranking screen); only the title string and the game id differ.
 *  Multiplayer results are transient — never written to the NVS tables.
 *  Design: devdocs/grid-games/CONCEPT.md §6.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

#include <LovyanGFX.hpp>
#include "MorseGridScore.h"     // Game enum + cpm()

namespace MorseGridNet {

  // True while a grid game's multiplayer flow owns the ESP-NOW broadcast
  // channel — onEspnowRecv() (m32_v6.ino) routes "GRD" packets to onRecv()
  // while this is set. Same pattern as MorseMorsel::morselNetMode.
  extern bool gridNetMode;

  // Called from onEspnowRecv() for broadcast packets while gridNetMode is
  // set. Callback context: copies into a ring and returns; parsing happens
  // in the game loop (pump), never here.
  void onRecv(const uint8_t* mac, const uint8_t* data, uint8_t len);

  // Blocking entry flow: mode select (Single player / Multiplayer) and, on
  // Multiplayer, the whole lobby (role pick -> server roster / client wait).
  // Black-knob long-press steps up one level and exits at the top (§12).
  //   FLOW_SINGLE  — player chose single player; run the normal ready screen.
  //   FLOW_MP_PLAY — a multiplayer game is starting NOW: the maze is already
  //                  in MorseGridEngine (generated on the server, imported on
  //                  a client) — reset your timers and start playing.
  //   FLOW_EXIT    — leave the game.
  // `resume` re-enters the multiplayer lobby directly (role kept) after a
  // ranking screen's "play again" instead of starting at mode select.
  enum Flow : uint8_t { FLOW_SINGLE, FLOW_MP_PLAY, FLOW_EXIT };
  Flow enter(LGFX_Sprite *canvas, const char *title, MorseGridScore::Game game,
             bool resume = false);

  // True while a multiplayer game is being played (between FLOW_MP_PLAY and
  // the ranking screen) — the games use it to pick the end-of-maze screen.
  bool isMpGame();

  // Blocking ranking screen for a just-finished multiplayer maze: reports the
  // local result (rebroadcast until acknowledged by inclusion in the ranking),
  // collects/ranks the field (server) or renders the received table (client).
  //   AGAIN — back to the multiplayer lobby (call enter(..., resume=true)).
  //   LEAVE — leave the game.
  enum After : uint8_t { AGAIN, LEAVE };
  After results(LGFX_Sprite *canvas, unsigned long elapsedMs, int wrong, int steps);

  // Stop routing, tear down ESP-NOW if this module brought it up, and restore
  // the player's real Koch lesson. Call once at the end of every grid game's
  // run(), before MorseGameMode::exit() — like Morsel, teardown must not lean
  // on menu_()'s entry-time teardown, which doesn't re-run between two game
  // launches. Safe to call when multiplayer was never entered.
  void teardown();

} // namespace MorseGridNet

#endif  // CONFIG_CW_GAME
#endif  // MORSEGRIDNET_H_

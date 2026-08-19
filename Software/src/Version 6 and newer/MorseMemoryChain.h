#ifndef MORSEMEMORYCHAIN_H_
#define MORSEMEMORYCHAIN_H_

/******************************************************************************************************************************
 *  Memory Chain — grow-a-chain keying memory game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  The device presents ONE new character per round (shown on screen or
 *  sounded in CW, per the lobby setting); the player keys the whole growing
 *  chain from memory, untimed, with a row of boxes as per-character feedback
 *  (grey = pending, green = correct, red = the correct character revealed on
 *  an error). Two content modes: Characters (random from the Koch lesson set,
 *  one tolerated error per round — the second error in a round ends the game)
 *  and Call Signs (one random call is the chain, revealed letter by letter,
 *  call after call, no tolerated error). See devdocs/memory-chain/CONCEPT.md.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

#include <ArduinoJson.h>   // JsonArray, for the protocol high-score export

namespace MorseMemoryChain {
    void run();

    // The two content modes, which keep separate high-score tables.
    enum Mode : uint8_t { CHARACTERS = 0, CALLSIGNS = 1 };

    // Protocol 1.4 (GET game/scores): append one mode's high-score rows to
    // `arr`. Empty rows are skipped, so an untouched table yields [].
    void exportHighScores(Mode m, JsonArray arr);

    // Drop the in-RAM cache so the next ensureLoaded() re-reads NVS. Must be
    // called whenever the stored tables are cleared behind this module's back
    // (Reset Scores in the preferences menu, PUT game/scores/clear) —
    // otherwise the sticky cache would show, and re-save, the scores that
    // were just wiped.
    void forgetHighScores();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEMEMORYCHAIN_H_

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

namespace MorseMemoryChain {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEMEMORYCHAIN_H_

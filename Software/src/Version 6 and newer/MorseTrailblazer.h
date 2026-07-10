#ifndef MORSETRAILBLAZER_H_
#define MORSETRAILBLAZER_H_

/******************************************************************************************************************************
 *  Trailblazer — "see & send" grid-maze game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  The player crosses a hidden path across MorseGridEngine's grid one cell at
 *  a time: the next path cell is highlighted, the player keys that visible
 *  letter, a correct answer advances and plays an OK tone, a wrong one plays
 *  an error tone and holds position. Reaching the far edge shows a brief
 *  "Solved!" and a new maze starts. See devdocs/grid-games/CONCEPT.md.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

namespace MorseTrailblazer {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSETRAILBLAZER_H_

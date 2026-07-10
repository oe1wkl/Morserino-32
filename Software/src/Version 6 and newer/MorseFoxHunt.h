#ifndef MORSEFOXHUNT_H_
#define MORSEFOXHUNT_H_

/******************************************************************************************************************************
 *  Fox Hunt — "hear & navigate" grid-maze game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  The player crosses a hidden path across MorseGridEngine's grid one cell at
 *  a time: the device plays the next path cell's letter in CW, the player
 *  identifies which of the (up to 4) neighbours holds it and keys the
 *  direction (N/E/S/W, Koch-substituted via the always-on legend) toward it.
 *  A correct direction advances and plays an OK tone plus the new next
 *  character; a wrong one plays an error tone and holds position. Idle play
 *  auto-repeats the clue; a short black-button click repeats it on demand.
 *  See devdocs/grid-games/CONCEPT.md.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

namespace MorseFoxHunt {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEFOXHUNT_H_

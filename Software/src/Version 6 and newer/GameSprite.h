#ifndef GAMESPRITE_H_
#define GAMESPRITE_H_

/******************************************************************************************************************************
 *  Shared game sprite for Morserino-32 Pocket games
 *  Both Morse Invaders and Fight the Pileup use a 170x320 16-bit sprite.
 *  Sharing one allocation avoids memory fragmentation.
 *****************************************************************************************************************************/

#ifdef CONFIG_CW_GAME

#include <LovyanGFX.hpp>

// Shared sprite — allocated once, reused by all games
extern LGFX_Sprite* gameCanvas;

// Call from any game's initDisplay(). Creates sprite if needed.
// Returns true if sprite is ready.
bool gameCanvasInit(lgfx::LGFX_Device* tft, int w, int h);

#endif
#endif

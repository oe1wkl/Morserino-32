#ifndef MORSEMORSEL_H_
#define MORSEMORSEL_H_

/******************************************************************************************************************************
 *  Morsel — Word-guessing game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  The player identifies a hidden word from two clues: one randomly revealed
 *  letter shown on screen, and the word played in Morse code. The CW starts
 *  fast and slows down each round. The player keys the full word every round;
 *  a long inter-word pause submits the guess, the <err> prosign (8 dits)
 *  deletes the last entered character. Letter boxes show green (correct
 *  position), red (wrong), grey (revealed slot keyed wrong).
 *
 *  Runs in landscape (304x170) because a row of up to six letter boxes plus
 *  status lines needs width more than height.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

// ---- Screen layout (304x170 landscape) ----
// 320 panel cols, but the sprite is trimmed by MORSE_GAMEMODE_SPRITE_TRIM
// (see MorseGameMode.cpp) to leave heap headroom. UI must lay out within
// MSL_SCREEN_W.
#define MSL_SCREEN_W       304
#define MSL_SCREEN_H       170

// ---- Colour palette ----
// The game sprite is 8-bpp indexed (see GamePalette.h): these names map to
// shared palette indices, not RGB565 values. The palette entries hold the
// exact RGB565 colours used before, so the on-screen result is unchanged.
#include "GamePalette.h"
#define MSL_BG             PAL_BG          // 0x0841 dark near-black
#define MSL_TEXT           PAL_WHITE       // 0xFFFF white — body text
#define MSL_DIM            PAL_LIGHTGREY   // 0xBDF7 light grey — secondary/hints
#define MSL_ACCENT         PAL_CYAN        // 0x07FF cyan — headings
#define MSL_HIGHLIGHT      PAL_YELLOW      // 0xFFE0 yellow — CW playing, warnings
#define MSL_RED            PAL_RED         // 0xF800 red — wrong letter
#define MSL_GREEN          PAL_GREEN       // 0x07E0 green — correct letter/position
#define MSL_GREY           PAL_MIDBLUE     // 0x528A grey — revealed slot keyed wrong
#define MSL_FRAME          PAL_MIDBLUE     // 0x528A mid-blue — decorative frames

// ---- Game states ----
enum MslState : uint8_t {
    MSL_INIT,
    MSL_MODESEL,        // choose single-player vs multiplayer
    MSL_LOBBY,          // single-player entry screen — press/key to start
    MSL_PLAYING,        // the game proper
    MSL_RESULTS,        // end-of-game score screen
    MSL_HISCORES,       // persistent high-score table
    MSL_MP_LOBBY,       // multiplayer: server/join role pick + roster
    MSL_MP_WAIT,        // multiplayer: armed / waiting (synced play lands in P2.2)
    MSL_EXIT
};

// ---- Public interface ----
namespace MorseMorsel {
    void run();

    // True only while Morsel multiplayer networking is active. Gates the
    // Morsel branch of the global ESP-NOW receive callback (m32_v6.ino) so
    // its broadcast packets are only inspected when a Morsel MP session is up.
    extern bool morselNetMode;

    // Called from onEspnowRecv() for broadcast packets while morselNetMode is
    // set. Copies the packet into Morsel's lock-free receive ring; the game
    // loop drains it. Runs in the ESP-NOW RX task context, so it only does a
    // bounded memcpy and never touches game/UI state directly.
    void mslNetOnRecv(const uint8_t* mac, const uint8_t* data, uint8_t len);

    // Call once at boot, after MorseOutput::initDisplay() and
    // MorseGameMode::warmup(). Pre-grows the module's static Arduino String
    // buffers so their first use during gameplay doesn't allocate small
    // persistent buffers next to the freshly-allocated game sprite.
    void warmup();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEMORSEL_H_

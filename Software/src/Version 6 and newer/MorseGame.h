#ifndef MORSEGAME_H_
#define MORSEGAME_H_

/******************************************************************************************************************************
 *  Morse Invaders — Game module for Morserino-32 Pocket
 *  Copyright (C) 2025  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *****************************************************************************************************************************/

#include <Arduino.h>

// Game character buffer — always available so displayDecodedMorse can use them
extern bool gameMode;
extern char gameCharBuffer;

// QSO-Bot character intercept. When true, the firmware decoder still
// writes the decoded char to the standard scroll display (so the user
// sees what they keyed), AND copies it into gameCharBuffer for the bot
// to consume. gameMode (used by the Pocket games) bypasses the display
// entirely; qsoBotMode does not. The two are mutually exclusive.
extern bool qsoBotMode;

#ifdef CONFIG_CW_GAME

#include <Preferences.h>

// ---- Game constants ----
#define GAME_MAX_INVADERS   6
#define GAME_NUM_LANES      4
#define GAME_FIELD_TOP      24
// Field bottom and keying strip both shifted up by 16 px (previously 278/280)
// so the keying display has its full 40 px height inside the trimmed sprite
// (304 px tall instead of the panel's native 320 — see GAME_SCREEN_H below).
// The playfield is 16 px shorter as a result.
#define GAME_FIELD_BOTTOM   262
#define GAME_KEYING_Y       264
#define GAME_INVADER_W      38
#define GAME_INVADER_H      32
#define GAME_LANE_W         42
#define GAME_LANE_MARGIN    1
#define GAME_HIGH_SCORES    5
#define GAME_DESTROYS_PER_LEVEL 10  // might increase this for finmal release

#define GAME_SCREEN_W       170
// 320 panel rows, but the sprite is trimmed by MORSE_GAMEMODE_SPRITE_TRIM
// (see MorseGameMode.cpp) to leave heap headroom for ESP-NOW/BT/WiFi.
// The bottom 16 px of the panel are not drawn (stay the black fill from
// game-mode entry); UI must lay out within GAME_SCREEN_H.
#define GAME_SCREEN_H       304

// ---- Colour palette ----
// The game sprite is 8-bpp indexed (see GamePalette.h): these names map to
// shared palette indices, not RGB565 values. The palette entries hold the
// exact RGB565 colours used before, so the on-screen result is unchanged.
#include "GamePalette.h"
#define GC_BG           PAL_BG            // 0x0841
#define GC_LETTERS      PAL_INV_LETTERS   // 0x0400 dark green
#define GC_NUMBERS      PAL_INV_NUMBERS   // 0x8400 dark yellow
#define GC_PUNCT        PAL_INV_PUNCT     // 0x8200 dark orange
#define GC_PROSIGN      PAL_INV_PROSIGN   // 0x8010 dark magenta
#define GC_HUD_TEXT     PAL_WHITE         // 0xFFFF
#define GC_LIVES        PAL_RED           // 0xF800
#define GC_SCORE        PAL_CYAN          // 0x07FF
#define GC_LEVELUP      PAL_YELLOW        // 0xFFE0
#define GC_GAMEOVER     PAL_RED           // 0xF800
#define GC_MISS         PAL_GREY          // 0x7BEF
#define GC_BAND         PAL_DARKGREY      // 0x2104 HUD / keying-zone band

// ---- Game states ----
enum GameStateEnum : uint8_t {
    GAME_INIT, GAME_MENU, GAME_COUNTDOWN, GAME_PLAYING,
    GAME_PAUSED, GAME_LEVEL_UP, GAME_OVER, GAME_EXIT
};

// ---- Invader ----
struct GameInvader {
    char     character;
    uint8_t  lane;
    float    y;
    float    speed;
    uint16_t color;        // fill palette index (PAL_INV_*)
    uint16_t borderColor;  // dimmed outline palette index (PAL_INV_*_B)
    bool     active;
    uint8_t  explodeFrame;
};

// ---- Game data ----
struct GameData {
    GameStateEnum state;
    uint32_t score;
    uint8_t  kochLesson;
    uint8_t  subLevel;
    uint8_t  startSubLevel;
    uint8_t  lives;
    uint16_t streak;
    uint16_t totalHits;
    uint16_t totalMisses;
    uint16_t totalDropped;
    uint32_t frameCount;
    uint16_t spawnInterval;
    uint16_t spawnCounter;
    float    baseSpeed;
    uint8_t  maxInvaders;
    uint8_t  destroysThisLevel;
    uint8_t  wpm;
    const char* kochSequence;
    uint8_t  kochPoolSize;
    GameInvader invaders[GAME_MAX_INVADERS];
};

// ---- High score ----
struct GameHighScore {
    uint32_t score;
    uint8_t  kochLesson;
    uint8_t  subLevel;
    uint8_t  wpm;
};

// ---- Public interface ----
namespace MorseGame {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEGAME_H_
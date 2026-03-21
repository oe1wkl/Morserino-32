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

#ifdef CONFIG_CW_GAME

#include <Preferences.h>

// ---- Game constants ----
#define GAME_MAX_INVADERS   8
#define GAME_NUM_LANES      6
#define GAME_FIELD_TOP      24
#define GAME_FIELD_BOTTOM   278
#define GAME_KEYING_Y       280
#define GAME_INVADER_W      22
#define GAME_INVADER_H      26
#define GAME_LANE_W         26
#define GAME_LANE_MARGIN    7
#define GAME_HIGH_SCORES    5
#define GAME_DESTROYS_PER_LEVEL 10

#define GAME_SCREEN_W       170
#define GAME_SCREEN_H       320

// ---- Colour palette (RGB565) ----
#define GC_BG           0x0841
#define GC_LETTERS      0x0400    // dark green
#define GC_NUMBERS      0x8400    // dark yellow
#define GC_PUNCT        0x8200    // dark orange
#define GC_PROSIGN      0x8010    // dark magenta
#define GC_HUD_TEXT     0xFFFF
#define GC_LIVES        0xF800
#define GC_SCORE        0x07FF
#define GC_LEVELUP      0xFFE0
#define GC_GAMEOVER     0xF800
#define GC_MISS         0x7BEF

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
    uint16_t color;
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
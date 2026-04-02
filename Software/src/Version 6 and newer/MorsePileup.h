#ifndef MORSEPILEUP_H_
#define MORSEPILEUP_H_

/******************************************************************************************************************************
 *  Fight the Pileup — Multiplayer CW game for Morserino-32 Pocket
 *  Copyright (C) 2025  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *****************************************************************************************************************************/

#include <Arduino.h>

// Pileup mode flag — defined OUTSIDE #ifdef so MOPP receive can always check it
extern bool pileupMode;
extern String pileupRxText;         // decoded text from received MOPP game packet
extern volatile bool pileupRxReady; // true when a new game packet is available


#ifdef CONFIG_CW_GAME

// ---- Constants ----
#define FTP_MAX_PLAYERS     8
#define FTP_MAX_ATTACKS     4
#define FTP_MAX_IDENT_LEN   8
#define FTP_MAX_CALL_LEN    12
#define FTP_MAGIC           "/ftp/"
#define FTP_MAGIC_LEN       5
#define FTP_BEACON_INTERVAL 5000
#define FTP_BEACON_TIMEOUT  15000
#define FTP_BEACON_REMOVE   30000
#define FTP_GAMEOVER_TIMEOUT 20000

// Pileup caller constants
#define FTP_MAX_CALLERS     6
#define FTP_NUM_DIFFICULTIES 4

// Code challenge constants
#define FTP_CODE_LEN_ENTRY  4           // initial entry code length
#define FTP_CODE_LEN_MAX    8           // revival codes scale up to this

// Scoring
#define FTP_SCORE_CORRECT   100         // base score per correct caller
#define FTP_SCORE_STREAK_BONUS 10       // extra per streak level
#define FTP_SCORE_WRONG     -50         // penalty for wrong call
#define FTP_SCORE_TIMEOUT   -25         // penalty when caller times out

// ---- Difficulty preset ----
struct FtpDifficulty {
    const char* label;
    uint16_t callerTimeout;     // ms before a caller gives up
    uint16_t spawnMax;          // max ms between spawns (easy pace)
    uint16_t spawnMin;          // min ms between spawns (at high streak)
    uint8_t  initialCallers;    // callers on screen at start
    uint8_t  dropsPerLife;      // dropped callers before losing a life
};

// ---- Game states ----
enum FtpGameState : uint8_t {
    FTP_NAME_ENTRY,
    FTP_LOBBY,
    FTP_CODE_CHALLENGE,
    FTP_PILEUP,
    FTP_ELIMINATED,
    FTP_GAME_OVER,
    FTP_EXIT
};

// ---- Pileup caller (bot or from network attack) ----
struct FtpCaller {
    char     call[FTP_MAX_CALL_LEN + 1];
    unsigned long spawnTime;
    bool     active;
    bool     fromNetwork;           // true = attack from another player
    uint8_t  senderIdx;             // index into players[] if fromNetwork
    float    progress;              // 0.0 (just arrived) to 1.0 (timed out)
};

// ---- Player roster entry ----
struct FtpPlayer {
    char     ident[FTP_MAX_IDENT_LEN + 1];
    uint8_t  lives;
    uint32_t score;
    unsigned long lastBeacon;
    bool     inPileup;
    bool     eliminated;
    bool     active;
};

// ---- Game data ----
struct FtpGameData {
    FtpGameState state;
    char     playerCall[FTP_MAX_IDENT_LEN + 1];
    char     playerName[FTP_MAX_IDENT_LEN + 1];
    bool     useCallsign;
    uint32_t score;
    uint8_t  lives;
    uint16_t streak;
    uint16_t bestStreak;
    uint16_t totalAttacksSent;
    uint16_t totalBlocked;
    uint16_t totalDropped;
    uint8_t  wpm;
    uint8_t  eliminationCount;
    bool     singlePlayer;          // true = no ESPNow, bot callers only
    uint8_t  difficulty;            // 0..FTP_NUM_DIFFICULTIES-1

    // Player roster
    FtpPlayer players[FTP_MAX_PLAYERS];
    uint8_t  playerCount;

    // Pileup callers
    FtpCaller callers[FTP_MAX_CALLERS];
    uint8_t  callerCount;
    unsigned long lastSpawn;
    unsigned long spawnInterval;    // current interval (shortens with streak)

    // CW input buffer for player's keyed response
    char     inputBuf[FTP_MAX_CALL_LEN + 2];
    uint8_t  inputPos;
    unsigned long lastCharTime;     // for word-gap timeout (auto-submit)

    // Code challenge
    char     challengeCode[FTP_CODE_LEN_MAX + 1];
    uint8_t  challengeLen;
    uint8_t  challengePos;          // how many chars correctly keyed so far

    // Timers
    unsigned long lastBeaconSent;
    unsigned long lastActivity;
};

// ---- Public interface ----
namespace MorsePileup {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSEPILEUP_H_
/******************************************************************************************************************************
 *  Fight the Pileup — Multiplayer CW game for Morserino-32 Pocket
 *  Copyright (C) 2025  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *****************************************************************************************************************************/

#include "MorsePileup.h"

bool pileupMode = false;
String pileupRxText = "";
volatile bool pileupRxReady = false;

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "morsedefs.h"
#include "ClickButton.h"
#include "DisplayWrapper.h"
#include <LovyanGFX.hpp>
#include <ESP32Encoder.h>
#include <Preferences.h>

extern int checkEncoder();
extern void serialEvent();
extern boolean checkPaddles();
extern boolean doPaddleIambic(boolean, boolean);
extern boolean leftKey, rightKey;
extern DisplayWrapper display;
extern ESP32Encoder rotaryEncoder;
extern const int PinCLK;
extern const int PinDT;
extern bool EspNowIsActive;
extern void updateTimings();
extern void clearPaddleLatches();
extern KEYERSTATES keyerState;
extern unsigned int ditLength;
extern unsigned int dahLength;
extern unsigned int interCharacterSpace;
extern unsigned int interWordSpace;
extern bool gameMode;
extern char gameCharBuffer;
extern void keyOut(boolean on, boolean fromHere, int f, int volume);

// Callsign & CW generation — from main .ino
extern String getRandomCall(int maxLength);
extern String generateCWword(const String& symbols);

static LGFX_Sprite* canvas = nullptr;
static FtpGameData   ftp;

static const char ENTRY_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
static const int  ENTRY_CHARS_LEN = 37;
static const int  ENTRY_CHARS_LEN_NAME = 36;

// Code challenge character pool
static const char CODE_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const int  CODE_CHARS_LEN = 36;

#define FTP_W  170
#define FTP_H  320
#define FTP_BG     0x0841
#define FTP_TEXT   0xFFFF
#define FTP_ACCENT 0x07FF
#define FTP_OK     0x07E0
#define FTP_WARN   0xF800
#define FTP_TITLE  0xFFE0
#define FTP_DIM    0x7BEF
#define FTP_INPUT  0x001F      // blue for input text
#define FTP_ATTACK 0xF81F      // magenta for attack prompt

// Difficulty presets
//                              label       timeout  spawnMax spawnMin initCal dropsPerLife
static const FtpDifficulty difficulties[FTP_NUM_DIFFICULTIES] = {
    { "EASY",      45000,  12000,    5000,    1,      5 },
    { "NORMAL",    30000,   8000,    3000,    1,      4 },
    { "HARD",      20000,   5000,    2000,    2,      3 },
    { "EXPERT",    12000,   3500,    1500,    3,      2 },
};

static const FtpDifficulty& diff() { return difficulties[ftp.difficulty]; }

// Word-gap timeout: 7 dit-lengths of silence → submit input
#define FTP_WORDGAP_FACTOR 7

// CW playback: number of plays before showing text hint
#define FTP_PLAYS_BEFORE_REVEAL 3

// Attack CW pitch offset (semitones * ratio from sidetone)
// We use a pitch ~4 semitones below the sidetone
#define FTP_ATTACK_PITCH_RATIO_NUM 15
#define FTP_ATTACK_PITCH_RATIO_DEN 18

// Visual feedback flash
#define FTP_FLASH_DURATION 400  // ms
static uint16_t      flashColor = 0;
static unsigned long flashStart = 0;
static char          flashText[20] = "";

static void triggerFlash(uint16_t color, const char* text) {
    flashColor = color;
    flashStart = millis();
    strncpy(flashText, text, sizeof(flashText) - 1);
    flashText[sizeof(flashText) - 1] = '\0';
}

static bool isFlashing() {
    return flashStart > 0 && (millis() - flashStart < FTP_FLASH_DURATION);
}

//=== Forward declarations ===

static void   initGameData();
static void   loadPlayerIdentity();
static void   savePlayerIdentity();
static void   stateNameEntry();
static void   stateLobby();
static void   stateCodeChallenge();
static void   statePileup();
static void   stateGameOver();
static void   sendBeacon();
static void   checkReceivedMessages();
static void   updateRoster();
static int    findPlayer(const char* ident);
static int    addPlayer(const char* ident);
static int    countActivePlayers();
static const char* getPlayerIdent();
static void   pushFrame();
static void   drawCentredText(int y, const char* text, uint16_t color,
                              const lgfx::IFont* font = nullptr);

// Pileup gameplay
static char   pollKeyedChar();
static void   clearInput();
static void   cleanupKeyer();
static void   addScore(int32_t points);
static void   generateCode(char* buf, int len);
static void   stopFtpSound();

// Sound effects
static void   playSoundCorrect();
static void   playSoundWrong();
static void   playSoundTimeout();
static void   playSoundLifeLost();
static void   playSoundLevelUp();


//=== Sound effect system (non-blocking, same pattern as Morse Invaders) ===

struct FtpSoundNote {
    uint16_t freq;
    uint16_t duration;
};

#define FTP_MAX_SOUND_NOTES 6

static FtpSoundNote  ftpSoundSeq[FTP_MAX_SOUND_NOTES];
static uint8_t       ftpSoundCount = 0;
static uint8_t       ftpSoundIdx   = 0;
static unsigned long ftpSoundStart = 0;
static bool          ftpSoundPlaying = false;

static void startFtpSound(const FtpSoundNote* notes, uint8_t count) {
    for (int i = 0; i < count && i < FTP_MAX_SOUND_NOTES; i++)
        ftpSoundSeq[i] = notes[i];
    ftpSoundCount = count;
    ftpSoundIdx   = 0;
    ftpSoundPlaying = true;
    ftpSoundStart = millis();
    MorseOutput::pwmTone(ftpSoundSeq[0].freq, MorsePreferences::sidetoneVolume, false);
}

static void updateFtpSound() {
    if (!ftpSoundPlaying) return;
    if (keyerState != IDLE_STATE) {
        ftpSoundPlaying = false;
        return;
    }
    if (millis() - ftpSoundStart >= ftpSoundSeq[ftpSoundIdx].duration) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        ftpSoundIdx++;
        if (ftpSoundIdx >= ftpSoundCount) {
            ftpSoundPlaying = false;
        } else {
            ftpSoundStart = millis();
            MorseOutput::pwmTone(ftpSoundSeq[ftpSoundIdx].freq,
                                 MorsePreferences::sidetoneVolume, false);
        }
    }
}

static void stopFtpSound() {
    if (ftpSoundPlaying) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        ftpSoundPlaying = false;
    }
}

static void playSoundCorrect() {
    if (keyerState != IDLE_STATE) return;
    static const FtpSoundNote s[] = {{1200, 15}, {1600, 15}};
    startFtpSound(s, 2);
}
static void playSoundWrong() {
    if (keyerState != IDLE_STATE) return;
    static const FtpSoundNote s[] = {{300, 25}};
    startFtpSound(s, 1);
}
static void playSoundTimeout() {
    if (keyerState != IDLE_STATE) return;
    static const FtpSoundNote s[] = {{400, 30}, {300, 30}};
    startFtpSound(s, 2);
}
static void playSoundLifeLost() {
    if (keyerState != IDLE_STATE) return;
    static const FtpSoundNote s[] = {{600, 30}, {400, 30}, {300, 40}};
    startFtpSound(s, 3);
}
static void playSoundLevelUp() {
    static const FtpSoundNote s[] = {{523, 30}, {659, 30}, {784, 30}, {1047, 40}};
    startFtpSound(s, 4);
}


//=== Non-blocking CW player for attack playback ===
//
// Plays a CW element string ("1201200120...") at a different pitch from the
// sidetone. Uses pwmTone/pwmNoTone directly — pauses automatically when the
// keyer is active (keyerState != IDLE_STATE) so the player's sidetone takes
// priority.

struct FtpCwPlayer {
    char     elements[128];     // dit/dah/space element string from generateCWword
    int      pos;               // current position in elements
    int      len;               // total length
    bool     playing;           // currently active
    bool     toneOn;            // is our tone currently sounding
    bool     paused;            // paused because keyer is active
    unsigned long timer;        // next state transition time
    int      pitch;             // our playback pitch (Hz)
    uint8_t  playCount;         // how many times we've played through
};

static FtpCwPlayer cwPlayer;

static int getAttackPitch() {
    int basePitch = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    return basePitch * FTP_ATTACK_PITCH_RATIO_NUM / FTP_ATTACK_PITCH_RATIO_DEN;
}

static void cwPlayerStart(const char* callsign) {
    // Convert callsign to lowercase for generateCWword
    String lower = callsign;
    lower.toLowerCase();
    String cw = generateCWword(lower);

    strncpy(cwPlayer.elements, cw.c_str(), sizeof(cwPlayer.elements) - 1);
    cwPlayer.elements[sizeof(cwPlayer.elements) - 1] = '\0';
    cwPlayer.len = strlen(cwPlayer.elements);
    cwPlayer.pos = 0;
    cwPlayer.playing = true;
    cwPlayer.toneOn = false;
    cwPlayer.paused = false;
    cwPlayer.timer = millis();
    cwPlayer.pitch = getAttackPitch();
    cwPlayer.playCount = 0;
}

static void cwPlayerStop() {
    if (cwPlayer.toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cwPlayer.toneOn = false;
    }
    cwPlayer.playing = false;
}

// Call from tight loop. Non-blocking.
static void cwPlayerUpdate() {
    if (!cwPlayer.playing) return;

    // Mute while:
    // - keyer is producing a dit/dah (keyerState != IDLE_STATE)
    // - player has started typing a response (inputPos > 0)
    // - sound effects are playing
    bool shouldMute = (keyerState != IDLE_STATE) ||
                      (ftp.inputPos > 0) ||
                      ftpSoundPlaying;

    if (shouldMute) {
        if (cwPlayer.toneOn) {
            // Stop our tone ONCE when transitioning to muted state
            MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
            cwPlayer.toneOn = false;
        }
        cwPlayer.paused = true;
        cwPlayer.timer = millis() + 100;
        return;
    }

    // Resume from pause — add a gap so tones don't collide
    if (cwPlayer.paused) {
        cwPlayer.paused = false;
        cwPlayer.timer = millis() + ditLength * 2;  // comfortable gap
        return;
    }

    if (millis() < cwPlayer.timer) return;  // waiting

    // If tone is on, turn it off (end of dit/dah)
    if (cwPlayer.toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cwPlayer.toneOn = false;
        cwPlayer.timer = millis() + ditLength - 7;  // inter-element space, compensate for pwmNoTone delay
        return;
    }

    // Advance to next element
    if (cwPlayer.pos >= cwPlayer.len) {
        // Finished one play-through
        cwPlayer.playCount++;
        cwPlayer.pos = 0;
        // Inter-word gap before replay
        cwPlayer.timer = millis() + interWordSpace + ditLength * 3;
        return;
    }

    char elem = cwPlayer.elements[cwPlayer.pos++];

    switch (elem) {
        case '1':  // dit
            MorseOutput::pwmTone(cwPlayer.pitch, MorsePreferences::sidetoneVolume, false);
            cwPlayer.toneOn = true;
            cwPlayer.timer = millis() + ditLength - 7;  // compensate for pwmTone delay
            break;
        case '2':  // dah
            MorseOutput::pwmTone(cwPlayer.pitch, MorsePreferences::sidetoneVolume, false);
            cwPlayer.toneOn = true;
            cwPlayer.timer = millis() + dahLength - 7;  // compensate for pwmTone delay
            break;
        case '0':  // inter-character space (3 dit-lengths total, 1 already from inter-element)
            cwPlayer.timer = millis() + interCharacterSpace;
            break;
        default:
            break;
    }
}


//=== Drawing helpers ===

static void pushFrame() {
    display.pushGameFrame();
}

static void drawCentredText(int y, const char* text, uint16_t color,
                            const lgfx::IFont* font) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, FTP_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(text, FTP_W / 2, y);
    canvas->setTextDatum(lgfx::top_left);
}

static void drawFlash() {
    if (!isFlashing()) return;
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentredText(252, flashText, flashColor);
}

//=== Player identity — NVS ===

static void loadPlayerIdentity() {
    Preferences pref;
    pref.begin("morserino", true);
    String call = pref.getString("playerCall", "");
    String name = pref.getString("playerName", "");
    pref.end();
    strncpy(ftp.playerCall, call.c_str(), FTP_MAX_IDENT_LEN);
    ftp.playerCall[FTP_MAX_IDENT_LEN] = '\0';
    strncpy(ftp.playerName, name.c_str(), FTP_MAX_IDENT_LEN);
    ftp.playerName[FTP_MAX_IDENT_LEN] = '\0';
}

static void savePlayerIdentity() {
    Preferences pref;
    pref.begin("morserino", false);
    pref.putString("playerCall", ftp.playerCall);
    pref.putString("playerName", ftp.playerName);
    pref.end();
}

static const char* getPlayerIdent() {
    return ftp.useCallsign ? ftp.playerCall : ftp.playerName;
}

//=== Networking stubs — real networking in Phase 3 ===

static void sendBeacon() { ftp.lastBeaconSent = millis(); }
static void checkReceivedMessages() {}

//=== Player roster ===

static int findPlayer(const char* ident) {
    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
        if (ftp.players[i].active && strcmp(ftp.players[i].ident, ident) == 0) return i;
    return -1;
}

static int addPlayer(const char* ident) {
    for (int i = 0; i < FTP_MAX_PLAYERS; i++) {
        if (!ftp.players[i].active) {
            strncpy(ftp.players[i].ident, ident, FTP_MAX_IDENT_LEN);
            ftp.players[i].ident[FTP_MAX_IDENT_LEN] = '\0';
            ftp.players[i].active = true;
            ftp.players[i].inPileup = false;
            ftp.players[i].eliminated = false;
            ftp.players[i].lastBeacon = millis();
            ftp.players[i].lives = 3;
            ftp.players[i].score = 0;
            ftp.playerCount++;
            return i;
        }
    }
    return -1;
}

static void updateRoster() {
    unsigned long now = millis();
    for (int i = 0; i < FTP_MAX_PLAYERS; i++) {
        if (!ftp.players[i].active) continue;
        if (now - ftp.players[i].lastBeacon > FTP_BEACON_REMOVE) {
            ftp.players[i].active = false;
            if (ftp.playerCount > 0) ftp.playerCount--;
        }
    }
}

static int countActivePlayers() {
    int n = 0;
    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
        if (ftp.players[i].active) n++;
    return n;
}

//=== CW keyer input (same pattern as Morse Invaders) ===

static char pollKeyedChar() {
    checkPaddles();
    doPaddleIambic(leftKey, rightKey);
    char c = gameCharBuffer;
    if (c != 0) { gameCharBuffer = 0; return c; }
    return 0;
}

static void clearInput() {
    ftp.inputBuf[0] = '\0';
    ftp.inputPos = 0;
    ftp.lastCharTime = 0;
}

static void cleanupKeyer() {
    gameMode = false;
    keyerState = IDLE_STATE;
    clearPaddleLatches();
    gameCharBuffer = 0;
    keyOut(false, true, 0, 0);
    cwPlayerStop();
    stopFtpSound();
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
}

//=== Scoring ===

static void addScore(int32_t points) {
    int32_t newScore = (int32_t)ftp.score + points;
    ftp.score = (newScore < 0) ? 0 : (uint32_t)newScore;
}

//=== Code generation ===

static void generateCode(char* buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = CODE_CHARS[random(0, CODE_CHARS_LEN)];
    buf[len] = '\0';
}

//=== Attack queue management ===
//
// Attacks arrive and queue up. Only the bottom one (index 0) is "active":
// its CW is being played. The player must key it back to clear it.
// When cleared, the next one slides down.

static void clearAllCallers() {
    for (int i = 0; i < FTP_MAX_CALLERS; i++)
        ftp.callers[i].active = false;
    ftp.callerCount = 0;
}

static void spawnAttack() {
    // Find a free slot — fill from the end (queue grows upward on screen)
    int slot = -1;
    for (int i = FTP_MAX_CALLERS - 1; i >= 0; i--) {
        if (!ftp.callers[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    String call = getRandomCall(0);
    call.toUpperCase();

    // Avoid duplicates
    for (int retry = 0; retry < 5; retry++) {
        bool dup = false;
        for (int i = 0; i < FTP_MAX_CALLERS; i++) {
            if (ftp.callers[i].active && strcmp(ftp.callers[i].call, call.c_str()) == 0) {
                dup = true; break;
            }
        }
        if (!dup) break;
        call = getRandomCall(0);
        call.toUpperCase();
    }

    strncpy(ftp.callers[slot].call, call.c_str(), FTP_MAX_CALL_LEN);
    ftp.callers[slot].call[FTP_MAX_CALL_LEN] = '\0';
    ftp.callers[slot].spawnTime = millis();
    ftp.callers[slot].active = true;
    ftp.callers[slot].fromNetwork = false;
    ftp.callers[slot].senderIdx = 0;
    ftp.callers[slot].progress = 0.0f;
    ftp.callerCount++;
    ftp.lastSpawn = millis();
}

// Find the active attack with the earliest spawn time (= the one to defend)
static int findActiveAttack() {
    int best = -1;
    unsigned long earliest = 0xFFFFFFFF;
    for (int i = 0; i < FTP_MAX_CALLERS; i++) {
        if (!ftp.callers[i].active) continue;
        if (ftp.callers[i].spawnTime < earliest) {
            earliest = ftp.callers[i].spawnTime;
            best = i;
        }
    }
    return best;
}

static void removeAttack(int idx) {
    if (idx < 0 || idx >= FTP_MAX_CALLERS) return;
    ftp.callers[idx].active = false;
    if (ftp.callerCount > 0) ftp.callerCount--;
}

//=== Generate attack prompt (what the player sends to attack others) ===

static char attackPrompt[FTP_MAX_CALL_LEN + 1] = "";
static bool attackPromptActive = false;

static void generateAttackPrompt() {
    String call = getRandomCall(0);
    call.toUpperCase();
    strncpy(attackPrompt, call.c_str(), FTP_MAX_CALL_LEN);
    attackPrompt[FTP_MAX_CALL_LEN] = '\0';
    attackPromptActive = true;
}


//=== Game data init ===

static void initGameData() {
    ftp.state = FTP_LOBBY;
    ftp.useCallsign = (strlen(ftp.playerCall) > 0);
    ftp.score = 0;
    ftp.lives = 3;
    ftp.streak = 0;
    ftp.bestStreak = 0;
    ftp.totalAttacksSent = 0;
    ftp.totalBlocked = 0;
    ftp.totalDropped = 0;
    ftp.wpm = MorsePreferences::wpm;
    ftp.eliminationCount = 0;
    ftp.playerCount = 0;
    ftp.singlePlayer = false;
    ftp.lastBeaconSent = 0;
    ftp.lastActivity = millis();
    ftp.callerCount = 0;
    ftp.lastSpawn = 0;
    ftp.spawnInterval = diff().spawnMax;
    clearInput();
    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
        ftp.players[i].active = false;
    clearAllCallers();
    attackPromptActive = false;
    cwPlayer.playing = false;
    cwPlayer.toneOn = false;
}


//=== STATE: Name Entry ===

static void enterString(const char* prompt, char* result, int maxLen,
                        bool allowSlash) {
    int charMax = allowSlash ? ENTRY_CHARS_LEN : ENTRY_CHARS_LEN_NAME;
    int cursorPos = 0;
    int charIdx = 0;
    result[0] = '\0';

    while (true) {
        canvas->fillSprite(FTP_BG);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        drawCentredText(20, "FIGHT THE", FTP_ACCENT);
        drawCentredText(48, "PILEUP", FTP_ACCENT);

        canvas->setFont(&fonts::FreeSans9pt7b);
        drawCentredText(100, prompt, FTP_TEXT);

        char confirmedBuf[FTP_MAX_IDENT_LEN + 1];
        strncpy(confirmedBuf, result, cursorPos);
        confirmedBuf[cursorPos] = '\0';

        canvas->setFont(&fonts::FreeSansBold12pt7b);
        if (cursorPos > 0) {
            canvas->setTextColor(FTP_TEXT, FTP_BG);
            canvas->setTextDatum(lgfx::top_center);
            canvas->drawString(confirmedBuf, FTP_W / 2, 145);
            canvas->setTextDatum(lgfx::top_left);
        }

        char cursorBuf[4];
        snprintf(cursorBuf, sizeof(cursorBuf), "[%c]", ENTRY_CHARS[charIdx]);
        canvas->setFont(&fonts::FreeSansBold18pt7b);
        drawCentredText(175, cursorBuf, FTP_TITLE);

        canvas->setFont(&fonts::Font0);
        char infoBuf[20];
        snprintf(infoBuf, sizeof(infoBuf), "%d of %d chars", cursorPos, maxLen);
        drawCentredText(215, infoBuf, FTP_DIM);
        drawCentredText(235, "Encoder: select char", FTP_TEXT);
        drawCentredText(251, "Click: add character", FTP_TEXT);
        drawCentredText(267, "FN: delete last", FTP_TEXT);
        if (cursorPos >= 2)
            drawCentredText(283, "Long press: DONE", FTP_OK);
        else
            drawCentredText(283, "(need min 2 chars)", FTP_WARN);
        pushFrame();

        int enc = checkEncoder();
        if (enc) {
            charIdx = (charIdx + enc + charMax) % charMax;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == 1 && cursorPos < maxLen) {
            result[cursorPos] = ENTRY_CHARS[charIdx];
            cursorPos++;
            result[cursorPos] = '\0';
            charIdx = 0;
        }
        if (Buttons::modeButton.clicks == -1) {
            result[cursorPos] = '\0';
            if (cursorPos < 2) result[0] = '\0';
            return;
        }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1 && cursorPos > 0) {
            cursorPos--;
            result[cursorPos] = '\0';
            charIdx = 0;
        }
        if (Buttons::volButton.clicks == -1) {
            result[cursorPos] = '\0';
            if (cursorPos < 2) result[0] = '\0';
            return;
        }

        serialEvent();
        delay(20);
    }
}

static void stateNameEntry() {
    enterString("Enter your call sign:", ftp.playerCall, FTP_MAX_IDENT_LEN, true);
    enterString("Enter your name:", ftp.playerName, FTP_MAX_IDENT_LEN, false);
    savePlayerIdentity();
    ftp.useCallsign = (strlen(ftp.playerCall) > 0);
    ftp.state = FTP_LOBBY;
}

//=== STATE: Lobby ===

static void stateLobby() {
    pileupMode = true;
    bool hasCall = (strlen(ftp.playerCall) > 0);
    bool hasName = (strlen(ftp.playerName) > 0);

    while (ftp.state == FTP_LOBBY) {
        if (!ftp.singlePlayer) {
            if (millis() - ftp.lastBeaconSent >= FTP_BEACON_INTERVAL)
                sendBeacon();
            checkReceivedMessages();
            updateRoster();
        }

        canvas->fillSprite(FTP_BG);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        drawCentredText(15, "FIGHT THE", FTP_ACCENT);
        drawCentredText(43, "PILEUP", FTP_ACCENT);

        canvas->setFont(&fonts::FreeSans9pt7b);
        char buf[40];
        snprintf(buf, sizeof(buf), "%s: %s",
                 ftp.useCallsign ? "Call" : "Name", getPlayerIdent());
        drawCentredText(80, buf, FTP_TITLE);

        // Difficulty display
        canvas->setFont(&fonts::FreeSans9pt7b);
        snprintf(buf, sizeof(buf), "< %s >", diff().label);
        drawCentredText(108, buf, FTP_ACCENT);

        canvas->setFont(&fonts::Font0);
        snprintf(buf, sizeof(buf), "Timeout %ds  Start %d",
                 diff().callerTimeout / 1000, diff().initialCallers);
        drawCentredText(130, buf, FTP_DIM);

        if (ftp.singlePlayer) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            drawCentredText(155, "SINGLE PLAYER", FTP_TITLE);
        } else {
            canvas->setFont(&fonts::Font0);
            int y = 155;
            int active2 = countActivePlayers();
            if (active2 == 0)
                drawCentredText(155, "Searching...", FTP_DIM);
            else {
                for (int i = 0; i < FTP_MAX_PLAYERS && y < 230; i++) {
                    if (!ftp.players[i].active) continue;
                    uint16_t color = ftp.players[i].inPileup ? FTP_OK : FTP_TEXT;
                    if (millis() - ftp.players[i].lastBeacon > FTP_BEACON_TIMEOUT) color = FTP_DIM;
                    canvas->setTextColor(color, FTP_BG);
                    canvas->setTextDatum(lgfx::top_center);
                    canvas->drawString(ftp.players[i].ident, FTP_W / 2, y);
                    canvas->setTextDatum(lgfx::top_left);
                    y += 14;
                }
            }
        }

        canvas->setFont(&fonts::Font0);
        drawCentredText(240, "Encoder: difficulty", FTP_TEXT);
        drawCentredText(254, "Paddle: enter pileup", FTP_TEXT);
        drawCentredText(268, "Click: single/multi", FTP_TEXT);
        if (hasCall && hasName)
            drawCentredText(282, "FN: toggle call/name", FTP_TEXT);
        drawCentredText(296, "Long press: exit", FTP_TEXT);
        pushFrame();

        // Encoder: change difficulty
        int enc = checkEncoder();
        if (enc) {
            int d = (int)ftp.difficulty + enc;
            if (d < 0) d = 0;
            if (d >= FTP_NUM_DIFFICULTIES) d = FTP_NUM_DIFFICULTIES - 1;
            ftp.difficulty = (uint8_t)d;
            ftp.spawnInterval = diff().spawnMax;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        }

        if (checkPaddles()) {
            while (checkPaddles()) delay(5);
            ftp.state = FTP_CODE_CHALLENGE;
            return;
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == 1) {
            ftp.singlePlayer = !ftp.singlePlayer;
        }
        if (Buttons::modeButton.clicks == -1) { ftp.state = FTP_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {
            if (hasCall && hasName) ftp.useCallsign = !ftp.useCallsign;
        }
        if (Buttons::volButton.clicks == -1) { ftp.state = FTP_EXIT; return; }

        serialEvent();
        delay(20);
    }
}

//=== STATE: Code Challenge ===

static void stateCodeChallenge() {
    ftp.challengeLen = FTP_CODE_LEN_ENTRY;
    generateCode(ftp.challengeCode, ftp.challengeLen);
    ftp.challengePos = 0;

    gameMode = true;
    keyerState = IDLE_STATE;
    clearPaddleLatches();
    gameCharBuffer = 0;
    updateTimings();

    unsigned long startTime = millis();
    bool success = false;
    bool failed = false;
    unsigned long lastFrame = millis();

    while (!success && !failed) {
        if (millis() - lastFrame < 33) {
            checkPaddles();
            doPaddleIambic(leftKey, rightKey);
            updateFtpSound();
            delay(1);
            continue;
        }
        lastFrame = millis();

        canvas->fillSprite(FTP_BG);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        drawCentredText(20, "ENTER CODE", FTP_ACCENT);

        canvas->setFont(&fonts::FreeSansBold18pt7b);
        drawCentredText(65, ftp.challengeCode, FTP_TITLE);

        canvas->setFont(&fonts::FreeSansBold12pt7b);
        int totalWidth = ftp.challengeLen * 22;
        int startX = (FTP_W - totalWidth) / 2;
        for (int i = 0; i < ftp.challengeLen; i++) {
            char ch[2] = {ftp.challengeCode[i], '\0'};
            uint16_t color;
            if (i < ftp.challengePos)
                color = FTP_OK;
            else if (i == ftp.challengePos)
                color = FTP_ACCENT;
            else
                color = FTP_DIM;
            canvas->setTextColor(color, FTP_BG);
            canvas->setTextDatum(lgfx::top_center);
            canvas->drawString(ch, startX + i * 22 + 11, 110);
        }
        canvas->setTextDatum(lgfx::top_left);

        canvas->setFont(&fonts::Font0);
        drawCentredText(160, "Key the code above", FTP_TEXT);
        drawCentredText(180, "using your paddles", FTP_TEXT);

        char timeBuf[16];
        unsigned long elapsed = (millis() - startTime) / 1000;
        snprintf(timeBuf, sizeof(timeBuf), "%lu sec", elapsed);
        drawCentredText(210, timeBuf, FTP_DIM);

        drawCentredText(280, "Long press: cancel", FTP_TEXT);
        pushFrame();

        char decoded = pollKeyedChar();
        if (decoded) {
            if (decoded >= 'a' && decoded <= 'z') decoded = decoded - 'a' + 'A';
            if (decoded == ftp.challengeCode[ftp.challengePos]) {
                ftp.challengePos++;
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                if (ftp.challengePos >= ftp.challengeLen) success = true;
            } else {
                ftp.challengePos = 0;
                playSoundWrong();
            }
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) { failed = true; }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) { failed = true; }

        serialEvent();
    }

    cleanupKeyer();

    if (success) {
        playSoundLevelUp();
        for (int i = 0; i < 3; i++) {
            canvas->fillSprite(FTP_BG);
            canvas->setFont(&fonts::FreeSansBold12pt7b);
            drawCentredText(110, "ENTERING", FTP_OK);
            drawCentredText(140, "THE PILEUP!", FTP_OK);
            pushFrame();
            delay(500);
            updateFtpSound();
        }
        ftp.lives = 3;
        ftp.score = 0;
        ftp.streak = 0;
        ftp.bestStreak = 0;
        ftp.totalBlocked = 0;
        ftp.totalDropped = 0;
        ftp.totalAttacksSent = 0;
        ftp.eliminationCount = 0;
        ftp.callerCount = 0;
        ftp.lastSpawn = 0;
        ftp.spawnInterval = diff().spawnMax;
        clearInput();
        clearAllCallers();
        attackPromptActive = false;
        ftp.lastActivity = millis();
        ftp.state = FTP_PILEUP;
    } else {
        ftp.state = FTP_LOBBY;
    }
}


//=== STATE: Pileup — the main gameplay ===
//
// DEFEND: An incoming attack plays as CW audio at a different pitch.
//         The player decodes it by ear and keys it back.
//         After 3 plays the callsign text is revealed on screen.
//
// ATTACK: When no incoming attack is active, a prompt shows a callsign
//         for the player to key — this sends an attack to other players.

static int  currentAttackIdx = -1;     // index of the attack being defended, or -1

static void drawPileupHUD() {
    // Top bar: lives (dots) | ident | score
    for (int i = 0; i < ftp.lives; i++)
        canvas->fillCircle(10 + i * 14, 12, 5, FTP_WARN);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(FTP_TEXT, FTP_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(getPlayerIdent(), FTP_W / 2, 4);

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)ftp.score);
    canvas->setTextColor(FTP_ACCENT, FTP_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(buf, FTP_W - 4, 4);
    canvas->setTextDatum(lgfx::top_left);

    canvas->drawFastHLine(0, 23, FTP_W, FTP_TEXT);

    if (ftp.streak > 0) {
        snprintf(buf, sizeof(buf), "x%d", ftp.streak);
        canvas->setFont(&fonts::Font0);
        canvas->setTextColor(FTP_TITLE, FTP_BG);
        canvas->setTextDatum(lgfx::top_right);
        canvas->drawString(buf, FTP_W - 4, 26);
        canvas->setTextDatum(lgfx::top_left);
    }
}

static void drawDefendArea() {
    int y = 38;

    if (currentAttackIdx < 0) {
        // No active attack — show attack prompt instead
        if (attackPromptActive) {
            canvas->setFont(&fonts::Font0);
            drawCentredText(y, "YOUR ATTACK:", FTP_ATTACK);
            canvas->setFont(&fonts::FreeSans9pt7b);
            drawCentredText(y + 16, "SEND:", FTP_ATTACK);
            canvas->setFont(&fonts::FreeSansBold12pt7b);
            drawCentredText(y + 38, attackPrompt, FTP_ATTACK);
        } else {
            canvas->setFont(&fonts::Font0);
            drawCentredText(y + 20, "Waiting for callers...", FTP_DIM);
        }
        return;
    }

    // Active defend
    FtpCaller& atk = ftp.callers[currentAttackIdx];
    unsigned long timeout = diff().callerTimeout;
    unsigned long elapsed = millis() - atk.spawnTime;
    atk.progress = (float)elapsed / (float)timeout;

    canvas->setFont(&fonts::Font0);
    drawCentredText(y, "DEFEND!", FTP_WARN);

    // Progress bar (time remaining)
    int barW = FTP_W - 20;
    int barH = 6;
    int barX = 10;
    int barY = y + 14;
    float remaining = 1.0f - atk.progress;
    if (remaining < 0.0f) remaining = 0.0f;
    int fillW = (int)(barW * remaining);
    uint16_t barColor = remaining > 0.5f ? FTP_OK :
                        remaining > 0.25f ? FTP_TITLE : FTP_WARN;
    canvas->drawRect(barX, barY, barW, barH, FTP_DIM);
    if (fillW > 0)
        canvas->fillRect(barX, barY, fillW, barH, barColor);

    // Show callsign text only after enough plays
    if (cwPlayer.playCount >= FTP_PLAYS_BEFORE_REVEAL) {
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        drawCentredText(y + 30, atk.call, FTP_TEXT);
    } else {
        // Show play count
        char pbuf[20];
        snprintf(pbuf, sizeof(pbuf), "Listen... (%d/%d)",
                 cwPlayer.playCount + 1, FTP_PLAYS_BEFORE_REVEAL);
        canvas->setFont(&fonts::Font0);
        drawCentredText(y + 34, pbuf, FTP_DIM);
    }

    // Show queued attacks below
    if (ftp.callerCount > 1) {
        int qy = y + 58;
        canvas->setFont(&fonts::Font0);
        char qbuf[16];
        snprintf(qbuf, sizeof(qbuf), "Queued: %d", ftp.callerCount - 1);
        drawCentredText(qy, qbuf, FTP_DIM);
    }
}

static void drawInputArea() {
    int inputY = 160;
    canvas->drawFastHLine(0, inputY, FTP_W, FTP_DIM);

    canvas->setFont(&fonts::Font0);
    const char* label = (currentAttackIdx >= 0) ? "YOUR RESPONSE:" : "KEY ATTACK:";
    uint16_t labelColor = (currentAttackIdx >= 0) ? FTP_DIM : FTP_ATTACK;
    drawCentredText(inputY + 4, label, labelColor);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    if (ftp.inputPos > 0) {
        uint16_t inputColor = (currentAttackIdx >= 0) ? FTP_INPUT : FTP_ATTACK;
        drawCentredText(inputY + 20, ftp.inputBuf, inputColor);
    } else {
        canvas->setFont(&fonts::Font0);
        drawCentredText(inputY + 26, "Key a callsign...", FTP_DIM);
    }

    // Cursor blink
    if (ftp.inputPos > 0 && (millis() / 400) % 2 == 0) {
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        int tw = canvas->textWidth(ftp.inputBuf);
        int cx = (FTP_W + tw) / 2 + 2;
        canvas->fillRect(cx, inputY + 22, 2, 16, FTP_INPUT);
    }

    // Bottom status bar
    canvas->fillRect(0, 290, FTP_W, 30, 0x2104);
    canvas->setFont(&fonts::Font0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d wpm", ftp.wpm);
    canvas->setTextColor(FTP_TEXT, 0x2104);
    canvas->drawString(buf, 4, 298);

    if (ftp.singlePlayer) {
        canvas->setTextColor(FTP_TITLE, 0x2104);
        canvas->setTextDatum(lgfx::top_right);
        canvas->drawString("SOLO", FTP_W - 4, 298);
        canvas->setTextDatum(lgfx::top_left);
    }

    // Hint text (hidden during flash)
    if (!isFlashing()) {
        canvas->setFont(&fonts::Font0);
        drawCentredText(204, "Click: submit response", FTP_TEXT);
        drawCentredText(218, "or pause to auto-submit", FTP_DIM);
    }
}

static void handleDefendSubmit() {
    if (currentAttackIdx < 0 || ftp.inputPos == 0) return;

    ftp.inputBuf[ftp.inputPos] = '\0';

    // Compare uppercase: input is already uppercase, ensure stored call is too
    char expected[FTP_MAX_CALL_LEN + 1];
    strncpy(expected, ftp.callers[currentAttackIdx].call, FTP_MAX_CALL_LEN);
    expected[FTP_MAX_CALL_LEN] = '\0';
    for (int i = 0; expected[i]; i++)
        if (expected[i] >= 'a' && expected[i] <= 'z')
            expected[i] = expected[i] - 'a' + 'A';

    if (strcmp(ftp.inputBuf, expected) == 0) {
        // Correct!
        ftp.streak++;
        if (ftp.streak > ftp.bestStreak) ftp.bestStreak = ftp.streak;
        int bonus = FTP_SCORE_CORRECT + (ftp.streak * FTP_SCORE_STREAK_BONUS);
        addScore(bonus);
        ftp.totalBlocked++;

        char flashBuf[20];
        snprintf(flashBuf, sizeof(flashBuf), "+%d", bonus);
        triggerFlash(FTP_OK, flashBuf);
        playSoundCorrect();

        cwPlayerStop();
        removeAttack(currentAttackIdx);
        currentAttackIdx = -1;

        // Adjust spawn interval
        uint16_t range = diff().spawnMax - diff().spawnMin;
        uint16_t streakCap = ftp.streak > 20 ? 20 : ftp.streak;
        ftp.spawnInterval = diff().spawnMax - (uint16_t)(streakCap * range / 20);
    } else {
        // Wrong — can try again! Show both strings for debug
        ftp.streak = 0;
        playSoundWrong();
        char tryBuf[40];
        snprintf(tryBuf, sizeof(tryBuf), "%s!=%s", ftp.inputBuf, expected);
        triggerFlash(FTP_WARN, tryBuf);
    }

    clearInput();
}

static void handleAttackSubmit() {
    if (!attackPromptActive || ftp.inputPos == 0) return;

    ftp.inputBuf[ftp.inputPos] = '\0';

    // Compare uppercase
    char expected[FTP_MAX_CALL_LEN + 1];
    strncpy(expected, attackPrompt, FTP_MAX_CALL_LEN);
    expected[FTP_MAX_CALL_LEN] = '\0';
    for (int i = 0; expected[i]; i++)
        if (expected[i] >= 'a' && expected[i] <= 'z')
            expected[i] = expected[i] - 'a' + 'A';

    if (strcmp(ftp.inputBuf, expected) == 0) {
        // Attack sent!
        ftp.totalAttacksSent++;
        addScore(50);
        triggerFlash(FTP_ATTACK, "ATTACK SENT!");
        playSoundCorrect();
        attackPromptActive = false;
        // In multiplayer, this would send the attack to other players
    } else {
        playSoundWrong();
        triggerFlash(FTP_WARN, "TRY AGAIN");
    }

    clearInput();
}


static void statePileup() {
    pileupMode = true;
    gameMode = true;
    keyerState = IDLE_STATE;
    clearPaddleLatches();
    gameCharBuffer = 0;
    updateTimings();

    char challenge[FTP_MAX_CALL_LEN + 1];
    challenge[0] = '\0';
    bool needNewChallenge = true;
    bool waitingForResult = false;
    char resultExpected[FTP_MAX_CALL_LEN + 1] = "";
    char resultGot[FTP_MAX_CALL_LEN + 1] = "";
    unsigned long correctFlashTime = 0;
    bool lastWasTimeout = false;
    unsigned long lastSubmitTime = 0;
    unsigned long challengeStartTime = 0;
    bool encoderIsVolume = false;

    clearInput();
    unsigned long lastFrame = millis();

    while (ftp.state == FTP_PILEUP) {

        // --- Tight keyer polling loop ---
        if (millis() - lastFrame < 33) {
            if (!waitingForResult) {
                switch (keyerState) {
                    case DIT: case DAH: case KEY_START:
                        break;
                    default:
                        checkPaddles();
                        break;
                }
                if (doPaddleIambic(leftKey, rightKey)) {
                    continue;
                }

                char c = gameCharBuffer;
                if (c != 0) {
                    gameCharBuffer = 0;
                    if (c == ' ') {
                        if (ftp.inputPos >= 2) {
                            ftp.lastCharTime = 0;
                            ftp.challengePos = 1;
                        }
                    } else {
                        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
                        if (ftp.inputPos < FTP_MAX_CALL_LEN) {
                            ftp.inputBuf[ftp.inputPos++] = c;
                            ftp.inputBuf[ftp.inputPos] = '\0';
                        }
                        ftp.lastCharTime = millis();
                    }
                }

                if (ftp.inputPos >= 2 && ftp.lastCharTime > 0 &&
                    keyerState == IDLE_STATE && !leftKey && !rightKey) {
                    unsigned long wordGap = interWordSpace + ditLength;
                    if (wordGap < 1200) wordGap = 1200;
                    if (millis() - ftp.lastCharTime > wordGap) {
                        ftp.lastCharTime = 0;
                        ftp.challengePos = 1;
                    }
                }

                if (keyerState == IDLE_STATE && ftp.inputPos == 0 &&
                    !leftKey && !rightKey &&
                    millis() - lastSubmitTime > 2000) {
                    cwPlayerUpdate();
                }
                if (keyerState == IDLE_STATE) {
                    updateFtpSound();
                }
            }
            continue;
        }
        lastFrame = millis();

        checkPaddles();
        doPaddleIambic(leftKey, rightKey);

        if (keyerState != IDLE_STATE || leftKey || rightKey) {
            serialEvent();
            continue;
        }

        if (cwPlayer.toneOn) {
            cwPlayerUpdate();
            serialEvent();
            continue;
        }

        // --- CW player management ---
        if (ftp.inputPos > 0 || leftKey || rightKey || keyerState != IDLE_STATE) {
            if (cwPlayer.playing) cwPlayerStop();
        }
        if (keyerState == IDLE_STATE && ftp.inputPos == 0 &&
            !leftKey && !rightKey &&
            millis() - lastSubmitTime > 2000) {
            if (!cwPlayer.playing && challenge[0] != '\0' &&
                correctFlashTime == 0) {
                uint8_t saved = cwPlayer.playCount;
                cwPlayerStart(challenge);
                cwPlayer.playCount = saved;
            }
            cwPlayerUpdate();
            updateFtpSound();
        }

        // --- New challenge ---
        if (needNewChallenge) {
            String call = getRandomCall(0);
            call.toUpperCase();
            strncpy(challenge, call.c_str(), FTP_MAX_CALL_LEN);
            challenge[FTP_MAX_CALL_LEN] = '\0';
            cwPlayerStart(challenge);
            clearInput();
            ftp.challengePos = 0;
            needNewChallenge = false;
            waitingForResult = false;
            challengeStartTime = millis();
        }

        // --- Submission ---
        if (ftp.challengePos == 1 && !waitingForResult) {
            ftp.challengePos = 0;
            cwPlayerStop();

            ftp.inputBuf[ftp.inputPos] = '\0';
            strncpy(resultGot, ftp.inputBuf, FTP_MAX_CALL_LEN);
            resultGot[FTP_MAX_CALL_LEN] = '\0';

            strncpy(resultExpected, challenge, FTP_MAX_CALL_LEN);
            resultExpected[FTP_MAX_CALL_LEN] = '\0';
            for (int i = 0; resultExpected[i]; i++)
                if (resultExpected[i] >= 'a' && resultExpected[i] <= 'z')
                    resultExpected[i] = resultExpected[i] - 'a' + 'A';

            if (strcmp(resultGot, resultExpected) == 0) {
                ftp.streak++;
                if (ftp.streak > ftp.bestStreak) ftp.bestStreak = ftp.streak;
                int bonus = FTP_SCORE_CORRECT + (ftp.streak * FTP_SCORE_STREAK_BONUS);
                addScore(bonus);
                ftp.totalBlocked++;
                playSoundCorrect();
                correctFlashTime = millis();
                lastWasTimeout = false;
            } else {
                ftp.streak = 0;
                addScore(FTP_SCORE_WRONG);
                playSoundWrong();
                triggerFlash(FTP_WARN, "WRONG");
                uint8_t saved = cwPlayer.playCount;
                cwPlayerStart(challenge);
                cwPlayer.playCount = saved;
            }
            clearInput();
            lastSubmitTime = millis();
        }

        // --- Auto-advance after correct ---
        if (correctFlashTime > 0 && millis() - correctFlashTime > 1000) {
            correctFlashTime = 0;
            needNewChallenge = true;
        }

        // --- Timeout ---
        if (challengeStartTime > 0 && correctFlashTime == 0 &&
            millis() - challengeStartTime > diff().callerTimeout) {
            cwPlayerStop();
            ftp.totalDropped++;
            ftp.streak = 0;
            addScore(FTP_SCORE_TIMEOUT);
            playSoundTimeout();
            challengeStartTime = 0;
            lastWasTimeout = true;
            correctFlashTime = millis();
            clearInput();
            lastSubmitTime = millis();
        }

        // --- Life loss ---
        {
            uint8_t dpl = diff().dropsPerLife;
            uint8_t threshold = ftp.totalDropped / dpl;
            if (threshold > ftp.eliminationCount) {
                ftp.eliminationCount = threshold;
                if (ftp.lives > 0) {
                    ftp.lives--;
                    playSoundLifeLost();
                    triggerFlash(FTP_WARN, "LIFE LOST!");
                }
            }
        }

        if (ftp.lives == 0) {
            cleanupKeyer();
            ftp.state = FTP_GAME_OVER;
            pileupMode = false;
            return;
        }

        // --- Draw ---
        canvas->fillSprite(FTP_BG);

        // HUD: lives | ident | score
        for (int i = 0; i < ftp.lives; i++)
            canvas->fillCircle(10 + i * 14, 10, 5, FTP_WARN);

        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextColor(FTP_TEXT, FTP_BG);
        canvas->setTextDatum(lgfx::top_center);
        canvas->drawString(getPlayerIdent(), FTP_W / 2, 2);
        {
            char sbuf[16];
            snprintf(sbuf, sizeof(sbuf), "%lu", (unsigned long)ftp.score);
            canvas->setTextColor(FTP_ACCENT, FTP_BG);
            canvas->setTextDatum(lgfx::top_right);
            canvas->drawString(sbuf, FTP_W - 4, 2);
            canvas->setTextDatum(lgfx::top_left);
        }
        canvas->drawFastHLine(0, 21, FTP_W, FTP_TEXT);

        if (ftp.streak > 0) {
            char stbuf[8];
            snprintf(stbuf, sizeof(stbuf), "x%d", ftp.streak);
            canvas->setFont(&fonts::Font0);
            canvas->setTextColor(FTP_TITLE, FTP_BG);
            canvas->setTextDatum(lgfx::top_right);
            canvas->drawString(stbuf, FTP_W - 4, 24);
            canvas->setTextDatum(lgfx::top_left);
        }

        // Progress bar
        if (challengeStartTime > 0 && correctFlashTime == 0) {
            unsigned long elapsed = millis() - challengeStartTime;
            float remaining = 1.0f - (float)elapsed / (float)diff().callerTimeout;
            if (remaining < 0.0f) remaining = 0.0f;
            int barW = FTP_W - 20, barX = 10, barY = 34, barH = 4;
            int fillW = (int)(barW * remaining);
            uint16_t barColor = remaining > 0.5f ? FTP_OK :
                                remaining > 0.25f ? FTP_TITLE : FTP_WARN;
            canvas->drawRect(barX, barY, barW, barH, FTP_DIM);
            if (fillW > 0) canvas->fillRect(barX, barY, fillW, barH, barColor);
        }

        // Challenge area
        if (correctFlashTime > 0) {
            canvas->setFont(&fonts::FreeSansBold12pt7b);
            if (lastWasTimeout) {
                drawCentredText(50, "TIMEOUT", FTP_WARN);
                canvas->setFont(&fonts::FreeSans9pt7b);
                drawCentredText(80, challenge, FTP_TITLE);
            } else {
                drawCentredText(50, "OK!", FTP_OK);
                char cbuf[16];
                snprintf(cbuf, sizeof(cbuf), "+%d",
                    FTP_SCORE_CORRECT + ftp.streak * FTP_SCORE_STREAK_BONUS);
                canvas->setFont(&fonts::FreeSans9pt7b);
                drawCentredText(80, cbuf, FTP_OK);
            }
        } else {
            canvas->setFont(&fonts::Font0);
            char pbuf[20];
            snprintf(pbuf, sizeof(pbuf), "Play #%d", cwPlayer.playCount + 1);
            drawCentredText(44, pbuf, FTP_DIM);

            if (cwPlayer.playCount >= FTP_PLAYS_BEFORE_REVEAL) {
                canvas->setFont(&fonts::Font0);
                drawCentredText(60, "Hint:", FTP_DIM);
                canvas->setFont(&fonts::FreeSansBold12pt7b);
                drawCentredText(74, challenge, FTP_TITLE);
            } else {
                canvas->setFont(&fonts::FreeSans9pt7b);
                drawCentredText(66, "Listen...", FTP_TEXT);
                char dots[8] = "";
                for (int i = 0; i < FTP_PLAYS_BEFORE_REVEAL; i++)
                    dots[i] = (i < cwPlayer.playCount) ? '*' : '.';
                dots[FTP_PLAYS_BEFORE_REVEAL] = '\0';
                drawCentredText(86, dots, FTP_ACCENT);
            }
        }

        drawFlash();

        // Input area
        int inputY = 130;
        canvas->drawFastHLine(0, inputY, FTP_W, FTP_DIM);
        canvas->setFont(&fonts::Font0);
        drawCentredText(inputY + 4, "YOUR RESPONSE:", FTP_DIM);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        if (ftp.inputPos > 0) {
            drawCentredText(inputY + 20, ftp.inputBuf, FTP_INPUT);
        } else {
            canvas->setFont(&fonts::Font0);
            drawCentredText(inputY + 26, "Key the callsign...", FTP_DIM);
        }

        // Stats
        canvas->setFont(&fonts::Font0);
        {
            char statBuf[32];
            snprintf(statBuf, sizeof(statBuf), "OK:%d  Miss:%d",
                     ftp.totalBlocked, ftp.totalDropped);
            drawCentredText(200, statBuf, FTP_DIM);
        }
        drawCentredText(218, "Click:submit FN:spd/vol", FTP_DIM);

        // Status bar
        canvas->fillRect(0, 290, FTP_W, 30, 0x2104);
        canvas->setFont(&fonts::Font0);
        {
            char buf[20];
            snprintf(buf, sizeof(buf), "%d wpm%s", ftp.wpm,
                     encoderIsVolume ? "" : " <");
            canvas->setTextColor(encoderIsVolume ? FTP_DIM : FTP_TEXT, 0x2104);
            canvas->drawString(buf, 4, 294);
            snprintf(buf, sizeof(buf), "%sVol %d",
                     encoderIsVolume ? "< " : "",
                     MorsePreferences::sidetoneVolume);
            canvas->setTextColor(encoderIsVolume ? FTP_TEXT : FTP_DIM, 0x2104);
            canvas->drawString(buf, 4, 306);
        }

        pushFrame();

        checkPaddles();
        doPaddleIambic(leftKey, rightKey);

        // --- Encoder ---
        if (!waitingForResult && keyerState == IDLE_STATE) {
            int enc = checkEncoder();
            if (enc) {
                if (encoderIsVolume) {
                    int newVol = constrain((int)MorsePreferences::sidetoneVolume + enc, 0, 19);
                    MorsePreferences::sidetoneVolume = newVol;
                    #ifdef CONFIG_TLV320AIC3100
                    MorseOutput::soundSetVolume(newVol);
                    #endif
                    MorseOutput::pwmClick(newVol);
                } else {
                    ftp.wpm = constrain(ftp.wpm + enc, 5, 60);
                    MorsePreferences::wpm = ftp.wpm;
                    updateTimings();
                }
            }
        }

        // --- Buttons ---
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == 1 && ftp.inputPos >= 1) {
            ftp.challengePos = 1;
        }
        if (Buttons::modeButton.clicks == -1) {
            cleanupKeyer();
            ftp.state = FTP_EXIT;
            pileupMode = false;
            return;
        }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {
            encoderIsVolume = !encoderIsVolume;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        }
        if (Buttons::volButton.clicks == -1) {
            cleanupKeyer();
            ftp.state = FTP_EXIT;
            pileupMode = false;
            return;
        }

        serialEvent();
    }

    cleanupKeyer();
    pileupMode = false;
}

//=== STATE: Game Over ===

static void stateGameOver() {
    uint16_t totalCallers = ftp.totalBlocked + ftp.totalDropped;
    uint8_t accuracy = (totalCallers > 0) ?
        (uint8_t)(ftp.totalBlocked * 100 / totalCallers) : 0;

    canvas->fillSprite(FTP_BG);
    canvas->setFont(&fonts::FreeSansBold18pt7b);
    drawCentredText(20, "PILEUP", FTP_WARN);
    drawCentredText(58, "OVER!", FTP_WARN);

    canvas->setFont(&fonts::FreeSans9pt7b);
    char buf[40];
    snprintf(buf, sizeof(buf), "Score: %lu", (unsigned long)ftp.score);
    drawCentredText(110, buf, FTP_ACCENT);

    canvas->setFont(&fonts::Font0);
    snprintf(buf, sizeof(buf), "Defended: %d  Dropped: %d", ftp.totalBlocked, ftp.totalDropped);
    drawCentredText(140, buf, FTP_TEXT);

    snprintf(buf, sizeof(buf), "Accuracy: %d%%", accuracy);
    drawCentredText(158, buf, accuracy >= 70 ? FTP_OK : FTP_WARN);

    snprintf(buf, sizeof(buf), "Best streak: %d", ftp.bestStreak);
    drawCentredText(176, buf, FTP_TEXT);

    snprintf(buf, sizeof(buf), "%d wpm / %s", ftp.wpm, diff().label);
    drawCentredText(200, buf, FTP_DIM);

    drawCentredText(260, "Click: play again", FTP_TEXT);
    drawCentredText(276, "Long press: exit", FTP_TEXT);
    pushFrame();

    while (ftp.state == FTP_GAME_OVER) {
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == 1) {
            initGameData();
            ftp.state = FTP_LOBBY;
            return;
        }
        if (Buttons::modeButton.clicks == -1) { ftp.state = FTP_EXIT; return; }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) { ftp.state = FTP_EXIT; return; }
        serialEvent();
        delay(20);
    }
}

//=== Main entry point ===

void MorsePileup::run() {
    canvas = display.enterGameMode(MorsePreferences::leftHanded);
    if (!canvas) return;

    loadPlayerIdentity();
    ftp.difficulty = 0;
    initGameData();

    if (strlen(ftp.playerCall) == 0 && strlen(ftp.playerName) == 0)
        ftp.state = FTP_NAME_ENTRY;

    while (ftp.state != FTP_EXIT) {
        switch (ftp.state) {
            case FTP_NAME_ENTRY:     stateNameEntry();     break;
            case FTP_LOBBY:          stateLobby();         break;
            case FTP_CODE_CHALLENGE: stateCodeChallenge(); break;
            case FTP_PILEUP:         statePileup();        break;
            case FTP_GAME_OVER:      stateGameOver();      break;
            default:                 ftp.state = FTP_EXIT; break;
        }
    }

    pileupMode = false;
    cleanupKeyer();
    MorsePreferences::wpm = ftp.wpm;
    MorsePreferences::writePreferences("morserino");
    display.exitGameMode();

    MorseOutput::initDisplay();
    #ifdef CONFIG_DISPLAYWRAPPER
    MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
    #endif
    pinMode(PinCLK, INPUT_PULLUP);
    pinMode(PinDT, INPUT_PULLUP);
    rotaryEncoder.attachHalfQuad(PinDT, PinCLK);
    rotaryEncoder.setCount(0);
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks = 0;
}

#endif  // CONFIG_CW_GAME
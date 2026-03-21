/******************************************************************************************************************************
 *  Morse Invaders — Game module for Morserino-32 Pocket
 *  Copyright (C) 2025  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *****************************************************************************************************************************/

#include "MorseGame.h"

// Always defined (even without CONFIG_CW_GAME)
bool gameMode = false;
char gameCharBuffer = 0;


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

// ---- External references ----
extern int checkEncoder();
extern void checkShutDown(boolean);
extern void serialEvent();
extern boolean checkPaddles();
extern boolean doPaddleIambic(boolean, boolean);
extern boolean leftKey, rightKey;
extern DisplayWrapper display;
extern ESP32Encoder rotaryEncoder;
extern const int PinCLK;
extern const int PinDT;
extern void updateTimings();
extern void clearPaddleLatches();
extern KEYERSTATES keyerState;
extern unsigned int ditLength;


//=============================================================================
// Module-level variables
//=============================================================================

static lgfx::LGFX_Device* tft = nullptr;
static LGFX_Sprite*       canvas = nullptr;
static GameData            game;
static char                lastDecodedChar = 0;
static GameHighScore       highScores[GAME_HIGH_SCORES];


//=============================================================================
// Sound effect system (non-blocking)
//=============================================================================

struct SoundNote {
    uint16_t freq;
    uint16_t duration;
};

#define MAX_SOUND_NOTES 6

static SoundNote     soundSequence[MAX_SOUND_NOTES];
static uint8_t       soundNoteCount = 0;
static uint8_t       soundNoteIndex = 0;
static unsigned long soundNoteStart = 0;
static bool          soundPlaying = false;

static void startSound(const SoundNote* notes, uint8_t count) {
    for (int i = 0; i < count && i < MAX_SOUND_NOTES; i++)
        soundSequence[i] = notes[i];
    soundNoteCount = count;
    soundNoteIndex = 0;
    soundPlaying = true;
    soundNoteStart = millis();
    MorseOutput::pwmTone(soundSequence[0].freq, MorsePreferences::sidetoneVolume, false);
}

static void playSoundHit() {
    static const SoundNote s[] = {{880, 40}, {1320, 40}};
    startSound(s, 2);
}

static void playSoundMiss() {
    static const SoundNote s[] = {{330, 60}};
    startSound(s, 1);
}

static void playSoundLifeLost() {
    static const SoundNote s[] = {{660, 80}, {440, 80}, {330, 120}};
    startSound(s, 3);
}

static void playSoundLevelUp() {
    static const SoundNote s[] = {{523, 60}, {659, 60}, {784, 60}, {1047, 100}};
    startSound(s, 4);
}

static void playSoundExtraLife() {
    static const SoundNote s[] = {{1047, 50}, {1319, 50}};
    startSound(s, 2);
}

static void playSoundGameOver() {
    static const SoundNote s[] = {{440, 200}, {349, 200}, {294, 200}, {262, 400}};
    startSound(s, 4);
}

static void updateSound() {
    if (!soundPlaying) return;

    // Stop sound effect if keyer becomes active (CW sidetone takes priority)
    if (keyerState != IDLE_STATE) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        soundPlaying = false;
        return;
    }

    if (millis() - soundNoteStart >= soundSequence[soundNoteIndex].duration) {
        soundNoteIndex++;
        if (soundNoteIndex >= soundNoteCount) {
            MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
            soundPlaying = false;
        } else {
            soundNoteStart = millis();
            if (soundSequence[soundNoteIndex].freq > 0)
                MorseOutput::pwmTone(soundSequence[soundNoteIndex].freq,
                                     MorsePreferences::sidetoneVolume, false);
            else
                MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        }
    }
}


//=============================================================================
// High scores (NVS persistence)
//=============================================================================

static void loadHighScores() {
    Preferences pref;
    pref.begin("m32game", true);
    for (int i = 0; i < GAME_HIGH_SCORES; i++) {
        char key[12];
        snprintf(key, sizeof(key), "hs%d_s", i);
        highScores[i].score = pref.getUInt(key, 0);
        snprintf(key, sizeof(key), "hs%d_k", i);
        highScores[i].kochLesson = pref.getUChar(key, 0);
        snprintf(key, sizeof(key), "hs%d_l", i);
        highScores[i].subLevel = pref.getUChar(key, 0);
        snprintf(key, sizeof(key), "hs%d_w", i);
        highScores[i].wpm = pref.getUChar(key, 0);
    }
    pref.end();
}

static void saveHighScores() {
    Preferences pref;
    pref.begin("m32game", false);
    for (int i = 0; i < GAME_HIGH_SCORES; i++) {
        char key[12];
        snprintf(key, sizeof(key), "hs%d_s", i);
        pref.putUInt(key, highScores[i].score);
        snprintf(key, sizeof(key), "hs%d_k", i);
        pref.putUChar(key, highScores[i].kochLesson);
        snprintf(key, sizeof(key), "hs%d_l", i);
        pref.putUChar(key, highScores[i].subLevel);
        snprintf(key, sizeof(key), "hs%d_w", i);
        pref.putUChar(key, highScores[i].wpm);
    }
    pref.end();
}

// Returns rank (0-4) or -1 if not a high score
static int8_t insertHighScore() {
    int pos = -1;
    for (int i = 0; i < GAME_HIGH_SCORES; i++) {
        if (game.score > highScores[i].score) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return -1;

    for (int i = GAME_HIGH_SCORES - 1; i > pos; i--)
        highScores[i] = highScores[i - 1];

    highScores[pos].score = game.score;
    highScores[pos].kochLesson = game.kochLesson;
    highScores[pos].subLevel = game.subLevel;
    highScores[pos].wpm = game.wpm;
    saveHighScores();
    return pos;
}

static void drawHighScores(int startY, int8_t highlightRank) {
    canvas->setFont(&fonts::Font0);
    canvas->setTextDatum(lgfx::top_left);
    for (int i = 0; i < GAME_HIGH_SCORES; i++) {
        if (highScores[i].score == 0) break;
        uint16_t color = (i == highlightRank) ? GC_LEVELUP : GC_HUD_TEXT;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d. %5lu L%d-%d %dwpm",
                 i + 1, (unsigned long)highScores[i].score,
                 highScores[i].kochLesson, highScores[i].subLevel,
                 highScores[i].wpm);
        canvas->setTextColor(color, GC_BG);
        canvas->drawString(buf, 8, startY + i * 14);
    }
}


//=============================================================================
// Forward declarations
//=============================================================================

static bool   initDisplay();
static void   deinitDisplay();
static void   initGameData();
static void   updateLevelParams();

static void   stateMenu();
static void   stateCountdown();
static void   statePlaying();
static void   statePaused();
static void   stateLevelUp();
static void   stateGameOver();

static void   spawnInvader();
static void   moveInvaders();
static char   pollKeyedChar();
static int    findMatchingInvader(char c);
static void   destroyInvader(int idx);
static uint16_t getCharColor(char c);
static void   getDisplayStr(char c, char* buf);

static void   drawGameField();
static void   drawInvader(GameInvader& inv);
static void   drawHUD();
static void   drawKeyingZone();
static void   pushFrame();
static void   drawCentredText(int y, const char* text, uint16_t color,
                              const lgfx::IFont* font = nullptr);


//=============================================================================
// Display init / deinit
//=============================================================================

static bool initDisplay() {
    tft = display.getLGFX();
    if (!tft) return false;
    tft->setRotation(2);
    tft->fillScreen(GC_BG);

    if (!canvas) {
        canvas = new LGFX_Sprite(tft);
        if (!canvas) return false;
        canvas->setPsram(false);
        canvas->setColorDepth(16);
        if (!canvas->createSprite(GAME_SCREEN_W, GAME_SCREEN_H)) {
            delete canvas;
            canvas = nullptr;
            tft = nullptr;
            return false;
        }
    }
    canvas->fillSprite(GC_BG);
    return true;
}

static void deinitDisplay() {
    tft = nullptr;

    MorsePreferences::wpm = game.wpm;
    MorsePreferences::writePreferences("morserino");

    MorseOutput::initDisplay();
    #ifdef CONFIG_DISPLAYWRAPPER
    MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
    #endif

    pinMode(PinCLK, INPUT_PULLUP);
    pinMode(PinDT, INPUT_PULLUP);
    rotaryEncoder.attachHalfQuad(PinDT, PinCLK);
    rotaryEncoder.setCount(0);
}


//=============================================================================
// Game data init
//=============================================================================

static void initGameData() {
    game.state = GAME_MENU;
    game.score = 0;
    game.kochLesson = MorsePreferences::kochFilter;
    game.subLevel = 1;
    game.startSubLevel = 1;
    game.lives = 3;
    game.streak = 0;
    game.totalHits = 0;
    game.totalMisses = 0;
    game.totalDropped = 0;
    game.frameCount = 0;
    game.destroysThisLevel = 0;
    game.wpm = MorsePreferences::wpm;

    static String kochChars;
    kochChars = koch.getCharSet();
    game.kochSequence = kochChars.c_str();
    game.kochPoolSize = kochChars.length();

    for (int i = 0; i < GAME_MAX_INVADERS; i++) {
        game.invaders[i].active = false;
        game.invaders[i].explodeFrame = 0;
    }
    updateLevelParams();
}

static void updateLevelParams() {
    game.baseSpeed = 0.8f + (game.subLevel - 1) * 0.15f;
    game.spawnInterval = max(20, 120 - (game.subLevel - 1) * 8);
    game.spawnCounter = game.spawnInterval / 2;
    game.maxInvaders = constrain(3 + (game.subLevel - 1) / 2, 3, GAME_MAX_INVADERS);
}


//=============================================================================
// Character helpers
//=============================================================================

static uint16_t getCharColor(char c) {
    if (c >= '0' && c <= '9') return GC_NUMBERS;
    if (c == '.' || c == ',' || c == '?' || c == '/' || c == '-' || c == '=' || c == ':')
        return GC_PUNCT;
    if (c == 'S' || c == 'A' || c == 'N' || c == 'K' || c == 'E' || c == 'B' || c == '+')
        return GC_PROSIGN;
    return GC_LETTERS;
}

static void getDisplayStr(char c, char* buf) {
    bool upper = (MorsePreferences::pliste[posOutputCase].value != 0);
    switch (c) {
        case 'S': strcpy(buf, upper ? "AS" : "as"); return;
        case 'A': strcpy(buf, upper ? "KA" : "ka"); return;
        case 'N': strcpy(buf, upper ? "KN" : "kn"); return;
        case 'K': strcpy(buf, upper ? "SK" : "sk"); return;
        case 'E': strcpy(buf, upper ? "VE" : "ve"); return;
        case 'B': strcpy(buf, upper ? "BK" : "bk"); return;
        case '+': strcpy(buf, upper ? "AR" : "ar"); return;
        default:
            if (upper)
                buf[0] = (c >= 'a' && c <= 'z') ? c - 32 : c;
            else
                buf[0] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            buf[1] = '\0';
    }
}

static char chooseCharacter() {
    int pool = game.kochPoolSize;
    if (pool < 2) pool = 2;
    int idx;
    int r = random(100);
    if (r < 40 && pool >= 3)
        idx = random(pool * 2 / 3, pool);
    else if (r < 75 && pool >= 3)
        idx = random(pool / 3, pool * 2 / 3);
    else
        idx = random(0, max(1, pool / 3));
    return game.kochSequence[idx];
}


//=============================================================================
// Invader management
//=============================================================================

static void spawnInvader() {
    int activeCount = 0;
    for (int i = 0; i < GAME_MAX_INVADERS; i++)
        if (game.invaders[i].active) activeCount++;
    if (activeCount >= game.maxInvaders) return;

    int slot = -1;
    for (int i = 0; i < GAME_MAX_INVADERS; i++)
        if (!game.invaders[i].active && game.invaders[i].explodeFrame == 0) { slot = i; break; }
    if (slot < 0) return;

    uint8_t lane;
    int attempts = 0;
    do {
        lane = random(0, GAME_NUM_LANES);
        bool ok = true;
        for (int i = 0; i < GAME_MAX_INVADERS; i++)
            if (game.invaders[i].active && game.invaders[i].lane == lane && game.invaders[i].y < GAME_FIELD_TOP + 60)
                { ok = false; break; }
        if (ok) break;
    } while (++attempts < 10);

    GameInvader& inv = game.invaders[slot];
    inv.character = chooseCharacter();
    inv.lane = lane;
    inv.y = GAME_FIELD_TOP;
    inv.speed = game.baseSpeed + random(0, 30) / 100.0f;
    inv.color = getCharColor(inv.character);
    inv.active = true;
    inv.explodeFrame = 0;
}

static void moveInvaders() {
    for (int i = 0; i < GAME_MAX_INVADERS; i++) {
        GameInvader& inv = game.invaders[i];
        if (!inv.active) continue;
        inv.y += inv.speed;
        if (inv.y >= GAME_FIELD_BOTTOM - GAME_INVADER_H) {
            inv.active = false;
            game.lives--;
            game.totalDropped++;
            game.streak = 0;
            playSoundLifeLost();
        }
    }
}

static int findMatchingInvader(char c) {
    int bestIdx = -1;
    float bestY = -1;
    for (int i = 0; i < GAME_MAX_INVADERS; i++) {
        GameInvader& inv = game.invaders[i];
        if (!inv.active) continue;
        if (inv.character == c && inv.y > bestY) { bestY = inv.y; bestIdx = i; }
    }
    return bestIdx;
}

static void destroyInvader(int idx) {
    GameInvader& inv = game.invaders[idx];
    inv.active = false;
    inv.explodeFrame = 1;

    float speedMult = game.wpm / 10.0f;
    float urgencyMult = (inv.y > GAME_FIELD_BOTTOM * 0.7f) ? 2.0f : 1.0f;
    game.streak++;
    float streakMult = 1.0f;
    if (game.streak >= 20) streakMult = 3.0f;
    else if (game.streak >= 10) streakMult = 2.0f;
    else if (game.streak >= 5) streakMult = 1.5f;

    uint32_t points = (uint32_t)(10 * speedMult * urgencyMult * streakMult);
    game.score += points;
    game.totalHits++;
    game.destroysThisLevel++;
    playSoundHit();

    static uint32_t nextLifeAt = 1000;
    if (game.score >= nextLifeAt && game.lives < 5) {
        game.lives++;
        nextLifeAt += 1000;
        playSoundExtraLife();
    }
}


//=============================================================================
// Keyer input
//=============================================================================

static char pollKeyedChar() {
    checkPaddles();
    doPaddleIambic(leftKey, rightKey);
    char c = gameCharBuffer;
    if (c != 0) { gameCharBuffer = 0; return c; }
    return 0;
}


//=============================================================================
// Drawing
//=============================================================================

static void pushFrame() { canvas->pushSprite(0, 0); }

static void drawCentredText(int y, const char* text, uint16_t color,
                            const lgfx::IFont* font) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, GC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(text, GAME_SCREEN_W / 2, y);
    canvas->setTextDatum(lgfx::top_left);
}

static void drawInvader(GameInvader& inv) {
    int x = GAME_LANE_MARGIN + inv.lane * GAME_LANE_W + (GAME_LANE_W - GAME_INVADER_W) / 2;
    int y = (int)inv.y;

    if (inv.explodeFrame > 0) {
        int shrink = inv.explodeFrame * 3;
        if (inv.explodeFrame <= 2) {
            canvas->fillRoundRect(x + shrink, y + shrink,
                GAME_INVADER_W - shrink * 2, GAME_INVADER_H - shrink * 2, 3, GC_HUD_TEXT);
        } else if (inv.explodeFrame <= 4) {
            for (int p = 0; p < 4; p++) {
                int px = x + GAME_INVADER_W / 2 + random(-12, 12);
                int py = y + GAME_INVADER_H / 2 + random(-12, 12);
                canvas->fillRect(px, py, 2, 2, inv.color);
            }
        }
        inv.explodeFrame++;
        if (inv.explodeFrame > 5) inv.explodeFrame = 0;
        return;
    }
    if (!inv.active) return;

    canvas->fillRoundRect(x, y, GAME_INVADER_W, GAME_INVADER_H, 3, inv.color);
    canvas->drawRoundRect(x, y, GAME_INVADER_W, GAME_INVADER_H, 3,
        canvas->color565(
            ((inv.color >> 11) & 0x1F) * 4,
            ((inv.color >> 5) & 0x3F) * 2,
            (inv.color & 0x1F) * 4));

    char dispBuf[4];
    getDisplayStr(inv.character, dispBuf);

    if (strlen(dispBuf) > 1) {
        canvas->setFont(&fonts::Font0);
        canvas->setTextColor(GC_HUD_TEXT, inv.color);
        canvas->setTextDatum(lgfx::middle_center);
        canvas->drawString(dispBuf, x + GAME_INVADER_W / 2, y + GAME_INVADER_H / 2 + 2);
        int tw = canvas->textWidth(dispBuf);
        canvas->drawFastHLine(x + (GAME_INVADER_W - tw) / 2, y + 4, tw, GC_HUD_TEXT);
    } else {
        canvas->setFont(&fonts::FreeSansBold9pt7b);
        canvas->setTextColor(GC_HUD_TEXT, inv.color);
        canvas->setTextDatum(lgfx::middle_center);
        canvas->drawString(dispBuf, x + GAME_INVADER_W / 2, y + GAME_INVADER_H / 2);
    }
    canvas->setTextDatum(lgfx::top_left);
}

static void drawGameField() {
    for (int i = 0; i <= GAME_NUM_LANES; i++) {
        int x = GAME_LANE_MARGIN + i * GAME_LANE_W;
        canvas->drawFastVLine(x, GAME_FIELD_TOP, GAME_FIELD_BOTTOM - GAME_FIELD_TOP, 0x2104);
    }
    for (int i = 0; i < GAME_MAX_INVADERS; i++)
        if (game.invaders[i].active || game.invaders[i].explodeFrame > 0)
            drawInvader(game.invaders[i]);
}

static void drawHUD() {
    for (int i = 0; i < game.lives; i++)
        canvas->fillCircle(10 + i * 14, 12, 5, GC_LIVES);

    char buf[16];
    canvas->setFont(&fonts::FreeSans9pt7b);
    snprintf(buf, sizeof(buf), "%d-%d", game.kochLesson, game.subLevel);
    canvas->setTextColor(GC_HUD_TEXT, GC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(buf, GAME_SCREEN_W / 2, 4);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)game.score);
    canvas->setTextColor(GC_SCORE, GC_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(buf, GAME_SCREEN_W - 4, 4);

    canvas->setTextDatum(lgfx::top_left);
    canvas->drawFastHLine(0, GAME_FIELD_TOP - 1, GAME_SCREEN_W, GC_HUD_TEXT);
}

static void drawKeyingZone() {
    canvas->fillRect(0, GAME_KEYING_Y, GAME_SCREEN_W, GAME_SCREEN_H - GAME_KEYING_Y, 0x2104);

    canvas->setFont(&fonts::Font0);
    char wpmBuf[12];
    snprintf(wpmBuf, sizeof(wpmBuf), "%d wpm", game.wpm);
    canvas->setTextColor(GC_HUD_TEXT, 0x2104);
    canvas->drawString(wpmBuf, 4, GAME_KEYING_Y + 4);

    if (game.streak >= 5) {
        char streakBuf[12];
        snprintf(streakBuf, sizeof(streakBuf), "x%d", game.streak);
        canvas->setTextColor(GC_LEVELUP, 0x2104);
        canvas->drawString(streakBuf, 4, GAME_KEYING_Y + 18);
    }

    if (lastDecodedChar != 0) {
        char dispBuf[4];
        getDisplayStr(lastDecodedChar, dispBuf);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        canvas->setTextColor(GC_HUD_TEXT, 0x2104);
        canvas->setTextDatum(lgfx::middle_center);
        canvas->drawString(dispBuf, GAME_SCREEN_W / 2, GAME_KEYING_Y + 21);
        canvas->setTextDatum(lgfx::top_left);
    }
}


//=============================================================================
// STATE: Menu
//=============================================================================

static void stateMenu() {
    canvas->fillSprite(GC_BG);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentredText(20, "MORSE", GC_SCORE);
    drawCentredText(48, "INVADERS", GC_SCORE);

    canvas->setFont(&fonts::FreeSans9pt7b);
    char buf[40];

    snprintf(buf, sizeof(buf), "Koch Lesson: %d", game.kochLesson);
    drawCentredText(90, buf, GC_HUD_TEXT);

    int poolSize = game.kochPoolSize;
    if (poolSize > 14) poolSize = 14;
    char poolBuf[16];
    strncpy(poolBuf, game.kochSequence, poolSize);
    poolBuf[poolSize] = '\0';
    snprintf(buf, sizeof(buf), "Chars: %s%s", poolBuf,
             game.kochPoolSize > 14 ? "..." : "");
    drawCentredText(112, buf, GC_LETTERS);

    snprintf(buf, sizeof(buf), "Speed: %d WPM", game.wpm);
    drawCentredText(140, buf, GC_HUD_TEXT);

    snprintf(buf, sizeof(buf), "Start Level: %d-%d <",
             game.kochLesson, game.startSubLevel);
    drawCentredText(168, buf, GC_LEVELUP);

    // High scores
    if (highScores[0].score > 0) {
        canvas->setFont(&fonts::Font0);
        canvas->setTextColor(GC_HUD_TEXT, GC_BG);
        canvas->setTextDatum(lgfx::top_center);
        canvas->drawString("- HIGH SCORES -", GAME_SCREEN_W / 2, 198);
        canvas->setTextDatum(lgfx::top_left);
        drawHighScores(212, -1);
    }

    canvas->setFont(&fonts::Font0);
    drawCentredText(286, "Paddle: start  FN: toggle enc", GC_HUD_TEXT);
    drawCentredText(300, "Long press: exit", GC_HUD_TEXT);

    pushFrame();

    bool menuEncoderIsLevel = true;

    while (game.state == GAME_MENU) {
        int enc = checkEncoder();
        if (enc) {
            if (menuEncoderIsLevel) {
                game.startSubLevel += enc;
                if (game.startSubLevel < 1) game.startSubLevel = 1;
                if (game.startSubLevel > 50) game.startSubLevel = 50;
            } else {
                game.wpm = constrain(game.wpm + enc, 5, 60);
                MorsePreferences::wpm = game.wpm;
                updateTimings();
            }
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            canvas->fillRect(0, 135, GAME_SCREEN_W, 50, GC_BG);
            canvas->setFont(&fonts::FreeSans9pt7b);
            snprintf(buf, sizeof(buf), "Speed: %d WPM %s",
                     game.wpm, !menuEncoderIsLevel ? "<" : "");
            drawCentredText(140, buf, GC_HUD_TEXT);
            snprintf(buf, sizeof(buf), "Start Level: %d-%d %s",
                     game.kochLesson, game.startSubLevel,
                     menuEncoderIsLevel ? "<" : "");
            drawCentredText(168, buf, GC_LEVELUP);
            pushFrame();
        }

        if (checkPaddles()) {
            while (checkPaddles()) delay(5);
            game.subLevel = game.startSubLevel;
            game.state = GAME_COUNTDOWN;
            return;
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) { game.state = GAME_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {
            menuEncoderIsLevel = !menuEncoderIsLevel;
            canvas->fillRect(0, 135, GAME_SCREEN_W, 50, GC_BG);
            canvas->setFont(&fonts::FreeSans9pt7b);
            snprintf(buf, sizeof(buf), "Speed: %d WPM %s",
                     game.wpm, !menuEncoderIsLevel ? "<" : "");
            drawCentredText(140, buf, GC_HUD_TEXT);
            snprintf(buf, sizeof(buf), "Start Level: %d-%d %s",
                     game.kochLesson, game.startSubLevel,
                     menuEncoderIsLevel ? "<" : "");
            drawCentredText(168, buf, GC_LEVELUP);
            pushFrame();
        }
        if (Buttons::volButton.clicks == -1) { game.state = GAME_EXIT; return; }

        serialEvent();
        delay(10);
    }
}


//=============================================================================
// STATE: Countdown
//=============================================================================

static void stateCountdown() {
    const char* counts[] = {"3", "2", "1", "GO!"};
    const uint16_t colors[] = {GC_HUD_TEXT, GC_HUD_TEXT, GC_HUD_TEXT, GC_LETTERS};

    for (int i = 0; i < 4; i++) {
        canvas->fillSprite(GC_BG);
        canvas->setFont(&fonts::FreeSansBold18pt7b);
        drawCentredText(130, counts[i], colors[i]);
        canvas->setFont(&fonts::FreeSans9pt7b);
        char buf[20];
        snprintf(buf, sizeof(buf), "Level %d-%d", game.kochLesson, game.subLevel);
        drawCentredText(200, buf, GC_LEVELUP);
        pushFrame();
        delay(i < 3 ? 600 : 400);
    }

    keyerState = IDLE_STATE;
    clearPaddleLatches();
    gameCharBuffer = 0;
    lastDecodedChar = 0;
    game.frameCount = 0;
    game.state = GAME_PLAYING;
}


//=============================================================================
// STATE: Playing
//=============================================================================

static void statePlaying() {
    gameMode = true;

    unsigned long lastFrame = millis();
    while (game.state == GAME_PLAYING) {
        if (millis() - lastFrame < 33) {
            checkPaddles();
            doPaddleIambic(leftKey, rightKey);
            updateSound();
            delay(1);
            continue;
        }
        lastFrame = millis();
        game.frameCount++;

        // Keyer input
        char decoded = pollKeyedChar();
        if (decoded) {
            lastDecodedChar = decoded;
            int match = findMatchingInvader(decoded);
            if (match >= 0) {
                destroyInvader(match);
            } else {
                game.totalMisses++;
                game.streak = 0;
                playSoundMiss();
            }
        }

        // Encoder: speed
        int enc = checkEncoder();
        if (enc) {
            game.wpm = constrain(game.wpm + enc, 5, 60);
            MorsePreferences::wpm = game.wpm;
            updateTimings();
        }

        // Buttons
        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
            case 1:  game.state = GAME_PAUSED; gameMode = false; return;
            case -1: game.state = GAME_EXIT;   gameMode = false; return;
        }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {
            game.lives = 0; game.state = GAME_OVER; gameMode = false; return;  // test trigger
        }
        if (Buttons::volButton.clicks == -1) {
            game.state = GAME_EXIT; gameMode = false; return;
        }

        // Spawn
        if (--game.spawnCounter <= 0) {
            spawnInvader();
            game.spawnCounter = game.spawnInterval;
        }

        // Move
        moveInvaders();

        // Game over?
        if (game.lives <= 0) {
            game.lives = 0; game.state = GAME_OVER; gameMode = false; return;
        }

        // Level up?
        if (game.destroysThisLevel >= GAME_DESTROYS_PER_LEVEL) {
            game.subLevel++;
            game.destroysThisLevel = 0;
            updateLevelParams();
            game.state = GAME_LEVEL_UP; gameMode = false; return;
        }

        // Draw
        canvas->fillSprite(GC_BG);
        drawHUD();
        drawGameField();
        drawKeyingZone();
        updateSound();
        pushFrame();

        serialEvent();
    }
    gameMode = false;
}


//=============================================================================
// STATE: Paused
//=============================================================================

static void statePaused() {
    canvas->fillRect(20, 120, 130, 60, GC_BG);
    canvas->drawRect(20, 120, 130, 60, GC_HUD_TEXT);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentredText(128, "PAUSED", GC_HUD_TEXT);
    canvas->setFont(&fonts::Font0);
    drawCentredText(158, "Click to resume", GC_HUD_TEXT);
    pushFrame();

    while (game.state == GAME_PAUSED) {
        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
            case 1:  game.state = GAME_PLAYING; return;
            case -1: game.state = GAME_EXIT;    return;
        }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) { game.state = GAME_EXIT; return; }
        serialEvent();
        delay(20);
    }
}


//=============================================================================
// STATE: Level Up
//=============================================================================

static void stateLevelUp() {
    canvas->fillSprite(GC_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    char buf[24];
    snprintf(buf, sizeof(buf), "LEVEL %d-%d", game.kochLesson, game.subLevel);
    drawCentredText(120, buf, GC_LEVELUP);
    canvas->setFont(&fonts::FreeSans9pt7b);
    snprintf(buf, sizeof(buf), "Score: %lu", (unsigned long)game.score);
    drawCentredText(170, buf, GC_SCORE);
    pushFrame();

    playSoundLevelUp();
    // Let the sound play during the delay
    unsigned long start = millis();
    while (millis() - start < 1500) {
        updateSound();
        delay(10);
    }

    for (int i = 0; i < GAME_MAX_INVADERS; i++) {
        if (game.invaders[i].active) { game.invaders[i].active = false; game.score += 5; }
        game.invaders[i].explodeFrame = 0;
    }
    game.state = GAME_PLAYING;
}


//=============================================================================
// STATE: Game Over
//=============================================================================

static void stateGameOver() {
    canvas->fillSprite(GC_BG);
    playSoundGameOver();

    canvas->setFont(&fonts::FreeSansBold18pt7b);
    drawCentredText(20, "GAME", GC_GAMEOVER);
    drawCentredText(55, "OVER", GC_GAMEOVER);

    canvas->setFont(&fonts::FreeSans9pt7b);
    char buf[40];

    snprintf(buf, sizeof(buf), "Score: %lu", (unsigned long)game.score);
    drawCentredText(105, buf, GC_SCORE);
    snprintf(buf, sizeof(buf), "Level: %d-%d", game.kochLesson, game.subLevel);
    drawCentredText(128, buf, GC_HUD_TEXT);
    snprintf(buf, sizeof(buf), "Speed: %d WPM", game.wpm);
    drawCentredText(148, buf, GC_HUD_TEXT);

    uint16_t total = game.totalHits + game.totalMisses + game.totalDropped;
    if (total > 0) {
        snprintf(buf, sizeof(buf), "Accuracy: %d%%",
                 (int)(game.totalHits * 100 / total));
        drawCentredText(168, buf, GC_HUD_TEXT);
    }
    snprintf(buf, sizeof(buf), "Hits: %d  Missed: %d", game.totalHits, game.totalMisses);
    drawCentredText(188, buf, GC_HUD_TEXT);
    snprintf(buf, sizeof(buf), "Dropped: %d", game.totalDropped);
    drawCentredText(204, buf, GC_HUD_TEXT);

    // High score check
    int8_t rank = insertHighScore();
    if (rank >= 0) {
        canvas->setFont(&fonts::FreeSans9pt7b);
        drawCentredText(224, "NEW HIGH SCORE!", GC_LEVELUP);
    }
    drawHighScores(242, rank);

    canvas->setFont(&fonts::Font0);
    drawCentredText(316 - 24, "Click: Play Again", GC_HUD_TEXT);
    drawCentredText(316 - 10, "Long press: Exit", GC_HUD_TEXT);

    pushFrame();

    // Let game over sound play
    unsigned long start = millis();
    while (millis() - start < 1200) { updateSound(); delay(10); }

    while (game.state == GAME_OVER) {
        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
            case 1: {
                uint8_t startLvl = game.startSubLevel;
                uint8_t wpm = game.wpm;
                initGameData();
                game.startSubLevel = startLvl;
                game.subLevel = startLvl;
                game.wpm = wpm;
                MorsePreferences::wpm = wpm;
                game.state = GAME_COUNTDOWN;
                return;
            }
            case -1: game.state = GAME_EXIT; return;
        }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) { game.state = GAME_EXIT; return; }
        updateSound();
        serialEvent();
        delay(20);
    }
}


//=============================================================================
// Main entry point
//=============================================================================

void MorseGame::run() {
    if (!initDisplay()) {
        MorseOutput::initDisplay();
        #ifdef CONFIG_DISPLAYWRAPPER
        MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
        #endif
        return;
    }

    initGameData();
    loadHighScores();

    while (game.state != GAME_EXIT) {
        switch (game.state) {
            case GAME_MENU:      stateMenu();       break;
            case GAME_COUNTDOWN: stateCountdown();   break;
            case GAME_PLAYING:   statePlaying();     break;
            case GAME_PAUSED:    statePaused();      break;
            case GAME_LEVEL_UP:  stateLevelUp();     break;
            case GAME_OVER:      stateGameOver();    break;
            default:             game.state = GAME_EXIT; break;
        }
    }

    gameMode = false;
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
    deinitDisplay();
}

#endif  // CONFIG_CW_GAME
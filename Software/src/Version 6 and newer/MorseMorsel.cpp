/******************************************************************************************************************************
 *  Morsel — Word-guessing game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  Milestone S1: the core single-word loop.
 *   - Combined dictionary + ham-abbreviation pool, filtered by length and the
 *     current Koch lesson (every letter must be in koch.getCharSet()).
 *   - One random letter position revealed in clear text, persists for the word.
 *   - The word is played once in CW at a fixed clue speed (independent of the
 *     player's own keyer speed).
 *   - The player keys the whole word; a long inter-word pause submits the
 *     guess; the <err> prosign (8 dits) deletes the last entered character.
 *   - Letter boxes colour after each guess: green = right letter & position,
 *     red = wrong, grey = revealed slot keyed wrong.
 *   - Wrong guess replays the word (same speed in S1) and lets the player
 *     try again. Correct guess advances to a new word.
 *
 *  No speed schedule, scoring, skip or high scores yet — those are S2..S4.
 *****************************************************************************************************************************/

#include "MorseMorsel.h"
#include "MorseGameMode.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseGame.h"           // gameMode, gameCharBuffer
#include "morsedefs.h"
#include "ClickButton.h"
#include "DisplayWrapper.h"
#include "english_words.h"
#include "abbrev.h"
#include "DejaVuSansMono_Bold15pt7b.h"

#include <Preferences.h>
#include <WiFi.h>                 // WiFi.macAddress() for the MP identity fallback

#include <LovyanGFX.hpp>

// ---- External references ----
extern int        checkEncoder();
extern void       checkShutDown(boolean);
extern void       serialEvent();
extern boolean    checkPaddles();
extern boolean    doPaddleIambic(boolean, boolean);
extern boolean    leftKey, rightKey;
extern void       clearPaddleLatches();
extern void       keyOut(boolean, boolean, int, int);
extern String     generateCWword(const String& symbols);   // m32_v6.ino — pure
extern KEYERSTATES keyerState;
extern unsigned int ditLength;          // player's own keyer timing (for submit gap)
extern unsigned int interWordSpace;
extern bool       EspNowIsActive;       // owned by MorseMenu; ESP-NOW is up when true

//=============================================================================
// S1 tunables — kept here so playtesting can tweak the feel quickly
//=============================================================================

#define MSL_START_WPM       48      // clue speed in round 1
#define MSL_WPM_STEP         5      // clue slows this much per round
#define MSL_MIN_WPM         18      // clue speed floor (round 7+)
#define MSL_MIN_POOL         6      // minimum eligible words to start
#define MSL_HI_N             7      // high-score table size (fits the screen)
#define MSL_MAX_WORD_LEN     6      // longest word Morsel ever handles
#define MSL_FEEDBACK_MS   1500      // how long the coloured result is shown
#define MSL_CORRECT_MS    1100      // how long the all-green "solved" is shown
#define MSL_IDLE_REPLAY_MS 12000UL  // no input this long -> auto-replay the clue
#define MSL_IDLE_EXIT_MS   60000UL  // total idle this long -> back to lobby
#define MSL_GAME_WORDS      10      // words per single-player game
#define MSL_GUESS_PENALTY_S  5      // seconds added per guess (from guess #1)
#define MSL_SKIP_PENALTY_S  60      // flat seconds added when a word is skipped
#define MSL_SKIP_MS        900      // how long the "Skipped" notice is shown

//=============================================================================
// Module-level state
//=============================================================================

static LGFX_Sprite* canvas   = nullptr;
static MslState     mslState = MSL_INIT;

// What the rotary encoder controls during play (mirrors the other games).
enum MslEnc : uint8_t { MSL_ENC_SPEED, MSL_ENC_VOLUME };
static MslEnc   mslEnc = MSL_ENC_SPEED;

// The Koch lesson can be changed temporarily in the lobby for this game
// session only; the user's real setting is restored on exit.
static uint8_t  savedKochFilter = 0;

// Combined word pool: lightweight references into the two static lists.
struct MslPoolEntry { uint16_t idx; bool isAbbr; };
static MslPoolEntry mslPool[EnglishWords::WORDS_NUMBER_OF_ELEMENTS +
                            Abbrev::ABBREV_NUMBER_OF_ELEMENTS];
static int          mslPoolN = 0;

// The 10 words selected for the current game (no repetition).
static MslPoolEntry  gameSel[MSL_GAME_WORDS];
// Resolved word strings for the current game. Single-player and the MP
// server fill these from gameSel (the pool selection); an MP client fills
// them from the word list the server broadcasts. startRound() always reads
// the word from here, so the play engine is identical in all three cases.
static char          gameWordStr[MSL_GAME_WORDS][MSL_MAX_WORD_LEN + 1];
static int           gameWords;          // = min(MSL_GAME_WORDS, mslPoolN)

// True while playing a multiplayer synced game (vs single-player). Gates the
// MP per-word timeout and end-of-game result reporting in the play loop.
// Declared up here because the shared play engine (startRound/finishWord/
// playLoop) reads it, but it's driven by the multiplayer code further down.
static bool          mpGame = false;
static void          mslSendResult();    // defined in the multiplayer section
static int           wordIndex;          // 0-based: which game word we're on
static unsigned long totalAdjMs;         // accumulated adjusted time
static int           totalGuesses;
static int           solvedCount;
static int           skippedCount;
static uint8_t       gameKoch;           // Koch lesson this game was played at
static uint8_t       gameWlen;           // word-length option this game used

// Word-length setting: 7 options (GDD: 3,4,max4,5,max5,6,max6). "max N"
// means any length from 3..N; an exact option means only that length.
#define MSL_WLEN_OPTS 7
static const uint8_t wlenMin[MSL_WLEN_OPTS] = { 3, 4, 3, 5, 3, 6, 3 };
static const uint8_t wlenMax[MSL_WLEN_OPTS] = { 3, 4, 4, 5, 5, 6, 6 };
static const char*  wlenLabel[MSL_WLEN_OPTS] =
    { "3", "4", "max 4", "5", "max 5", "6", "max 6" };
static uint8_t       wlenOpt = 1;        // default: exactly 4 (persisted)

// Suggested minimum Koch lesson per word length for a viable pool
// (computed offline against the default Morserino sequence; index by len).
static const uint8_t recKoch[7] = { 0, 0, 0, 8, 10, 14, 16 };

// Persistent high-score table (unified across settings).
struct MslScore {
    uint32_t adjMs;      // total adjusted time
    uint16_t guesses;    // total guesses
    uint8_t  wlen;       // word-length option index
    uint8_t  koch;       // Koch lesson played at
    uint8_t  solved;     // words solved
    uint8_t  total;      // words in that game
};
static MslScore hiTable[MSL_HI_N];
static int      lastRank = -1;           // 0-based rank just achieved, or -1

#define MSL_NVS_NS   "morsel"
#define MSL_HI_VER   2          // bumped: table size changed (old blob ignored)

// Current round
static char          target[MSL_MAX_WORD_LEN + 1];
static int           wordLen;
static int           revealPos;
static char          guess[MSL_MAX_WORD_LEN + 1];
static int           guessLen;
static bool          evaluated;          // true while showing coloured result
static unsigned long lastCharTime;
static unsigned long idleSince;          // last real player activity
static unsigned long nextReplayAt;       // when to auto-replay the clue
static unsigned long mpWordDeadline;     // MP: hard per-word skip deadline
static int           roundNo;            // 1-based attempt count for this word
static int           clueWpm;            // current clue speed (per schedule)
static unsigned long wordStartMs;        // when the clue first began this word
static unsigned long lastWordMs;         // solve time of the last solved word

// Per-position result for drawing (after a submit)
enum MslCell : uint8_t { CELL_NEUTRAL, CELL_GREEN, CELL_RED, CELL_GREY };
static MslCell cellState[MSL_MAX_WORD_LEN];

// Non-blocking CW clue player (own timing — independent of keyer speed)
struct MslCwPlayer {
    char          elements[128];
    int           pos, len;
    bool          playing, toneOn;
    bool          done;            // finished its single play-through
    unsigned long timer;
    int           pitch;
    unsigned int  ditMs, dahMs, charMs;
};
static MslCwPlayer cw;

//=============================================================================
// Word pool
//=============================================================================

static const char* poolWord(const MslPoolEntry& e) {
    return e.isAbbr ? Abbrev::abbreviations[e.idx] : EnglishWords::words[e.idx];
}

// All letters of `w` must be within the currently-learned Koch set.
static bool isKochEligible(const char* w, const String& kochSet) {
    for (const char* p = w; *p; ++p)
        if (kochSet.indexOf(*p) < 0)
            return false;
    return true;
}

// Append all exactly-`len`-letter Koch-eligible words/abbrevs to the pool.
// Both lists are length-indexed: entries of length L occupy
// [POINTER[L], POINTER[L-1]) (the arrays are ordered longest-first).
static void addLenToPool(int len, const String& kochSet) {
    if (len < 1 || len > 6) return;
    for (int i = EnglishWords::WORDS_POINTER[len];
             i < EnglishWords::WORDS_POINTER[len - 1]; ++i)
        if (isKochEligible(EnglishWords::words[i], kochSet))
            mslPool[mslPoolN++] = { (uint16_t)i, false };
    for (int i = Abbrev::ABBREV_POINTER[len];
             i < Abbrev::ABBREV_POINTER[len - 1]; ++i)
        if (isKochEligible(Abbrev::abbreviations[i], kochSet))
            mslPool[mslPoolN++] = { (uint16_t)i, true };
}

// Build the combined pool for the current word-length setting and Koch
// lesson. Returns the pool size.
static int buildPool() {
    mslPoolN = 0;
    String kochSet = koch.getCharSet();
    for (int len = wlenMin[wlenOpt]; len <= wlenMax[wlenOpt]; ++len)
        addLenToPool(len, kochSet);
    return mslPoolN;
}

//=============================================================================
// Persistence (NVS namespace "morsel")
//=============================================================================

static void hiClear() {
    for (int i = 0; i < MSL_HI_N; ++i)
        hiTable[i] = { 0xFFFFFFFFUL, 0, 0, 0, 0, 0 };   // empty = max time
}

static void mslLoadPrefs() {
    Preferences p;
    p.begin(MSL_NVS_NS, true);
    wlenOpt = p.getUChar("wlen", 1);
    if (wlenOpt >= MSL_WLEN_OPTS) wlenOpt = 1;
    hiClear();
    if (p.getUChar("hv", 0) == MSL_HI_VER)
        p.getBytes("hi", hiTable, sizeof(hiTable));
    p.end();
}

static void mslSaveWlen() {
    Preferences p;
    p.begin(MSL_NVS_NS, false);
    p.putUChar("wlen", wlenOpt);
    p.end();
}

static void mslSaveHi() {
    Preferences p;
    p.begin(MSL_NVS_NS, false);
    p.putBytes("hi", hiTable, sizeof(hiTable));
    p.putUChar("hv", MSL_HI_VER);
    p.end();
}

// Insert the just-finished game into the table if it qualifies; sets
// lastRank to the 0-based rank, or -1. Persists on qualification.
static void mslRecordScore() {
    MslScore s = { (uint32_t)totalAdjMs, (uint16_t)totalGuesses,
                   gameWlen, gameKoch,
                   (uint8_t)solvedCount, (uint8_t)gameWords };
    lastRank = -1;
    for (int i = 0; i < MSL_HI_N; ++i) {
        if (s.adjMs < hiTable[i].adjMs) {
            for (int j = MSL_HI_N - 1; j > i; --j) hiTable[j] = hiTable[j - 1];
            hiTable[i] = s;
            lastRank = i;
            mslSaveHi();
            return;
        }
    }
}

//=============================================================================
// Drawing helpers
//=============================================================================

static void pushFrame() { MorseGameMode::pushFrame(); }

static void drawCentred(int y, const char* text, uint16_t color,
                         const lgfx::IFont* font = nullptr) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, MSL_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(text, MSL_SCREEN_W / 2, y);
    canvas->setTextDatum(lgfx::top_left);
}

// Draw the row of letter boxes for the current round state.
static void drawBoard(const char* status) {
    canvas->fillSprite(MSL_BG);

    // --- Top bar: round (left) and clue speed (right). No big title here:
    // it crowded the corners; the game name lives in the lobby. ---
    char rb[16];
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    snprintf(rb, sizeof(rb), "Round %d", roundNo);
    canvas->setTextColor(MSL_TEXT, MSL_BG);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(rb, 8, 6);
    snprintf(rb, sizeof(rb), "Clue %d", clueWpm);
    canvas->setTextColor(clueWpm <= MSL_MIN_WPM ? MSL_HIGHLIGHT : MSL_ACCENT, MSL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(rb, MSL_SCREEN_W - 8, 6);

    snprintf(rb, sizeof(rb), "%d/%d", wordIndex + 1, gameWords);
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(MSL_DIM, MSL_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(rb, MSL_SCREEN_W / 2, 9);
    canvas->setTextDatum(lgfx::top_left);

    // --- Letter boxes: snug around the glyph, sized to fit up to 6 cells ---
    const int marginX = 8, gap = 6, boxH = 44;
    int avail = MSL_SCREEN_W - 2 * marginX;
    int boxW  = (avail - (wordLen - 1) * gap) / wordLen;
    if (boxW > 46) boxW = 46;
    int totalW = wordLen * boxW + (wordLen - 1) * gap;
    int x0 = (MSL_SCREEN_W - totalW) / 2;
    int y0 = 46;

    for (int i = 0; i < wordLen; ++i) {
        int bx = x0 + i * (boxW + gap);
        uint16_t border = MSL_DIM, fill = MSL_BG, fg = MSL_TEXT;
        char ch = 0;
        bool isReveal = (i == revealPos);

        if (!evaluated) {
            // The revealed slot keeps its cyan border so the hint position is
            // always identifiable. Show what the player keyed wherever the
            // cursor has passed (so corrections are visible); fall back to the
            // clue letter only at the revealed slot before the cursor reaches it.
            if (isReveal) border = MSL_ACCENT;
            if (i < guessLen)      { ch = guess[i];  fg = MSL_TEXT; }
            else if (isReveal)     { ch = target[i]; fg = MSL_ACCENT; }
        } else {
            switch (cellState[i]) {
                case CELL_GREEN: fill = MSL_GREEN; fg = MSL_BG;
                                 ch = target[i]; break;
                case CELL_RED:   fill = MSL_RED;   fg = MSL_TEXT;
                                 ch = (i < guessLen ? guess[i] : 0); break;
                case CELL_GREY:  fill = MSL_GREY;  fg = MSL_TEXT;
                                 ch = target[i]; break;   // revealed slot kept
                default: break;
            }
        }

        canvas->fillRect(bx, y0, boxW, boxH, fill);
        // 2-px border so it reads at normal viewing distance.
        canvas->drawRect(bx,     y0,     boxW,     boxH,     border);
        canvas->drawRect(bx + 1, y0 + 1, boxW - 2, boxH - 2, border);

        if (ch) {
            char s[2] = { (char)toupper(ch), 0 };
            canvas->setFont(&DejaVuSansMono_Bold15pt7b);
            canvas->setTextColor(fg, fill);
            canvas->setTextDatum(lgfx::middle_center);
            canvas->drawString(s, bx + boxW / 2, y0 + boxH / 2);
            canvas->setTextDatum(lgfx::top_left);
        }

        // Position number under each box (1-based); the revealed slot's
        // number is cyan so the hint's position is instantly clear.
        char pn[3];
        snprintf(pn, sizeof(pn), "%d", i + 1);
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextColor(isReveal ? MSL_ACCENT : MSL_DIM, MSL_BG);
        canvas->setTextDatum(lgfx::top_center);
        canvas->drawString(pn, bx + boxW / 2, y0 + boxH + 4);
        canvas->setTextDatum(lgfx::top_left);
    }

    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(110, status, MSL_TEXT);
    drawCentred(128, "click = skip   hold = quit", MSL_DIM);

    // HUD: Koch lesson, player keyer speed, volume. The encoder target
    // (speed or volume) is highlighted; FN/vol-button short-press toggles it.
    char hud[24];
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::top_left);

    snprintf(hud, sizeof(hud), "Koch %d", MorsePreferences::kochFilter);
    canvas->setTextColor(MSL_DIM, MSL_BG);
    canvas->drawString(hud, 8, 148);

    snprintf(hud, sizeof(hud), "key %d wpm", MorsePreferences::wpm);
    canvas->setTextColor(mslEnc == MSL_ENC_SPEED ? MSL_HIGHLIGHT : MSL_DIM, MSL_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(hud, MSL_SCREEN_W / 2, 148);

    snprintf(hud, sizeof(hud), "vol %d", MorsePreferences::sidetoneVolume);
    canvas->setTextColor(mslEnc == MSL_ENC_VOLUME ? MSL_HIGHLIGHT : MSL_DIM, MSL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(hud, MSL_SCREEN_W - 8, 148);
    canvas->setTextDatum(lgfx::top_left);

    pushFrame();
}

//=============================================================================
// CW clue player — non-blocking, plays the word ONCE
//=============================================================================

static void cwStart(const char* word, int clueWpm) {
    String lower = word;
    lower.toLowerCase();
    String el = generateCWword(lower);
    strncpy(cw.elements, el.c_str(), sizeof(cw.elements) - 1);
    cw.elements[sizeof(cw.elements) - 1] = '\0';
    cw.len     = strlen(cw.elements);
    cw.pos     = 0;
    cw.playing = true;
    cw.toneOn  = false;
    cw.done    = false;
    cw.timer   = millis();
    cw.pitch   = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    cw.ditMs   = 1200 / clueWpm;
    cw.dahMs   = 3 * cw.ditMs;
    cw.charMs  = 3 * cw.ditMs;
}

static void cwStop() {
    if (cw.toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cw.toneOn = false;
    }
    cw.playing = false;
}

static void cwUpdate() {
    if (!cw.playing) return;

    // Mute while the player is keying or has started entering a guess.
    if (keyerState != IDLE_STATE || guessLen > 0) {
        if (cw.toneOn) {
            MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
            cw.toneOn = false;
        }
        return;
    }

    if (millis() < cw.timer) return;

    if (cw.toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cw.toneOn = false;
        cw.timer  = millis() + cw.ditMs - 7;     // inter-element gap
        return;
    }

    if (cw.pos >= cw.len) {                       // one play-through done
        cw.playing = false;
        cw.done    = true;
        return;
    }

    char e = cw.elements[cw.pos++];
    switch (e) {
        case '1':
            MorseOutput::pwmTone(cw.pitch, MorsePreferences::sidetoneVolume, false);
            cw.toneOn = true;
            cw.timer  = millis() + cw.ditMs - 7;
            break;
        case '2':
            MorseOutput::pwmTone(cw.pitch, MorsePreferences::sidetoneVolume, false);
            cw.toneOn = true;
            cw.timer  = millis() + cw.dahMs - 7;
            break;
        case '0':
            cw.timer = millis() + cw.charMs;
            break;
        default:
            break;
    }
}

//=============================================================================
// Round logic
//=============================================================================

// GDD speed schedule: 48,43,38,33,28,23 then 18 WPM floor.
static int clueWpmForRound(int r) {
    int w = MSL_START_WPM - (r - 1) * MSL_WPM_STEP;
    return w < MSL_MIN_WPM ? MSL_MIN_WPM : w;
}

// Player did something — reset the idle clocks (replay + exit) AND the
// global power-off Time-Out, so an active player is never kicked out.
static void noteActivity() {
    idleSince    = millis();
    nextReplayAt = millis() + MSL_IDLE_REPLAY_MS;
    MorseOutput::resetTOT();
}

// Pick the game's words (no repetition) and reset the running score.
// Reset per-game counters shared by single-player and multiplayer.
static void resetGameCounters() {
    wordIndex    = 0;
    totalAdjMs   = 0;
    totalGuesses = 0;
    solvedCount  = 0;
    skippedCount = 0;
    gameKoch     = MorsePreferences::kochFilter;   // record for high scores
    gameWlen     = wlenOpt;
    lastRank     = -1;
}

static void startGame() {
    gameWords = mslPoolN < MSL_GAME_WORDS ? mslPoolN : MSL_GAME_WORDS;
    // Partial Fisher-Yates: shuffle the first gameWords entries of the pool.
    for (int i = 0; i < gameWords; ++i) {
        int j = i + random(mslPoolN - i);
        MslPoolEntry t = mslPool[i];
        mslPool[i] = mslPool[j];
        mslPool[j] = t;
        gameSel[i] = mslPool[i];
        strncpy(gameWordStr[i], poolWord(gameSel[i]), MSL_MAX_WORD_LEN);
        gameWordStr[i][MSL_MAX_WORD_LEN] = '\0';
    }
    resetGameCounters();
}

// MP client entry: play a fixed list of words supplied by the server,
// bypassing the local pool selection.
static void startGameFromWords(const char words[][MSL_MAX_WORD_LEN + 1],
                               int count) {
    gameWords = count > MSL_GAME_WORDS ? MSL_GAME_WORDS : count;
    for (int i = 0; i < gameWords; ++i) {
        strncpy(gameWordStr[i], words[i], MSL_MAX_WORD_LEN);
        gameWordStr[i][MSL_MAX_WORD_LEN] = '\0';
    }
    resetGameCounters();
}

static void startRound() {
    const char* w = gameWordStr[wordIndex];
    strncpy(target, w, MSL_MAX_WORD_LEN);
    target[MSL_MAX_WORD_LEN] = '\0';
    for (char* p = target; *p; ++p) *p = tolower(*p);
    wordLen = strlen(target);

    revealPos = random(wordLen);
    guess[0]  = '\0';
    guessLen  = 0;
    evaluated = false;
    lastCharTime = 0;
    for (int i = 0; i < wordLen; ++i) cellState[i] = CELL_NEUTRAL;

    roundNo     = 1;
    clueWpm     = clueWpmForRound(roundNo);
    wordStartMs = millis();              // GDD: time from first clue play

    if (mpGame) {
        // GDD multiplayer per-word timeout: 20 x (play time at 18 WPM) + 10 s.
        // Approximate the CW play time as wordLen chars x ~10 dit-units/char
        // at 18 WPM (dit ~ 1200/18 ms). Generous on purpose — it's a stall
        // guard, not the expected solve time.
        unsigned long ditMs  = 1200UL / 18;
        unsigned long playMs = (unsigned long)wordLen * 10UL * ditMs;
        mpWordDeadline = millis() + 20UL * playMs + 10000UL;
    }

    gameMode = true;
    clearPaddleLatches();
    cwStart(target, clueWpm);
    noteActivity();
}

// Compare the keyed guess to the target and fill cellState[].
// Returns true if the whole word is correct.
static bool evaluateGuess() {
    bool allRight = (guessLen == wordLen);
    for (int i = 0; i < wordLen; ++i) {
        bool right = (i < guessLen && guess[i] == target[i]);
        if (right) {
            cellState[i] = CELL_GREEN;
        } else if (i == revealPos) {
            cellState[i] = CELL_GREY;     // revealed slot keyed wrong/missing
            allRight = false;
        } else {
            cellState[i] = CELL_RED;
            allRight = false;
        }
    }
    return allRight;
}

// Record the just-finished word into the running score, then advance to the
// next word or to the results screen. `skipped` => add the flat skip penalty
// and count only the guesses actually submitted (roundNo-1).
static void finishWord(bool skipped) {
    unsigned long raw = millis() - wordStartMs;
    int guesses = skipped ? (roundNo - 1) : roundNo;
    unsigned long adj = raw
                      + (unsigned long)guesses * MSL_GUESS_PENALTY_S * 1000UL
                      + (skipped ? MSL_SKIP_PENALTY_S * 1000UL : 0UL);
    totalAdjMs   += adj;
    totalGuesses += guesses;
    if (skipped) skippedCount++; else solvedCount++;

    wordIndex++;
    if (wordIndex >= gameWords) {
        cwStop();
        if (mpGame) {
            // Multiplayer: report our final score to the server and show the
            // MP results screen. No NVS high score for MP games.
            mslSendResult();
            mslState = MSL_MP_RESULTS;
        } else {
            mslRecordScore();          // insert into NVS high-score table
            mslState = MSL_RESULTS;
        }
    } else {
        startRound();
    }
}

//=============================================================================
// LOBBY
//=============================================================================

static void drawLobby() {
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(8, "MORSEL", MSL_ACCENT);
    canvas->drawFastHLine(20, 34, MSL_SCREEN_W - 40, MSL_FRAME);

    char buf[28];
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    snprintf(buf, sizeof(buf), "Koch lesson:  %d", MorsePreferences::kochFilter);
    drawCentred(44, buf, MSL_HIGHLIGHT);
    snprintf(buf, sizeof(buf), "Word length:  %s", wlenLabel[wlenOpt]);
    drawCentred(70, buf, MSL_ACCENT);

    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(96, "knob = Koch    FN = length", MSL_DIM);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(116, "Click or key to start", MSL_GREEN);
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(142, "FN-hold = high scores", MSL_DIM);
    pushFrame();
}

static void drawPoolWarning() {
    int maxLen = wlenMax[wlenOpt];
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(20, "MORSEL", MSL_ACCENT);
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(52, "Word pool too small", MSL_RED);
    char buf[40];
    snprintf(buf, sizeof(buf), "Length %s needs Koch >= %d",
             wlenLabel[wlenOpt], recKoch[maxLen]);
    drawCentred(78, buf, MSL_TEXT);
    snprintf(buf, sizeof(buf), "(now Koch %d)", MorsePreferences::kochFilter);
    drawCentred(100, buf, MSL_DIM);
    drawCentred(134, "Press to return", MSL_DIM);
    pushFrame();
}

static void lobbyLoop() {
    gameMode = false;
    gameCharBuffer = 0;
    clearPaddleLatches();
    drawLobby();

    while (mslState == MSL_LOBBY) {
        if (checkPaddles()) {
            MorseOutput::resetTOT();
            while (checkPaddles()) delay(5);
            clearPaddleLatches();
            gameCharBuffer = 0;
            mslState = MSL_PLAYING;
            return;
        }
        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            int v = constrain((int)MorsePreferences::kochFilter + enc,
                              (int)MorsePreferences::kochMinimum,
                              (int)MorsePreferences::kochMaximum);
            MorsePreferences::kochFilter = v;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawLobby();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1) {
            clearPaddleLatches();
            gameCharBuffer = 0;
            mslState = MSL_PLAYING;
            return;
        }
        if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {        // short press: word length
            wlenOpt = (wlenOpt + 1) % MSL_WLEN_OPTS;
            mslSaveWlen();
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawLobby();
        }
        if (Buttons::volButton.clicks == -1) {       // long press: high scores
            mslState = MSL_HISCORES;
            return;
        }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// PLAY
//=============================================================================

static void playLoop() {
    // Single-player builds the pool and picks words here. In multiplayer the
    // word list is already set (server picked it in the lobby and broadcast
    // it; the client received it), so skip straight to the first round.
    if (!mpGame) {
        if (buildPool() < MSL_MIN_POOL) {
            drawPoolWarning();
            while (mslState == MSL_PLAYING) {
                // Any press returns to the lobby so the player can raise the
                // Koch lesson or pick a shorter word length.
                if (checkPaddles()) { while (checkPaddles()) delay(5); mslState = MSL_LOBBY; return; }
                Buttons::modeButton.Update();
                if (Buttons::modeButton.clicks != 0) { mslState = MSL_LOBBY; return; }
                Buttons::volButton.Update();
                if (Buttons::volButton.clicks != 0) { mslState = MSL_LOBBY; return; }
                serialEvent();
                checkShutDown(false);
                delay(10);
            }
            return;
        }
        startGame();
    }
    startRound();
    drawBoard("Listen and key the word");
    unsigned long lastFrame = millis();

    while (mslState == MSL_PLAYING) {
        // --- tight keyer poll ---
        switch (keyerState) {
            case DIT: case DAH: case KEY_START: break;
            default: checkPaddles(); break;
        }
        if (doPaddleIambic(leftKey, rightKey)) { noteActivity(); continue; }
        if (keyerState != IDLE_STATE || leftKey || rightKey) noteActivity();

        if (!evaluated) {
            char c = gameCharBuffer;
            if (c != 0) {
                gameCharBuffer = 0;
                if (c == 'R') {                       // <err> prosign = backspace
                    if (guessLen > 0) guess[--guessLen] = '\0';
                    lastCharTime = millis();
                } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                    if (guessLen < wordLen) {
                        guess[guessLen++] = c;
                        guess[guessLen]   = '\0';
                    }
                    lastCharTime = millis();
                }
                // other prosigns (uppercase) are ignored
                noteActivity();
                drawBoard("Listen and key the word");
            }

            // Inter-word pause submits the guess — but only once a FULL-length
            // entry exists. Backspacing for error correction drops below full
            // length, disarming submit, so the player can pause and think
            // freely while correcting without an accidental early submit.
            if (guessLen == wordLen && lastCharTime > 0 &&
                keyerState == IDLE_STATE && !leftKey && !rightKey) {
                unsigned long wordGap = interWordSpace + ditLength;
                if (wordGap < 1200) wordGap = 1200;
                if (millis() - lastCharTime > wordGap) {
                    cwStop();
                    bool correct = evaluateGuess();
                    evaluated = true;
                    char msg[40];
                    if (correct) {
                        lastWordMs = millis() - wordStartMs;
                        snprintf(msg, sizeof(msg), "Correct!  %.1fs  %d %s",
                                 lastWordMs / 1000.0, roundNo,
                                 roundNo == 1 ? "try" : "tries");
                    } else {
                        snprintf(msg, sizeof(msg), "Try again");
                    }
                    drawBoard(msg);
                    delay(correct ? MSL_CORRECT_MS : MSL_FEEDBACK_MS);
                    if (correct) {
                        finishWord(false);          // score + advance / results
                        if (mslState == MSL_RESULTS) return;
                    } else {
                        // Same word: next round, replay the clue slower.
                        roundNo++;
                        clueWpm = clueWpmForRound(roundNo);
                        guess[0] = '\0';
                        guessLen = 0;
                        evaluated = false;
                        lastCharTime = 0;
                        for (int i = 0; i < wordLen; ++i) cellState[i] = CELL_NEUTRAL;
                        gameMode = true;
                        clearPaddleLatches();
                        cwStart(target, clueWpm);
                        noteActivity();
                    }
                    drawBoard("Listen and key the word");
                    lastFrame = millis();
                    continue;
                }
            }

            // Drive the clue (only before the player starts keying).
            if (keyerState == IDLE_STATE && guessLen == 0 &&
                !leftKey && !rightKey) {
                cwUpdate();
            }
        }

        // Exit / housekeeping at a relaxed cadence.
        if (millis() - lastFrame >= 33) {
            lastFrame = millis();

            // Encoder adjusts player keyer speed or volume (per mslEnc).
            int enc = checkEncoder();
            if (enc != 0) {
                MorseOutput::resetTOT();
                if (mslEnc == MSL_ENC_SPEED) {
                    int w = constrain((int)MorsePreferences::wpm + enc,
                                      (int)MorsePreferences::wpmMin,
                                      (int)MorsePreferences::wpmMax);
                    MorsePreferences::wpm = w;
                    updateTimings();
                } else {
                    int vol = constrain((int)MorsePreferences::sidetoneVolume + enc, 0, 20);
                    MorsePreferences::sidetoneVolume = vol;
                }
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                noteActivity();
                drawBoard("Listen and key the word");
            }

            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) { MorseOutput::resetTOT(); noteActivity(); }
            if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT; return; }
            if (Buttons::modeButton.clicks == 1) {       // short press: skip word
                cwStop();
                evaluated = true;
                char sm[24];
                snprintf(sm, sizeof(sm), "Skipped  (-%ds)", MSL_SKIP_PENALTY_S);
                drawBoard(sm);
                delay(MSL_SKIP_MS);
                finishWord(true);                        // score + advance / results
                if (mslState == MSL_RESULTS) return;
                drawBoard("Listen and key the word");
                lastFrame = millis();
                continue;
            }

            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) { MorseOutput::resetTOT(); noteActivity(); }
            if (Buttons::volButton.clicks == 1) {        // short press: toggle target
                mslEnc = (mslEnc == MSL_ENC_SPEED) ? MSL_ENC_VOLUME : MSL_ENC_SPEED;
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                drawBoard("Listen and key the word");
            }
            if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }

            // Multiplayer hard per-word deadline (GDD): if the word isn't
            // solved in time, skip it (penalty) and move on, so a stuck
            // player can't stall their own game.
            if (mpGame && millis() > mpWordDeadline) {
                finishWord(true);                 // skip -> advance or end
                if (mslState != MSL_PLAYING) return;
                drawBoard("Time! Next word");
                continue;
            }

            // Idle handling (single-player quality of life, not a penalty):
            // after a spell of no input, replay the clue; after a long total
            // idle, return gracefully to the lobby. (MP uses the deadline
            // above instead of the idle-exit.)
            if (!evaluated && guessLen == 0 && cw.done && !cw.playing) {
                if (!mpGame && millis() - idleSince > MSL_IDLE_EXIT_MS) {
                    cwStop();
                    mslState = MSL_LOBBY;
                    return;
                }
                if (millis() >= nextReplayAt) {
                    cwStart(target, clueWpm);                      // same round/speed
                    nextReplayAt = millis() + MSL_IDLE_REPLAY_MS;  // keep idleSince
                    drawBoard("Listen and key the word");
                }
            }

            serialEvent();
            checkShutDown(false);
        }
    }
}

//=============================================================================
// RESULTS
//=============================================================================

static void drawResults() {
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(8, "GAME OVER", MSL_ACCENT);
    canvas->drawFastHLine(20, 34, MSL_SCREEN_W - 40, MSL_FRAME);

    unsigned long secs = (totalAdjMs + 500) / 1000;
    char line[40];

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    snprintf(line, sizeof(line), "Time  %lu:%02lu", secs / 60, secs % 60);
    drawCentred(46, line, MSL_TEXT);

    canvas->setFont(&fonts::FreeSans9pt7b);
    snprintf(line, sizeof(line), "Solved %d / %d     Skipped %d",
             solvedCount, gameWords, skippedCount);
    drawCentred(76, line, MSL_TEXT);
    snprintf(line, sizeof(line), "Total guesses: %d   (+%ds each)",
             totalGuesses, MSL_GUESS_PENALTY_S);
    drawCentred(98, line, MSL_DIM);

    if (lastRank >= 0) {
        snprintf(line, sizeof(line), "NEW HIGH SCORE  -  rank %d", lastRank + 1);
        drawCentred(118, line, MSL_GREEN);
    }
    drawCentred(140, "click = high scores   hold = quit", MSL_DIM);
    pushFrame();
}

static void resultsLoop() {
    cwStop();
    gameMode = false;
    clearPaddleLatches();
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
    drawResults();

    while (mslState == MSL_RESULTS) {
        if (checkPaddles()) {
            MorseOutput::resetTOT();
            while (checkPaddles()) delay(5);
            clearPaddleLatches();
            mslState = MSL_HISCORES;
            return;
        }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  { mslState = MSL_HISCORES; return; }
        if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT;     return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// HIGH SCORES
//=============================================================================

// Column geometry shared by header and data rows.
#define HS_X_RANK   30      // right-aligned
#define HS_X_TIME   96      // right-aligned
#define HS_X_LEN   108      // left-aligned
#define HS_X_KOCH  186      // left-aligned
#define HS_X_SOLV  298      // right-aligned

static void hsRow(int y, const char* rk, const char* tm, const char* ln,
                   const char* kc, const char* sv, uint16_t col) {
    canvas->setTextColor(col, MSL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(rk, HS_X_RANK, y);
    canvas->drawString(tm, HS_X_TIME, y);
    canvas->drawString(sv, HS_X_SOLV, y);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(ln, HS_X_LEN,  y);
    canvas->drawString(kc, HS_X_KOCH, y);
}

static void drawHiscores() {
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(2, "HIGH SCORES", MSL_ACCENT);

    canvas->setFont(&fonts::FreeSans9pt7b);
    hsRow(28, "#", "Time", "Length", "Koch", "Solved", MSL_DIM);
    canvas->drawFastHLine(8, 45, MSL_SCREEN_W - 16, MSL_FRAME);

    bool any = false;
    int y = 49;
    for (int i = 0; i < MSL_HI_N; ++i) {
        if (hiTable[i].adjMs == 0xFFFFFFFFUL) continue;
        any = true;
        unsigned long s = (hiTable[i].adjMs + 500) / 1000;
        char rk[4], tm[8], ln[8], kc[6], sv[8];
        snprintf(rk, sizeof(rk), "%d", i + 1);
        snprintf(tm, sizeof(tm), "%lu:%02lu", s / 60, s % 60);
        snprintf(ln, sizeof(ln), "%s", wlenLabel[hiTable[i].wlen]);
        snprintf(kc, sizeof(kc), "K%d", hiTable[i].koch);
        snprintf(sv, sizeof(sv), "%d/%d", hiTable[i].solved, hiTable[i].total);
        hsRow(y, rk, tm, ln, kc, sv, i == lastRank ? MSL_GREEN : MSL_TEXT);
        y += 16;
    }
    if (!any) {
        canvas->setTextDatum(lgfx::top_center);
        canvas->setTextColor(MSL_DIM, MSL_BG);
        canvas->drawString("No scores yet", MSL_SCREEN_W / 2, 80);
    }
    canvas->setFont(&fonts::Font0);
    canvas->setTextDatum(lgfx::top_center);
    canvas->setTextColor(MSL_DIM, MSL_BG);
    canvas->drawString("press to continue", MSL_SCREEN_W / 2, 161);
    canvas->setTextDatum(lgfx::top_left);
    pushFrame();
}

static void hiscoresLoop() {
    cwStop();
    gameMode = false;
    clearPaddleLatches();
    drawHiscores();

    while (mslState == MSL_HISCORES) {
        if (checkPaddles()) {
            MorseOutput::resetTOT();
            while (checkPaddles()) delay(5);
            clearPaddleLatches();
            mslState = MSL_LOBBY;
            return;
        }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  { mslState = MSL_LOBBY; return; }
        if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT;  return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks != 0)  { mslState = MSL_LOBBY; return; }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}


//=============================================================================
// Multiplayer networking (ESP-NOW broadcast — protocol magic "MSL")
//=============================================================================
//
// Pure broadcast, no pairing. The sender MAC from the receive callback is the
// stable key for a client. Packet layout:
//   [0..2] 'M''S''L'   [3] proto ver   [4] type   [5..] payload

#define MSL_PROTO_VER   1
#define MSL_PKT_BEACON  1     // server -> all: settings + state
#define MSL_PKT_JOIN    2     // client -> server: presence + identity
#define MSL_PKT_START   3     // server -> all: begin (P2.2 will carry words)

#define MSL_PKT_RESULT  4     // client -> server: final score for this game
#define MSL_PKT_SCORES  5     // server -> all: ranked score table

#define MSL_IDENT_LEN   8
#define MSL_PKTMAX     200    // must fit the START word list (count + ~10 words)
#define MSL_RING        8
#define MSL_MAXPEERS   20     // GDD soft cap
#define MSL_RANK_BCAST 12     // max ranked entries we broadcast (limited by packet size)
#define MSL_RANK_SHOW   6     // max ranked entries we render on screen
#define MSL_SCORES_MS 1000UL  // server rebroadcast interval for the score table
#define MSL_PEER_TTL  8000UL  // drop a peer not heard from in this long
#define MSL_BEACON_MS  700UL  // server beacon interval
#define MSL_JOIN_MS   1000UL  // client join/keepalive interval

namespace MorseMorsel { bool morselNetMode = false; }

enum MslRole : uint8_t { MP_NONE, MP_SERVER, MP_CLIENT };
static MslRole       mpRole = MP_NONE;
static uint8_t       mpMenuSel = 0;            // shared two-item picker index

static char          myIdent[MSL_IDENT_LEN + 1];

// Word list received from the server's START packet (client side).
static char          mpWords[MSL_GAME_WORDS][MSL_MAX_WORD_LEN + 1];
static int           mpWordCount = 0;

// Lock-free-ish receive ring: producer = ESP-NOW RX task, consumer = game loop.
struct MslRx { uint8_t mac[6]; uint8_t len; uint8_t d[MSL_PKTMAX]; };
static volatile MslRx   mslRing[MSL_RING];
static volatile uint8_t mslRingHead = 0, mslRingTail = 0;

// Server-side roster of joined clients (keyed by MAC), plus the final result
// each client reports at game end (P2.2 collects; P2.3 ranks/broadcasts).
struct MslPeer { uint8_t mac[6]; char ident[MSL_IDENT_LEN + 1];
                 unsigned long lastHeard; bool used;
                 bool reported; uint32_t resAdjMs; uint16_t resGuesses;
                 uint8_t resSolved; };
static MslPeer       mslPeers[MSL_MAXPEERS];

// Client-side view of the server's broadcast settings.
static bool          mpSrvSeen = false;
static unsigned long mpSrvLast = 0;
static uint8_t       mpSrvWlen = 0, mpSrvKoch = 0, mpSrvWords = 0, mpSrvState = 0;
static char          mpSrvIdent[MSL_IDENT_LEN + 1] = "";

// Ranked score table. Filled by the server from its own result + every
// reported peer, then broadcast as MSL_PKT_SCORES. Clients store the
// last received broadcast here. Both sides render from the same array.
struct MslRank { uint32_t adjMs; uint16_t guesses; uint8_t solved;
                 char ident[MSL_IDENT_LEN + 1]; };
static MslRank       mpRank[MSL_RANK_BCAST];
static uint8_t       mpRankN = 0;
static bool          mpRankSeen = false;        // any ranking shown yet?
static bool          mpRankDirty = false;       // server: rebroadcast pending
static unsigned long mpRankLastBcast = 0;       // server: last SCORES tx

void MorseMorsel::mslNetOnRecv(const uint8_t* mac, const uint8_t* data,
                               uint8_t len) {
    uint8_t h = mslRingHead;
    uint8_t nxt = (h + 1) % MSL_RING;
    if (nxt == mslRingTail) return;            // ring full: drop
    memcpy((void*)mslRing[h].mac, mac, 6);
    uint8_t n = len > MSL_PKTMAX ? MSL_PKTMAX : len;
    mslRing[h].len = n;
    memcpy((void*)mslRing[h].d, data, n);
    mslRingHead = nxt;
}

static void mslLoadIdent() {
    Preferences p;
    p.begin("morserino", true);
    String call = p.getString("playerCall", "");
    String name = p.getString("playerName", "");
    p.end();
    String id = call.length() ? call : name;
    if (!id.length()) {                        // fall back to a MAC tag
        uint8_t m[6]; WiFi.macAddress(m);
        char t[10];
        snprintf(t, sizeof(t), "M-%02X%02X", m[4], m[5]);
        id = t;
    }
    id.toUpperCase();
    strncpy(myIdent, id.c_str(), MSL_IDENT_LEN);
    myIdent[MSL_IDENT_LEN] = '\0';
}

static void mslNetReset() {
    mslRingHead = mslRingTail = 0;
    for (int i = 0; i < MSL_MAXPEERS; ++i) mslPeers[i].used = false;
    mpSrvSeen = false;
    mpRankN = 0;
    mpRankSeen = false;
    mpRankDirty = false;
    mpRankLastBcast = 0;
}

static void mslSend(uint8_t type, const uint8_t* payload, uint8_t plen) {
    uint8_t buf[MSL_PKTMAX];
    buf[0] = 'M'; buf[1] = 'S'; buf[2] = 'L';
    buf[3] = MSL_PROTO_VER;
    buf[4] = type;
    uint8_t n = plen > (MSL_PKTMAX - 5) ? (MSL_PKTMAX - 5) : plen;
    if (n) memcpy(buf + 5, payload, n);
    quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, buf, 5 + n);
}

static void mslSendBeacon(uint8_t state) {
    uint8_t pl[4 + MSL_IDENT_LEN + 1];
    pl[0] = wlenOpt;
    pl[1] = MorsePreferences::kochFilter;
    pl[2] = (uint8_t)(mslPoolN < MSL_GAME_WORDS ? mslPoolN : MSL_GAME_WORDS);
    pl[3] = state;
    memcpy(pl + 4, myIdent, MSL_IDENT_LEN + 1);
    mslSend(MSL_PKT_BEACON, pl, sizeof(pl));
}

static void mslSendJoin() {
    uint8_t pl[MSL_IDENT_LEN + 1];
    memcpy(pl, myIdent, MSL_IDENT_LEN + 1);
    mslSend(MSL_PKT_JOIN, pl, sizeof(pl));
}

// START carries the word list the clients will play:
//   [0] count   then per word: [len][chars...]
static void mslSendStart() {
    uint8_t pl[MSL_PKTMAX - 5];
    uint8_t n = 0;
    pl[n++] = (uint8_t)gameWords;
    for (int i = 0; i < gameWords; ++i) {
        uint8_t wl = strlen(gameWordStr[i]);
        if (wl > MSL_MAX_WORD_LEN) wl = MSL_MAX_WORD_LEN;
        if ((int)(n + 1 + wl) > (int)sizeof(pl)) break;
        pl[n++] = wl;
        memcpy(pl + n, gameWordStr[i], wl);
        n += wl;
    }
    mslSend(MSL_PKT_START, pl, n);
}

// Client -> server final score: adjMs(4) guesses(2) solved(1) ident.
static void mslSendResult() {
    uint8_t pl[7 + MSL_IDENT_LEN + 1];
    uint8_t n = 0;
    uint32_t adj = (uint32_t)totalAdjMs;
    pl[n++] = adj & 0xFF;  pl[n++] = (adj >> 8) & 0xFF;
    pl[n++] = (adj >> 16) & 0xFF;  pl[n++] = (adj >> 24) & 0xFF;
    pl[n++] = (uint16_t)totalGuesses & 0xFF;
    pl[n++] = ((uint16_t)totalGuesses >> 8) & 0xFF;
    pl[n++] = (uint8_t)solvedCount;
    memcpy(pl + n, myIdent, MSL_IDENT_LEN + 1);
    n += MSL_IDENT_LEN + 1;
    mslSend(MSL_PKT_RESULT, pl, n);
}

// Server: gather own result + all reported peers, sort by adjMs ascending,
// keep at most MSL_RANK_BCAST entries. Writes into mpRank[]/mpRankN.
static void mslServerBuildRank() {
    MslRank tmp[1 + MSL_MAXPEERS];
    int n = 0;
    // Self entry first (server competes as a regular client per GDD §9 #8).
    tmp[n].adjMs   = (uint32_t)totalAdjMs;
    tmp[n].guesses = (uint16_t)totalGuesses;
    tmp[n].solved  = (uint8_t)solvedCount;
    strncpy(tmp[n].ident, myIdent, MSL_IDENT_LEN);
    tmp[n].ident[MSL_IDENT_LEN] = '\0';
    ++n;
    for (int i = 0; i < MSL_MAXPEERS; ++i) {
        if (!mslPeers[i].used || !mslPeers[i].reported) continue;
        tmp[n].adjMs   = mslPeers[i].resAdjMs;
        tmp[n].guesses = mslPeers[i].resGuesses;
        tmp[n].solved  = mslPeers[i].resSolved;
        strncpy(tmp[n].ident, mslPeers[i].ident, MSL_IDENT_LEN);
        tmp[n].ident[MSL_IDENT_LEN] = '\0';
        ++n;
    }
    // Simple insertion sort: n is tiny (<= 21) and the array is mostly small.
    for (int i = 1; i < n; ++i) {
        MslRank k = tmp[i]; int j = i - 1;
        while (j >= 0 && tmp[j].adjMs > k.adjMs) { tmp[j + 1] = tmp[j]; --j; }
        tmp[j + 1] = k;
    }
    int keep = n < MSL_RANK_BCAST ? n : MSL_RANK_BCAST;
    for (int i = 0; i < keep; ++i) mpRank[i] = tmp[i];
    mpRankN = (uint8_t)keep;
    mpRankSeen = true;
}

// Server -> all: ranked score table. Payload layout:
//   [0] count, then per entry: adjMs(4) guesses(2) solved(1) ident(9, NUL-term).
// Each entry is 16 bytes; with count=1 + 12*16 = 193 bytes <= MSL_PKTMAX-5.
static void mslSendScores() {
    uint8_t pl[1 + MSL_RANK_BCAST * (4 + 2 + 1 + MSL_IDENT_LEN + 1)];
    uint8_t n = 0;
    pl[n++] = mpRankN;
    for (int i = 0; i < mpRankN; ++i) {
        uint32_t a = mpRank[i].adjMs;
        pl[n++] = a & 0xFF;  pl[n++] = (a >> 8) & 0xFF;
        pl[n++] = (a >> 16) & 0xFF; pl[n++] = (a >> 24) & 0xFF;
        pl[n++] = mpRank[i].guesses & 0xFF;
        pl[n++] = (mpRank[i].guesses >> 8) & 0xFF;
        pl[n++] = mpRank[i].solved;
        memcpy(pl + n, mpRank[i].ident, MSL_IDENT_LEN + 1);
        n += MSL_IDENT_LEN + 1;
    }
    mslSend(MSL_PKT_SCORES, pl, n);
}

static void mslRosterAdd(const uint8_t* mac, const char* ident) {
    for (int i = 0; i < MSL_MAXPEERS; ++i)
        if (mslPeers[i].used && memcmp(mslPeers[i].mac, mac, 6) == 0) {
            mslPeers[i].lastHeard = millis();
            strncpy(mslPeers[i].ident, ident, MSL_IDENT_LEN);
            mslPeers[i].ident[MSL_IDENT_LEN] = '\0';
            return;
        }
    for (int i = 0; i < MSL_MAXPEERS; ++i)
        if (!mslPeers[i].used) {
            mslPeers[i].used = true;
            memcpy(mslPeers[i].mac, mac, 6);
            strncpy(mslPeers[i].ident, ident, MSL_IDENT_LEN);
            mslPeers[i].ident[MSL_IDENT_LEN] = '\0';
            mslPeers[i].lastHeard = millis();
            return;                            // (silently ignore > cap)
        }
}

static void mslRosterRecordResult(const uint8_t* mac, const char* ident,
                                  uint32_t adjMs, uint16_t guesses,
                                  uint8_t solved) {
    mslRosterAdd(mac, ident);              // ensure peer exists + refresh
    for (int i = 0; i < MSL_MAXPEERS; ++i)
        if (mslPeers[i].used && memcmp(mslPeers[i].mac, mac, 6) == 0) {
            mslPeers[i].reported   = true;
            mslPeers[i].resAdjMs   = adjMs;
            mslPeers[i].resGuesses = guesses;
            mslPeers[i].resSolved  = solved;
            mpRankDirty = true;            // ranking needs a refresh
            return;
        }
}

static void mslRosterAge() {
    for (int i = 0; i < MSL_MAXPEERS; ++i)
        if (mslPeers[i].used && millis() - mslPeers[i].lastHeard > MSL_PEER_TTL)
            mslPeers[i].used = false;
}

static int mslRosterCount() {
    int n = 0;
    for (int i = 0; i < MSL_MAXPEERS; ++i) if (mslPeers[i].used) ++n;
    return n;
}

// Drain the receive ring and update server roster / client server-view.
// Returns true if a START packet was seen (client side).
static bool mslNetPump() {
    bool started = false;
    while (mslRingTail != mslRingHead) {
        uint8_t t = mslRingTail;
        uint8_t  len = mslRing[t].len;
        uint8_t  d[MSL_PKTMAX];
        uint8_t  mac[6];
        memcpy(mac, (const void*)mslRing[t].mac, 6);
        memcpy(d,   (const void*)mslRing[t].d, len);
        mslRingTail = (t + 1) % MSL_RING;

        if (len < 5 || d[0] != 'M' || d[1] != 'S' || d[2] != 'L') continue;
        if (d[3] != MSL_PROTO_VER) continue;
        uint8_t type = d[4];
        const uint8_t* pl = d + 5;
        uint8_t pn = len - 5;

        if (mpRole == MP_SERVER && type == MSL_PKT_JOIN && pn >= 1) {
            char id[MSL_IDENT_LEN + 1];
            strncpy(id, (const char*)pl, MSL_IDENT_LEN);
            id[MSL_IDENT_LEN] = '\0';
            mslRosterAdd(mac, id);
        } else if (mpRole == MP_CLIENT && type == MSL_PKT_BEACON && pn >= 4) {
            mpSrvSeen  = true;
            mpSrvLast  = millis();
            mpSrvWlen  = pl[0];
            mpSrvKoch  = pl[1];
            mpSrvWords = pl[2];
            mpSrvState = pl[3];
            if (pn >= 5) {
                strncpy(mpSrvIdent, (const char*)(pl + 4), MSL_IDENT_LEN);
                mpSrvIdent[MSL_IDENT_LEN] = '\0';
            }
        } else if (mpRole == MP_CLIENT && type == MSL_PKT_START) {
            // Parse the word list: [count] then [len][chars]...
            mpWordCount = 0;
            if (pn >= 1) {
                uint8_t cnt = pl[0];
                uint8_t off = 1;
                for (uint8_t i = 0; i < cnt && mpWordCount < MSL_GAME_WORDS; ++i) {
                    if (off >= pn) break;
                    uint8_t wl = pl[off++];
                    if (wl > MSL_MAX_WORD_LEN || (uint16_t)off + wl > pn) break;
                    memcpy(mpWords[mpWordCount], pl + off, wl);
                    mpWords[mpWordCount][wl] = '\0';
                    off += wl;
                    mpWordCount++;
                }
            }
            started = true;
        } else if (mpRole == MP_CLIENT && type == MSL_PKT_SCORES && pn >= 1) {
            uint8_t cnt = pl[0];
            if (cnt > MSL_RANK_BCAST) cnt = MSL_RANK_BCAST;
            uint16_t off = 1;
            uint8_t k = 0;
            for (uint8_t i = 0; i < cnt; ++i) {
                if ((uint16_t)off + 7 + MSL_IDENT_LEN + 1 > pn) break;
                uint32_t a = (uint32_t)pl[off] | ((uint32_t)pl[off+1] << 8) |
                             ((uint32_t)pl[off+2] << 16) | ((uint32_t)pl[off+3] << 24);
                off += 4;
                uint16_t g = (uint16_t)pl[off] | ((uint16_t)pl[off+1] << 8);
                off += 2;
                uint8_t  s = pl[off++];
                mpRank[k].adjMs   = a;
                mpRank[k].guesses = g;
                mpRank[k].solved  = s;
                memcpy(mpRank[k].ident, pl + off, MSL_IDENT_LEN);
                mpRank[k].ident[MSL_IDENT_LEN] = '\0';
                off += MSL_IDENT_LEN + 1;
                ++k;
            }
            mpRankN = k;
            mpRankSeen = true;
        } else if (mpRole == MP_SERVER && type == MSL_PKT_RESULT && pn >= 7) {
            uint32_t adj = (uint32_t)pl[0] | ((uint32_t)pl[1] << 8) |
                           ((uint32_t)pl[2] << 16) | ((uint32_t)pl[3] << 24);
            uint16_t g  = (uint16_t)pl[4] | ((uint16_t)pl[5] << 8);
            uint8_t  sv = pl[6];
            char id[MSL_IDENT_LEN + 1] = "";
            if (pn >= 8) {
                strncpy(id, (const char*)(pl + 7), MSL_IDENT_LEN);
                id[MSL_IDENT_LEN] = '\0';
            }
            mslRosterRecordResult(mac, id, adj, g, sv);
        }
    }
    return started;
}

//=============================================================================
// MODE SELECT  (single-player vs multiplayer)
//=============================================================================

static void drawMenuList(const char* title, const char* a, const char* b,
                         uint8_t sel) {
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(16, title, MSL_ACCENT);
    canvas->setFont(&fonts::FreeSans12pt7b);
    drawCentred(64,  a, sel == 0 ? MSL_HIGHLIGHT : MSL_DIM);
    drawCentred(98,  b, sel == 1 ? MSL_HIGHLIGHT : MSL_DIM);
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(140, "turn=move  click=ok  hold=back", MSL_DIM);
    pushFrame();
}

static void modeSelLoop() {
    gameMode = false;
    clearPaddleLatches();
    bool dirty = true;
    while (mslState == MSL_MODESEL) {
        if (dirty) { drawMenuList("Morsel", "Single player", "Multiplayer", mpMenuSel);
                     dirty = false; }
        int e = checkEncoder();
        if (e != 0) { MorseOutput::resetTOT(); mpMenuSel ^= 1; dirty = true;
                      MorseOutput::pwmClick(MorsePreferences::sidetoneVolume); }
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1) {
            if (mpMenuSel == 0) { mpGame = false; mslState = MSL_LOBBY; return; }  // single player
            // Multiplayer: bring ESP-NOW up (the reboot guard is gone — the
            // QuickEspNow fork survives repeated cycles). The 8-bpp sprite
            // (~52 KB) and ESP-NOW now coexist with comfortable headroom, so
            // the sprite stays allocated and the MP lobby is drawn normally.
            if (!EspNowIsActive) MorseMenu::setupESPNow();
            MorseMorsel::morselNetMode = true;
            mslLoadIdent();
            mslNetReset();
            mpRole = MP_NONE;
            mpMenuSel = 0;
            mslState = MSL_MP_LOBBY;
            return;
        }
        if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT; return; }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }
        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// MULTIPLAYER LOBBY  (role pick -> server roster / client wait)
//=============================================================================

static void drawMpServer() {
    canvas->fillSprite(MSL_BG);
    char b[28];
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    snprintf(b, sizeof(b), "Server  L%s  K%d",
             wlenLabel[wlenOpt], MorsePreferences::kochFilter);
    drawCentred(8, b, MSL_ACCENT);

    canvas->setFont(&fonts::FreeSans9pt7b);
    int n = mslRosterCount();
    snprintf(b, sizeof(b), "Joined: %d", n);
    canvas->setTextColor(n ? MSL_GREEN : MSL_DIM, MSL_BG);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(b, 10, 40);

    // Roster idents, two columns.
    int shown = 0;
    for (int i = 0; i < MSL_MAXPEERS && shown < 10; ++i) {
        if (!mslPeers[i].used) continue;
        int col = shown % 2, row = shown / 2;
        canvas->setTextColor(MSL_TEXT, MSL_BG);
        canvas->drawString(mslPeers[i].ident, 14 + col * 150, 62 + row * 18);
        ++shown;
    }
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(150, "click=START  vol=len  hold=back", MSL_DIM);
    pushFrame();
}

static void drawMpClient() {
    canvas->fillSprite(MSL_BG);
    char b[32];
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(12, "Client", MSL_ACCENT);
    canvas->setFont(&fonts::FreeSans12pt7b);
    if (!mpSrvSeen || millis() - mpSrvLast > MSL_PEER_TTL) {
        drawCentred(60, "Searching for server...", MSL_DIM);
    } else {
        snprintf(b, sizeof(b), "Server: %s", mpSrvIdent);
        drawCentred(54, b, MSL_GREEN);
        canvas->setFont(&fonts::FreeSans9pt7b);
        snprintf(b, sizeof(b), "Length %s   Koch %d   %d words",
                 wlenLabel[mpSrvWlen % MSL_WLEN_OPTS], mpSrvKoch, mpSrvWords);
        drawCentred(90, b, MSL_TEXT);
        drawCentred(112, "waiting for server to start", MSL_DIM);
    }
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(150, "hold = back", MSL_DIM);
    pushFrame();
}

static void mpLobbyLoop() {
    gameMode = false;
    clearPaddleLatches();
    unsigned long lastBeacon = 0, lastJoin = 0, lastDraw = 0;
    bool dirty = true;

    while (mslState == MSL_MP_LOBBY) {
        // ---- role not chosen yet ----
        if (mpRole == MP_NONE) {
            if (dirty) { drawMenuList("Multiplayer", "Start as Server",
                                      "Join a game", mpMenuSel); dirty = false; }
            int e = checkEncoder();
            if (e != 0) { MorseOutput::resetTOT(); mpMenuSel ^= 1; dirty = true;
                          MorseOutput::pwmClick(MorsePreferences::sidetoneVolume); }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == 1) {
                if (mpMenuSel == 0) {
                    mpRole = MP_SERVER;
                    // A server session starts at a full-alphabet default
                    // (Koch 41) regardless of the operator's personal lesson,
                    // so a multiplayer game uses the whole character set by
                    // default; the operator can still dial it back with the
                    // encoder. (Single-player keeps the user's current lesson.)
                    // savedKochFilter restores the real value on Morsel exit.
                    MorsePreferences::kochFilter =
                        constrain(41, (int)MorsePreferences::kochMinimum,
                                      (int)MorsePreferences::kochMaximum);
                    buildPool();
                } else {
                    mpRole = MP_CLIENT;
                }
                mslNetReset();
                lastBeacon = lastJoin = 0; dirty = true;
            }
            if (Buttons::modeButton.clicks == -1) { mslState = MSL_MODESEL; return; }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }
            serialEvent(); checkShutDown(false); delay(10);
            continue;
        }

        // ---- active role ----
        bool started = mslNetPump();

        if (mpRole == MP_SERVER) {
            mslRosterAge();
            if (millis() - lastBeacon > MSL_BEACON_MS) {
                mslSendBeacon(0);
                lastBeacon = millis();
            }
            int e = checkEncoder();
            if (e != 0) {
                MorseOutput::resetTOT();
                int v = constrain((int)MorsePreferences::kochFilter + e,
                                  (int)MorsePreferences::kochMinimum,
                                  (int)MorsePreferences::kochMaximum);
                MorsePreferences::kochFilter = v;
                buildPool();
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                dirty = true;
            }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == 1) {       // START
                for (int i = 0; i < MSL_MAXPEERS; ++i)   // clear last game's scores
                    mslPeers[i].reported = false;
                startGame();          // pick the words (fills gameWordStr)
                mslSendStart();        // broadcast the word list to clients
                mpGame = true;
                mslState = MSL_PLAYING; return;
            }
            if (Buttons::modeButton.clicks == -1) {      // back to role pick
                mpRole = MP_NONE; mpMenuSel = 0; dirty = true; continue;
            }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::volButton.clicks == 1) {        // word length
                wlenOpt = (wlenOpt + 1) % MSL_WLEN_OPTS;
                mslSaveWlen(); buildPool();
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                dirty = true;
            }
            if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }
            if (dirty || millis() - lastDraw > 500) {
                drawMpServer(); dirty = false; lastDraw = millis();
            }
        } else {  // MP_CLIENT
            if (millis() - lastJoin > MSL_JOIN_MS) {
                mslSendJoin();
                lastJoin = millis();
            }
            if (started && mpWordCount > 0) {       // server began the game
                startGameFromWords(mpWords, mpWordCount);
                mpGame = true;
                mslState = MSL_PLAYING; return;
            }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == -1) {
                mpRole = MP_NONE; mpMenuSel = 0; dirty = true; continue;
            }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks == -1) { mslState = MSL_EXIT; return; }
            if (dirty || millis() - lastDraw > 400) {
                drawMpClient(); dirty = false; lastDraw = millis();
            }
        }
        serialEvent();
        checkShutDown(false);
        delay(8);
    }
}

//=============================================================================
// MULTIPLAYER RESULTS  (own score + scores collected from clients)
//=============================================================================

static void drawMpResults() {
    canvas->fillSprite(MSL_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(6, "Ranking", MSL_ACCENT);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::top_left);

    if (!mpRankSeen) {
        drawCentred(80, mpRole == MP_SERVER ? "waiting for players..."
                                            : "waiting for ranking...",
                    MSL_DIM);
    } else {
        int shown = mpRankN < MSL_RANK_SHOW ? mpRankN : MSL_RANK_SHOW;
        int y = 34;
        char b[48];
        for (int i = 0; i < shown; ++i) {
            bool isSelf = strncmp(mpRank[i].ident, myIdent,
                                  MSL_IDENT_LEN) == 0;
            unsigned long s = (mpRank[i].adjMs + 500) / 1000;
            snprintf(b, sizeof(b), "%2d  %-8s  %4lus  %d/%d  %dg",
                     i + 1, mpRank[i].ident, s,
                     mpRank[i].solved, gameWords, mpRank[i].guesses);
            canvas->setTextColor(isSelf ? MSL_HIGHLIGHT : MSL_TEXT, MSL_BG);
            canvas->drawString(b, 10, y);
            y += 18;
        }
        if (mpRankN > shown) {
            snprintf(b, sizeof(b), "+%d more", mpRankN - shown);
            canvas->setTextColor(MSL_DIM, MSL_BG);
            canvas->drawString(b, 10, y);
        }
    }
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentred(150, "click=lobby  hold=quit", MSL_DIM);
    pushFrame();
}

static void mpResultsLoop() {
    cwStop();
    gameMode = false;
    clearPaddleLatches();
    unsigned long lastDraw = 0, lastResend = 0;

    // Server seeds an initial ranking containing only its own result so
    // something is shown immediately; gets enriched as RESULTs trickle in.
    if (mpRole == MP_SERVER) {
        mslServerBuildRank();
        mslSendScores();
        mpRankLastBcast = millis();
        mpRankDirty = false;
    }
    drawMpResults();
    while (mslState == MSL_MP_RESULTS) {
        mslNetPump();                       // server collects client RESULTs
        // Client resends its result periodically in case the first was lost.
        if (mpRole == MP_CLIENT && millis() - lastResend > 1000) {
            mslSendResult();
            lastResend = millis();
        }
        // Server: rebuild + rebroadcast on new arrivals (dirty) or
        // periodically as a keepalive in case the last broadcast was lost.
        if (mpRole == MP_SERVER &&
            (mpRankDirty || millis() - mpRankLastBcast > MSL_SCORES_MS)) {
            if (mpRankDirty) mslServerBuildRank();
            mslSendScores();
            mpRankLastBcast = millis();
            mpRankDirty = false;
            drawMpResults();                // reflect the new ranking now
            lastDraw = millis();
        }
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  { mpGame = false; mslState = MSL_MP_LOBBY; return; }
        if (Buttons::modeButton.clicks == -1) { mslState = MSL_EXIT; return; }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1)  { mslState = MSL_EXIT; return; }
        if (millis() - lastDraw > 400) { drawMpResults(); lastDraw = millis(); }
        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// Public entry point
//=============================================================================

void MorseMorsel::run() {
    canvas = MorseGameMode::enterLandscape(MorsePreferences::leftHanded, 8);
    savedKochFilter = MorsePreferences::kochFilter;   // restored on exit
    mslEnc = MSL_ENC_SPEED;
    mslLoadPrefs();                                   // word length + scores
    mpMenuSel = 0;
    mpGame    = false;
    mslState  = MSL_MODESEL;                           // pick single vs multiplayer

    while (mslState != MSL_EXIT) {
        switch (mslState) {
            case MSL_MODESEL:  modeSelLoop();  break;
            case MSL_LOBBY:    lobbyLoop();    break;
            case MSL_PLAYING:  playLoop();     break;
            case MSL_RESULTS:  resultsLoop();  break;
            case MSL_HISCORES: hiscoresLoop(); break;
            case MSL_MP_LOBBY:   mpLobbyLoop();   break;
            case MSL_MP_RESULTS: mpResultsLoop(); break;
            default:             mslState = MSL_EXIT; break;
        }
    }

    // Stop routing ESP-NOW packets to Morsel, and tear down the radio we
    // brought up for multiplayer. This must happen here, not lean on
    // menu_()'s teardown: returning from a game keeps us inside the still-
    // running menu_() loop, so its entry-time WiFi/ESP-NOW teardown does NOT
    // re-run between two game launches. If ESP-NOW were left up, re-entering
    // Morsel would allocate the 52 KB sprite against a heap still fragmented
    // by ESP-NOW (free is ample, but the largest contiguous block drops just
    // below the sprite size) and the allocation would fail into a reboot.
    MorseMorsel::morselNetMode = false;
    mpRole = MP_NONE;
    if (EspNowIsActive) {
        quickEspNow.stop();
        EspNowIsActive = false;
        WiFi.mode(WIFI_OFF);
    }

    cwStop();
    keyOut(false, true, 0, 0);
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
    gameMode = false;
    clearPaddleLatches();
    MorsePreferences::kochFilter = savedKochFilter;   // restore user's lesson
    MorseGameMode::exit();
}

void MorseMorsel::warmup() {
    // No persistent String buffers yet.
}

#endif  // CONFIG_CW_GAME

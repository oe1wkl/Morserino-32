/******************************************************************************************************************************
 *  Memory Chain — implementation (see header for scope).
 *****************************************************************************************************************************/

#include "MorseMemoryChain.h"
#include "MorseGameMode.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "morsedefs.h"
#include "ClickButton.h"
#include "GamePalette.h"
#include "MorseGame.h"          // gameMode, gameCharBuffer
#include "MorseCwEngine.h"

#include <Preferences.h>
#include <LovyanGFX.hpp>

// ---- External references (same ad-hoc pattern every other game file uses) ----
extern int     checkEncoder();
extern void    checkShutDown(boolean);
extern void    serialEvent();
extern boolean checkPaddles();
extern void    clearPaddleLatches();
extern boolean doPaddleIambic(boolean, boolean);
extern boolean leftKey, rightKey;
extern String  getRandomCall(int maxLength);

//=============================================================================
// Layout constants — own copies, matching every other game file's convention.
//=============================================================================

#define MC_SCREEN_W   304
#define MC_SCREEN_H   170
#define MC_HUD_H       20

#define MC_BOX         20       // box side
#define MC_PITCH_X     25       // horizontal box pitch (box + gap)
#define MC_PERROW      12       // boxes per row — a call is never longer than
                                // 12 chars (getRandomCall caps at 10 + "/p"),
                                // so a call always fits one row
#define MC_ROWS         4
#define MC_MAX_CHAIN  (MC_PERROW * MC_ROWS)   // 48 — beyond human memory
#define MC_PITCH_Y     25       // vertical row pitch
#define MC_ROWS_Y      58       // first box row top
#define MC_ROWS_X    ((MC_SCREEN_W - (MC_PERROW * MC_PITCH_X - (MC_PITCH_X - MC_BOX))) / 2)

#define MC_ROUND_PAUSE_MS   600UL   // full-green row stays visible this long
#define MC_CALL_PAUSE_MS    900UL   // completed call (letters revealed) pause

//=============================================================================
// Module state
//=============================================================================

enum McState : uint8_t { MC_LOBBY, MC_PLAYING, MC_OVER, MC_EXIT };
enum McEnc   : uint8_t { MC_ENC_SPEED, MC_ENC_VOLUME };
enum McField : uint8_t { MC_FLD_MODE, MC_FLD_PROMPT, MC_FLD_KOCH };

static LGFX_Sprite *canvas = nullptr;
static McState mcState;
static McEnc   mcEnc;           // in-game encoder target (speed/volume, §2)
static McField mcField;         // lobby encoder target (FN click cycles it)

// Lobby settings, persisted immediately (one byte in the shared game NVS
// namespace — CLAUDE.md §4: reuse an existing namespace, don't add more).
static bool mcCallsMode   = false;   // false = Characters, true = Call Signs
static bool mcSoundPrompt = false;   // false = Display, true = Sound

// Game state. In Characters mode chain[] holds random Koch-set characters;
// in Call Signs mode it is the revealed-so-far prefix of call[].
static char    chain[MC_MAX_CHAIN + 1];
static int     chainLen;
static int     pos;             // next chain index to key this round
static int     errPos;          // Characters mode: tolerated error this round (-1 = none)
static int     totalErrs;       // Characters mode: tolerated errors over the whole game
static bool    promptShown;     // Display prompt still on screen
static String  pool;            // Characters mode: Koch chars minus prosign markers
static char    call[16];        // Call Signs mode: the full current call
static int     callLen;
static int     callsDone;
static int     deathPos;        // chain index of the fatal error (-1 = ended without one)

//=============================================================================
// High scores — same versioned-blob pattern as MorseGridScore, in the same
// "gridgame" NVS namespace under its own keys. Two independent tables: the
// two content modes score different things (chain length vs completed calls)
// and are not comparable.
//=============================================================================

#define MC_HI_N     7           // table size (fits the screen, like Morsel)
#define MC_HI_VER   1           // bump if ChainScore layout changes
#define MC_NS       "gridgame"

// Persisted per row. Layout is serialised verbatim to NVS — bump MC_HI_VER
// if this ever changes.
struct ChainScore {
    uint16_t primary;    // Characters: completed chain length; Calls: completed calls
    uint16_t secondary;  // rank tiebreak — Characters: boxes reached in the failed
                         // round; Calls: letters banked in the failed call
    uint16_t errs;       // Characters: tolerated errors used; Calls: 0
    uint8_t  koch;       // Characters: Koch lesson; Calls: 0 (full character set)
    uint8_t  prompt;     // 0 = Display, 1 = Sound
};

static ChainScore  hiTables[2][MC_HI_N];             // [0] = Characters, [1] = Calls
static bool        hiLoaded[2] = { false, false };
static const char* kData[2]    = { "mc",  "mcc" };
static const char* kVer[2]     = { "mcv", "mccv" };

static void ensureLoaded(int m) {
    if (hiLoaded[m]) return;
    for (int i = 0; i < MC_HI_N; ++i) hiTables[m][i] = ChainScore{};   // empty = all-zero
    Preferences p;
    p.begin(MC_NS, true);
    if (p.getUChar(kVer[m], 0) == MC_HI_VER)          // absent/old stamp -> stay empty
        p.getBytes(kData[m], hiTables[m], sizeof(hiTables[m]));
    p.end();
    hiLoaded[m] = true;
}

static void saveTable(int m) {
    Preferences p;
    p.begin(MC_NS, false);
    boolean ok = (p.putBytes(kData[m], hiTables[m], sizeof(hiTables[m])) == sizeof(hiTables[m]));
    ok &= (p.putUChar(kVer[m], MC_HI_VER) != 0);
    p.end();
    if (!ok)                            // put*() fails silently when NVS is full
        Serial.println("Memory Chain: high scores NOT saved - NVS full?");
}

// Insert if it qualifies (rank by primary, then secondary), persist on
// qualification, return the 0-based rank or -1. An all-zero score (died on
// the very first character) never displaces an empty slot.
static int recordScore(int m, const ChainScore &s) {
    ensureLoaded(m);
    if (s.primary == 0 && s.secondary == 0) return -1;
    for (int i = 0; i < MC_HI_N; ++i) {
        const ChainScore &t = hiTables[m][i];
        if (s.primary > t.primary || (s.primary == t.primary && s.secondary > t.secondary)) {
            for (int j = MC_HI_N - 1; j > i; --j) hiTables[m][j] = hiTables[m][j - 1];
            hiTables[m][i] = s;
            saveTable(m);
            return i;
        }
    }
    return -1;
}

//=============================================================================
// Lobby settings persistence (single byte, no version needed — like Morsel's
// "wlen"; an absent key just yields the defaults).
//=============================================================================

static void loadSettings() {
    Preferences p;
    p.begin(MC_NS, true);
    uint8_t b = p.getUChar("mcopt", 0);
    p.end();
    mcCallsMode   = b & 1;
    mcSoundPrompt = b & 2;
}

static void saveSettings() {
    Preferences p;
    p.begin(MC_NS, false);
    p.putUChar("mcopt", (uint8_t)((mcCallsMode ? 1 : 0) | (mcSoundPrompt ? 2 : 0)));
    p.end();
}

//=============================================================================
// CW prompt playback — MorseCwEngine at the player's live keyer speed
// (wpm=0), with the same half-tone pitch shift Echo Trainer / Trx / Fox Hunt
// use to distinguish incoming CW from the player's own sidetone. Unlike Fox
// Hunt's free-running clue, the prompt here plays to completion BLOCKING
// (nothing to do until it has been heard); paddle latches and the char
// buffer are cleared afterwards so touches during playback can't leak into
// the round as an answer.
//=============================================================================

static uint16_t promptPitchHz() {
    uint16_t pitch = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    if (MorsePreferences::pliste[posEchoToneShift].value != 0) {
        pitch = (MorsePreferences::pliste[posEchoToneShift].value == 1) ? pitch * 18 / 17 : pitch * 17 / 18;
    }
    return pitch;
}

static void playPrompt(char c) {
    char buf[2] = { c, '\0' };
    MorseCwEngine::PlayOpts opts = {
        promptPitchHz(),
        /*wpm*/           0,
        /*loop*/          false,
        /*resumeGapDits*/ 0,
        /*extraMute*/     nullptr,
    };
    MorseCwEngine::playStart(String(buf), opts);
    while (MorseCwEngine::playTick()) {
        serialEvent();
        checkShutDown(false);
        delay(3);
    }
    clearPaddleLatches();
    gameCharBuffer = 0;
}

//=============================================================================
// Keyer input — identical to Trailblazer's pollKeyedChar(), including the
// filter for the decoder's trailing inter-word space (the single-character-
// buffer form of the trailing-space gotcha, CLAUDE.md §3.3): left unfiltered
// it arrives on a later tick as a second, always-wrong "answer".
//=============================================================================

// `busy` reports whether the keyer is actively processing input — the caller
// keeps a very tight loop while keying so the keyer's element/gap timers
// aren't skewed by the idle delay (devdocs/cw-timing-audit/FINDINGS.md).
static char pollKeyedChar(bool &busy) {
    checkPaddles();
    busy = doPaddleIambic(leftKey, rightKey);
    if (busy) MorseOutput::resetTOT();
    char c = gameCharBuffer;
    if (c != 0) {
        gameCharBuffer = 0;
        MorseOutput::resetTOT();
        if (c == ' ') return 0;    // decoder's trailing space — not an answer
        return c;
    }
    return 0;
}

//=============================================================================
// Content generation
//=============================================================================

// Characters mode pool: the Koch lesson set minus the uppercase prosign
// markers (S/A/N/K/E/B — a prosign has no single glyph that fits a box).
// generateCWword() treats uppercase as prosign markers, so everything that
// stays in the pool is safe to hand to the CW engine as-is.
static void buildPool() {
    String set = koch.getCharSet();
    pool = "";
    for (unsigned int i = 0; i < set.length(); ++i) {
        char c = set.charAt(i);
        if (c >= 'A' && c <= 'Z') continue;
        pool += c;
    }
    if (pool.length() == 0) pool = "eish5";   // pathological custom set — never in practice
}

// Append one random pool character; avoid an immediate repeat (a doubled
// letter is indistinguishable from an echo when the prompt is sounded).
// Returns false when the chain is at its cap — a "perfect game".
static bool growChain() {
    if (chainLen >= MC_MAX_CHAIN) return false;
    char c;
    do {
        c = pool.charAt(random(pool.length()));
    } while (pool.length() > 1 && chainLen > 0 && c == chain[chainLen - 1]);
    chain[chainLen++] = c;
    chain[chainLen] = '\0';
    return true;
}

// Call Signs mode: fetch a fresh call and reset the chain. getRandomCall(0)
// honours the user's call-sign preferences (continent, common prefixes) and
// occasionally appends /p or /m; max length 12 = one box row. (Hard rule
// CLAUDE.md §3.7 — the generator is reused untouched.)
static void newCall() {
    String s = getRandomCall(0);
    callLen = s.length() < 16 ? (int)s.length() : 15;
    for (int i = 0; i < callLen; ++i) call[i] = s.charAt(i);
    call[callLen] = '\0';
    chainLen = 0;
}

static char dispChar(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

//=============================================================================
// Drawing
//=============================================================================

// Title + WPM/volume, bold on whichever the encoder currently owns — the
// same HUD convention as the other games (UX_CONVENTIONS.md §4/§12).
static void drawHud() {
    canvas->fillRect(0, 0, MC_SCREEN_W, MC_HUD_H, PAL_BLACK);
    canvas->drawFastHLine(0, MC_HUD_H, MC_SCREEN_W, PAL_DARKGREY);

    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextDatum(lgfx::middle_left);
    canvas->setTextColor(PAL_CYAN, PAL_BLACK);
    canvas->drawString("MEMORY CHAIN", 6, MC_HUD_H / 2 + 1);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::middle_right);
    char wbuf[16], vbuf[16];
    snprintf(wbuf, sizeof(wbuf), "%d wpm", MorsePreferences::wpm);
    snprintf(vbuf, sizeof(vbuf), "vol %d", MorsePreferences::sidetoneVolume);

    int wpmX = MC_SCREEN_W - 6;
    canvas->setTextColor(mcEnc == MC_ENC_SPEED ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(wbuf, wpmX, MC_HUD_H / 2 + 1);

    int volX = wpmX - canvas->textWidth(wbuf) - 16;
    canvas->setTextColor(mcEnc == MC_ENC_VOLUME ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(vbuf, volX, MC_HUD_H / 2 + 1);
    canvas->setTextDatum(lgfx::top_left);
}

// One box of the chain row. `letter` 0 = empty box.
static void drawBox(int i, uint16_t frame, uint16_t fill, char letter, uint16_t letterCol) {
    int x = MC_ROWS_X + (i % MC_PERROW) * MC_PITCH_X;
    int y = MC_ROWS_Y + (i / MC_PERROW) * MC_PITCH_Y;
    if (fill != PAL_BG) canvas->fillRect(x, y, MC_BOX, MC_BOX, fill);
    canvas->drawRect(x, y, MC_BOX, MC_BOX, frame);
    if (letter != 0) {
        char b[2] = { dispChar(letter), '\0' };
        canvas->setFont(&fonts::FreeSansBold9pt7b);
        canvas->setTextDatum(lgfx::middle_center);
        canvas->setTextColor(letterCol, fill);
        canvas->drawString(b, x + MC_BOX / 2, y + MC_BOX / 2 + 1);
        canvas->setTextDatum(lgfx::top_left);
    }
}

// The chain row(s) during play: green = keyed correctly (kept empty — the
// row must not become a crib sheet for the rest of the round), red with the
// correct character = the tolerated error, yellow frame = current position.
static void drawChainRow() {
    for (int i = 0; i < chainLen; ++i) {
        if (i == errPos)      drawBox(i, PAL_RED,    PAL_RED,   chain[i], PAL_WHITE);
        else if (i < pos)     drawBox(i, PAL_GREEN,  PAL_GREEN, 0,        PAL_WHITE);
        else if (i == pos)    drawBox(i, PAL_YELLOW, PAL_BG,    0,        PAL_WHITE);
        else                  drawBox(i, PAL_GREY,   PAL_BG,    0,        PAL_WHITE);
    }
}

static void drawPlay() {
    canvas->fillSprite(PAL_BG);
    drawHud();

    char buf[24];
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
    canvas->setTextDatum(lgfx::top_left);
    if (mcCallsMode) snprintf(buf, sizeof(buf), "Calls: %d", callsDone);
    else             snprintf(buf, sizeof(buf), "Chain: %d", chainLen);
    canvas->drawString(buf, 6, 30);

    if (promptShown) {          // Display prompt: the newest character, big,
        char b[2] = { dispChar(chain[chainLen - 1]), '\0' };    // until keying starts
        MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 24, b,
                                   PAL_YELLOW, PAL_BG, &fonts::FreeSansBold18pt7b);
    }

    drawChainRow();
    MorseGameMode::pushFrame();
}

// A just-completed call, letters revealed (the player earned the look).
static void drawCallComplete() {
    canvas->fillSprite(PAL_BG);
    drawHud();
    char buf[24];
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(PAL_GREEN, PAL_BG);
    canvas->setTextDatum(lgfx::top_left);
    snprintf(buf, sizeof(buf), "Calls: %d", callsDone);
    canvas->drawString(buf, 6, 30);
    for (int i = 0; i < callLen; ++i)
        drawBox(i, PAL_GREEN, PAL_GREEN, call[i], PAL_BLACK);
    MorseGameMode::pushFrame();
}

// Game-over reveal: every chain character visible; the fatal position (and a
// tolerated error of the final round) in red. In Call Signs mode the not-yet-
// revealed rest of the call is shown dimmed — what it would have been.
static void drawReveal() {
    int total = mcCallsMode ? callLen : chainLen;
    for (int i = 0; i < total; ++i) {
        char c = mcCallsMode ? call[i] : chain[i];
        if (i == deathPos || i == errPos) drawBox(i, PAL_RED,   PAL_RED,   c, PAL_WHITE);
        else if (i < pos)                 drawBox(i, PAL_GREEN, PAL_GREEN, c, PAL_BLACK);
        else if (i < chainLen)            drawBox(i, PAL_GREY,  PAL_BG,    c, PAL_WHITE);
        else                              drawBox(i, PAL_GREY,  PAL_BG,    c, PAL_GREY);
    }
}

static void drawGameOver(const ChainScore &s, int rank) {
    canvas->fillSprite(PAL_BG);
    bool win = (!mcCallsMode && deathPos < 0);        // chain cap reached
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 2,
                               win ? "Perfect!" : "Game Over",
                               win ? PAL_GREEN : PAL_RED, PAL_BG, &fonts::FreeSansBold12pt7b);
    char buf[48];
    if (mcCallsMode)
        snprintf(buf, sizeof(buf), "Calls %u  +%u      %s",
                 s.primary, s.secondary, s.prompt ? "Sound" : "Display");
    else
        snprintf(buf, sizeof(buf), "Chain %u    Err %u    Koch %u    %s",
                 s.primary, s.errs, s.koch, s.prompt ? "Sound" : "Display");
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 28, buf, PAL_WHITE, PAL_BG, &fonts::FreeSans9pt7b);
    if (rank >= 0) {
        snprintf(buf, sizeof(buf), "New High Score  -  Rank %d", rank + 1);
        MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 47, buf, PAL_GREEN, PAL_BG, &fonts::Font0);
    }
    drawReveal();
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 161, "Click: High Scores    Long press: Exit",
                               PAL_LIGHTGREY, PAL_BG, &fonts::Font0);
    MorseGameMode::pushFrame();
}

// Column geometry for the high-score tables (one layout per content mode).
static void hsRowChars(int y, const char *rk, const char *len, const char *err,
                       const char *kc, const char *pr, uint16_t col) {
    canvas->setTextColor(col, PAL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(rk,  36,  y);
    canvas->drawString(len, 104, y);
    canvas->drawString(err, 172, y);
    canvas->drawString(kc,  240, y);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(pr,  262, y);
}

static void hsRowCalls(int y, const char *rk, const char *cl, const char *pl,
                       const char *pr, uint16_t col) {
    canvas->setTextColor(col, PAL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(rk,  46,  y);
    canvas->drawString(cl,  140, y);
    canvas->drawString(pl,  214, y);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(pr,  244, y);
}

static void drawHiscores(int m, int lastRank) {
    ensureLoaded(m);
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 2,
                               m ? "HIGH SCORES - CALLS" : "HIGH SCORES - CHARS",
                               PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);

    canvas->setFont(&fonts::FreeSans9pt7b);
    if (m) hsRowCalls(28, "#", "Calls", "+Ch", "Prompt", PAL_LIGHTGREY);
    else   hsRowChars(28, "#", "Chain", "Err", "Koch", "Prompt", PAL_LIGHTGREY);
    canvas->drawFastHLine(8, 45, MC_SCREEN_W - 16, PAL_MIDBLUE);

    bool any = false;
    int y = 49;
    for (int i = 0; i < MC_HI_N; ++i) {
        const ChainScore &t = hiTables[m][i];
        if (t.primary == 0 && t.secondary == 0) continue;        // empty slot
        any = true;
        uint16_t col = (i == lastRank) ? PAL_GREEN : PAL_WHITE;
        char rk[4], a[8], b[8], kc[6];
        snprintf(rk, sizeof(rk), "%d", i + 1);
        snprintf(a,  sizeof(a),  "%u", t.primary);
        if (m) {
            snprintf(b, sizeof(b), "+%u", t.secondary);
            hsRowCalls(y, rk, a, b, t.prompt ? "S" : "D", col);
        } else {
            snprintf(b,  sizeof(b),  "%u",  t.errs);
            snprintf(kc, sizeof(kc), "K%u", t.koch);
            hsRowChars(y, rk, a, b, kc, t.prompt ? "S" : "D", col);
        }
        y += 16;
    }
    if (!any) {
        canvas->setTextDatum(lgfx::top_center);
        canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
        canvas->drawString("No scores yet", MC_SCREEN_W / 2, 80);
    }
    canvas->setFont(&fonts::Font0);
    canvas->setTextDatum(lgfx::top_center);
    canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
    canvas->drawString("press to continue", MC_SCREEN_W / 2, 161);
    canvas->setTextDatum(lgfx::top_left);
    MorseGameMode::pushFrame();
}

static void drawLobby() {
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 4, "MEMORY CHAIN", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    canvas->drawFastHLine(20, 30, MC_SCREEN_W - 40, PAL_MIDBLUE);

    char buf[32];
    snprintf(buf, sizeof(buf), "Mode:  %s", mcCallsMode ? "Call Signs" : "Characters");
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 40, buf,
                               mcField == MC_FLD_MODE ? PAL_YELLOW : PAL_WHITE, PAL_BG, &fonts::FreeSansBold12pt7b);
    snprintf(buf, sizeof(buf), "Prompt:  %s", mcSoundPrompt ? "Sound" : "Display");
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 66, buf,
                               mcField == MC_FLD_PROMPT ? PAL_YELLOW : PAL_WHITE, PAL_BG, &fonts::FreeSansBold12pt7b);
    if (!mcCallsMode) {
        snprintf(buf, sizeof(buf), "Koch lesson:  %d", MorsePreferences::kochFilter);
        MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 92, buf,
                                   mcField == MC_FLD_KOCH ? PAL_YELLOW : PAL_WHITE, PAL_BG, &fonts::FreeSansBold12pt7b);
    }

    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 126, "Key to start", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::drawCentred(canvas, MC_SCREEN_W / 2, 159, "knob = value   FN = next   click = high scores",
                               PAL_GREY, PAL_BG, &fonts::Font0);
    MorseGameMode::pushFrame();
}

//=============================================================================
// Round flow
//=============================================================================

// Grow the chain by one, present the newest character, arm the round.
static void startRound() {
    if (mcCallsMode) {
        chain[chainLen] = call[chainLen];
        chainLen++;
        chain[chainLen] = '\0';
    } else if (!growChain()) {
        deathPos = -1;                 // chain cap reached — a perfect game
        mcState = MC_OVER;
        return;
    }
    pos = 0;
    errPos = -1;
    promptShown = !mcSoundPrompt;
    drawPlay();
    if (mcSoundPrompt) playPrompt(chain[chainLen - 1]);
    gameCharBuffer = 0;
    clearPaddleLatches();
}

// The row is fully resolved: pause on the full-colour row, then grow (and in
// Call Signs mode bank a completed call and start a fresh one).
static void roundDone() {
    drawPlay();
    delay(MC_ROUND_PAUSE_MS);
    if (mcCallsMode && chainLen == callLen) {
        callsDone++;
        drawCallComplete();
        delay(MC_CALL_PAUSE_MS);
        newCall();
    }
    startRound();
}

// One keyed character: judge it against the chain. Deliberately NO OK/ERR
// sounds — audible feedback between letters would wreck the keying flow of
// the chain (Willi's call); the boxes are the only feedback.
static void handleAnswer(char c) {
    if (promptShown) promptShown = false;   // first answer clears the prompt
    if (c == chain[pos]) {
        pos++;
        if (pos == chainLen) roundDone();
        else                 drawPlay();
        return;
    }
    if (!mcCallsMode && errPos < 0) {       // the one tolerated error per round
        errPos = pos;
        totalErrs++;
        pos++;
        if (pos == chainLen) roundDone();
        else                 drawPlay();
        return;
    }
    deathPos = pos;                         // second error this round (Characters)
    mcState = MC_OVER;                      // or any error at all (Call Signs)
}

//=============================================================================
// States
//=============================================================================

static void stateLobby() {
    gameCharBuffer = 0;
    clearPaddleLatches();
    if (mcCallsMode && mcField == MC_FLD_KOCH) mcField = MC_FLD_MODE;
    drawLobby();

    while (mcState == MC_LOBBY) {
        if (checkPaddles()) {                  // start is paddle/key only,
            MorseOutput::resetTOT();           // matching the unified games
            while (checkPaddles()) delay(5);   // start gesture
            clearPaddleLatches();
            mcState = MC_PLAYING;
            return;
        }

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            switch (mcField) {
                case MC_FLD_MODE:
                    mcCallsMode = !mcCallsMode;
                    saveSettings();
                    break;
                case MC_FLD_PROMPT:
                    mcSoundPrompt = !mcSoundPrompt;
                    saveSettings();
                    break;
                case MC_FLD_KOCH:
                    MorsePreferences::kochFilter =
                        constrain((int)MorsePreferences::kochFilter + enc,
                                  (int)MorsePreferences::kochMinimum,
                                  (int)MorsePreferences::kochMaximum);
                    break;
            }
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawLobby();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { mcState = MC_EXIT; return; }
        if (Buttons::modeButton.clicks == 1) {          // black click: high scores
            drawHiscores(mcCallsMode ? 1 : 0, -1);
            for (;;) {                                  // any input returns
                if (checkPaddles()) { MorseOutput::resetTOT(); while (checkPaddles()) delay(5); clearPaddleLatches(); break; }
                Buttons::modeButton.Update();
                if (Buttons::modeButton.clicks != 0) { MorseOutput::resetTOT(); break; }
                Buttons::volButton.Update();
                if (Buttons::volButton.clicks != 0) { MorseOutput::resetTOT(); break; }
                serialEvent();
                checkShutDown(false);
                delay(10);
            }
            Buttons::modeButton.clicks = 0;             // don't leak the dismissing
            Buttons::volButton.clicks  = 0;             // press back into the lobby
            drawLobby();
        }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // FN: next lobby field
            if (mcField == MC_FLD_MODE)        mcField = MC_FLD_PROMPT;
            else if (mcField == MC_FLD_PROMPT) mcField = mcCallsMode ? MC_FLD_MODE : MC_FLD_KOCH;
            else                               mcField = MC_FLD_MODE;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawLobby();
        }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

static void statePlaying() {
    chainLen = 0;
    pos = 0;
    errPos = -1;
    totalErrs = 0;
    callsDone = 0;
    deathPos = -1;
    if (mcCallsMode) newCall();
    else             buildPool();
    startRound();

    while (mcState == MC_PLAYING) {
        bool busy = false;
        char c = pollKeyedChar(busy);
        if (c != 0) {
            handleAnswer(c);
            if (mcState != MC_PLAYING) return;
        }

        // Very tight loop while the player is actively keying — skip the idle
        // delay and idle-only housekeeping so the keyer's own element/gap
        // timers aren't skewed (devdocs/cw-timing-audit/FINDINGS.md).
        if (busy) continue;

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            if (mcEnc == MC_ENC_SPEED) changeSpeedValue(enc); else changeVolumeValue(enc);
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { mcState = MC_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // toggle encoder target
            mcEnc = (mcEnc == MC_ENC_SPEED) ? MC_ENC_VOLUME : MC_ENC_SPEED;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

static void drainPaddles() {
    while (checkPaddles()) delay(5);
    clearPaddleLatches();
}

static void stateOver() {
    int m = mcCallsMode ? 1 : 0;
    ChainScore s{};
    if (mcCallsMode) {
        s.primary   = (uint16_t)callsDone;
        s.secondary = (uint16_t)(chainLen > 0 ? chainLen - 1 : 0);
    } else {
        bool win = (deathPos < 0);
        s.primary   = (uint16_t)(win ? chainLen : chainLen - 1);
        s.secondary = (uint16_t)(win ? 0 : pos);
        s.errs      = (uint16_t)totalErrs;
        s.koch      = (uint8_t)MorsePreferences::kochFilter;
    }
    s.prompt = mcSoundPrompt ? 1 : 0;
    int rank = recordScore(m, s);

    MorseOutput::resetTOT();
    clearPaddleLatches();

    // Phase 1: game-over reveal — paddle/click advances to the table, long-press exits.
    drawGameOver(s, rank);
    for (;;) {
        if (checkPaddles()) { MorseOutput::resetTOT(); drainPaddles(); break; }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  break;                 // -> table
        if (Buttons::modeButton.clicks == -1) { mcState = MC_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }

    // Phase 2: high-score table — any press returns to the lobby (a new round
    // must start with the paddle gesture there), long-press exits.
    MorseOutput::resetTOT();
    clearPaddleLatches();
    drawHiscores(m, rank);
    for (;;) {
        if (checkPaddles()) { MorseOutput::resetTOT(); drainPaddles(); mcState = MC_LOBBY; return; }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  { mcState = MC_LOBBY; return; }
        if (Buttons::modeButton.clicks == -1) { mcState = MC_EXIT;  return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// Entry point
//=============================================================================

void MorseMemoryChain::run() {
    canvas = MorseGameMode::enterLandscape(MorsePreferences::leftHanded, 8);
    if (!canvas) return;

    MorseOutput::resetTOT();
    clearPaddleLatches();
    gameMode = true;
    gameCharBuffer = 0;
    loadSettings();
    mcState = MC_LOBBY;
    mcEnc   = MC_ENC_SPEED;
    mcField = MC_FLD_MODE;

    while (mcState != MC_EXIT) {
        switch (mcState) {
            case MC_LOBBY:   stateLobby();   break;
            case MC_PLAYING: statePlaying(); break;
            case MC_OVER:    stateOver();    break;
            default:         mcState = MC_EXIT; break;
        }
    }

    MorseCwEngine::playStop();                          // defensive — prompts are blocking
    gameMode = false;
    gameCharBuffer = 0;
    MorsePreferences::writePreferences("morserino");   // persist any speed/volume/Koch
                                                        // change — the value-cores don't
    MorseGameMode::exit();
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks  = 0;
}

#endif  // CONFIG_CW_GAME

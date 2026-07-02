/******************************************************************************************************************************
 *  Trailblazer — implementation (see header for scope).
 *****************************************************************************************************************************/

#include "MorseTrailblazer.h"
#include "MorseGameMode.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "morsedefs.h"
#include "ClickButton.h"
#include "GamePalette.h"
#include "MorseGame.h"          // gameMode, gameCharBuffer
#include "MorseGridEngine.h"
#include "MorseGridScore.h"
#include "MorseGridNet.h"

#include <LovyanGFX.hpp>

// ---- External references (same ad-hoc pattern every other game file uses) ----
extern int     checkEncoder();
extern void    checkShutDown(boolean);
extern void    serialEvent();
extern boolean checkPaddles();
extern void    clearPaddleLatches();
extern boolean doPaddleIambic(boolean, boolean);
extern boolean leftKey, rightKey;

//=============================================================================
// Layout constants — own copies, matching every other game file's convention
// (Morsel has its own MSL_SCREEN_W etc. rather than sharing a global).
//=============================================================================

#define TB_SCREEN_W   304
#define TB_SCREEN_H   170
#define TB_HUD_H       20

//=============================================================================
// Module state
//=============================================================================

enum TbState : uint8_t { TB_MODESEL, TB_READY, TB_PLAYING, TB_SOLVED, TB_MP_SOLVED, TB_EXIT };
enum TbEnc   : uint8_t { TB_ENC_SPEED, TB_ENC_VOLUME };

static LGFX_Sprite *canvas = nullptr;
static TbState tbState;
static TbEnc   tbEnc;
static bool    tbResumeMp;         // re-enter the MP lobby (role kept) after a ranking

static unsigned long tbStartMs;    // maze start time, for the CPM score
static int           tbWrong;      // wrong entries this maze

//=============================================================================
// Keyer input — checkPaddles() + doPaddleIambic() pump the keyer and (via the
// shared decoder) the current character; a completed character lands in
// gameCharBuffer instead of the scroll display because gameMode is true.
// Resets TOT on any keyer activity, not just completed characters, so a slow
// sender mid-letter can't time out (same safety margin as Morsel's
// noteActivity()).
//
// displayDecodedMorse() (m32_v6.ino) writes *every* decoded symbol into
// gameCharBuffer while gameMode is true, including the decoder's trailing
// inter-word space as its own separate ' ' event after a completed letter —
// the single-character-buffer form of the trailing-space gotcha (hard rule
// §3). Left unfiltered, that space arrives on a later tick and gets compared
// against nextChar() as a second "answer," which can never match a grid
// letter — silently doubling every result into <real answer>, ERR. Skip it
// here, the same way Morsel's own accumulator only accepts alnum characters.
//=============================================================================

// `busy` reports whether the keyer is actively processing input (doPaddleIambic
// returned true) — the caller uses it to keep a very tight loop while keying,
// mirroring the classic loop's "we are busy keying and so need a very tight
// loop" pattern (m32_v6.ino), so the keyer's own element/gap timers aren't
// skewed by the idle delay. See devdocs/cw-timing-audit/FINDINGS.md.
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
// Drawing
//=============================================================================

// Title + WPM/volume, bold on whichever the encoder currently owns — mirrors
// Morsel's mslEnc highlight convention (UX_CONVENTIONS.md §4).
static void drawHud() {
    canvas->fillRect(0, 0, TB_SCREEN_W, TB_HUD_H, PAL_BLACK);
    canvas->drawFastHLine(0, TB_HUD_H, TB_SCREEN_W, PAL_DARKGREY);

    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextDatum(lgfx::middle_left);
    canvas->setTextColor(PAL_CYAN, PAL_BLACK);
    canvas->drawString("TRAILBLAZER", 6, TB_HUD_H / 2 + 1);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::middle_right);
    char wbuf[16], vbuf[16];
    snprintf(wbuf, sizeof(wbuf), "%d wpm", MorsePreferences::wpm);
    snprintf(vbuf, sizeof(vbuf), "vol %d", MorsePreferences::sidetoneVolume);

    int wpmX = TB_SCREEN_W - 6;
    canvas->setTextColor(tbEnc == TB_ENC_SPEED ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(wbuf, wpmX, TB_HUD_H / 2 + 1);

    int volX = wpmX - canvas->textWidth(wbuf) - 16;
    canvas->setTextColor(tbEnc == TB_ENC_VOLUME ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(vbuf, volX, TB_HUD_H / 2 + 1);
}

static void drawReady() {
    canvas->fillSprite(PAL_BG);
    drawHud();
    MorseGameMode::drawCentred(canvas, TB_SCREEN_W / 2, 60, "TRAILBLAZER", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::drawCentred(canvas, TB_SCREEN_W / 2, 100, "Key to start", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);
    char buf[32];
    snprintf(buf, sizeof(buf), "Koch lesson %d", MorsePreferences::kochFilter);
    MorseGameMode::drawCentred(canvas, TB_SCREEN_W / 2, 130, buf, PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::drawCentred(canvas, TB_SCREEN_W / 2, 152, "click = high scores", PAL_GREY, PAL_BG, &fonts::Font0);
    MorseGameMode::pushFrame();
}

static void drawPlay() {
    canvas->fillSprite(PAL_BG);
    MorseGridEngine::drawBoard(canvas, TB_HUD_H + 2, TB_SCREEN_H - 2,
                               /*highlightNext=*/true, PAL_CYAN);
    drawHud();
    MorseGameMode::pushFrame();
}

static void drawSolved() {
    canvas->fillSprite(PAL_BG);
    drawHud();
    MorseGameMode::drawCentred(canvas, TB_SCREEN_W / 2, TB_HUD_H + (TB_SCREEN_H - TB_HUD_H) / 2,
                               "Solved!", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::pushFrame();
}

//=============================================================================
// States
//=============================================================================

// Reset the per-maze score counters (the maze itself is either generated
// locally, below, or already in the engine from a multiplayer START).
static void tbResetScore() {
    tbStartMs = millis();
    tbWrong   = 0;
}

// Generate a fresh maze and reset the per-maze score counters.
static void tbBeginMaze() {
    MorseGridEngine::generate();
    tbResetScore();
}

// Mode select + multiplayer lobby — the whole flow is shared with Fox Hunt
// (MorseGridNet); only the title and game id differ.
static void stateModeSel() {
    MorseGridNet::Flow f = MorseGridNet::enter(canvas, "TRAILBLAZER",
                                               MorseGridScore::TRAILBLAZER, tbResumeMp);
    tbResumeMp = false;
    switch (f) {
        case MorseGridNet::FLOW_SINGLE:                   // normal ready screen
            tbState = TB_READY;
            break;
        case MorseGridNet::FLOW_MP_PLAY:                  // race maze is already
            gameCharBuffer = 0;                           // in the engine
            tbResetScore();
            tbState = TB_PLAYING;
            break;
        default:
            tbState = TB_EXIT;
            break;
    }
}

static void stateReady() {
    drawReady();
    clearPaddleLatches();
    while (tbState == TB_READY) {
        if (checkPaddles()) {                  // start is paddle/key only,
            MorseOutput::resetTOT();           // matching the unified games
            while (checkPaddles()) delay(5);   // start gesture (Morsel's lobby)
            clearPaddleLatches();
            tbBeginMaze();
            tbState = TB_PLAYING;
            return;
        }

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            if (tbEnc == TB_ENC_SPEED) changeSpeedValue(enc); else changeVolumeValue(enc);
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawReady();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { tbState = TB_EXIT; return; }
        if (Buttons::modeButton.clicks == 1) {          // black click: high scores
            MorseGridScore::viewHiscores(canvas, MorseGridScore::TRAILBLAZER);
            drawReady();
        }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // toggle encoder target
            tbEnc = (tbEnc == TB_ENC_SPEED) ? TB_ENC_VOLUME : TB_ENC_SPEED;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawReady();
        }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

static void statePlaying() {
    drawPlay();
    while (tbState == TB_PLAYING) {
        bool busy = false;
        char c = pollKeyedChar(busy);
        if (c != 0) {
            if (c == MorseGridEngine::nextChar()) {
                MorseOutput::soundSignalOK();
                MorseGridEngine::advance();
                if (MorseGridEngine::atEnd()) {
                    tbState = MorseGridNet::isMpGame() ? TB_MP_SOLVED : TB_SOLVED;
                    return;
                }
                drawPlay();
            } else {
                MorseOutput::soundSignalERR();     // audio only — CONCEPT.md §2.4
                tbWrong++;                          // costs the CPM score (§5)
            }                                       // (no visual flash — not in spec)
        }

        // Very tight loop while the player is actively keying — skip the idle
        // delay and idle-only housekeeping so the keyer's own element/gap
        // timers aren't skewed. Mirror of the classic loop's "we are busy
        // keying and so need a very tight loop" (m32_v6.ino). See
        // devdocs/cw-timing-audit/FINDINGS.md.
        if (busy) continue;

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            if (tbEnc == TB_ENC_SPEED) changeSpeedValue(enc); else changeVolumeValue(enc);
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { tbState = TB_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // toggle encoder target
            tbEnc = (tbEnc == TB_ENC_SPEED) ? TB_ENC_VOLUME : TB_ENC_SPEED;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

static void stateSolved() {
    unsigned long elapsed = millis() - tbStartMs;
    int steps = MorseGridEngine::pathLength() - 1;      // moves = correct entries

    drawSolved();
    delay(800);                                         // brief acknowledgment

    MorseGridScore::GridScore sc;
    int rank = MorseGridScore::record(MorseGridScore::TRAILBLAZER, elapsed, steps,
                                      tbWrong, MorsePreferences::kochFilter, sc);
    MorseGridScore::Action act =
        MorseGridScore::showResultsAndHiscores(canvas, "TRAILBLAZER",
                                               MorseGridScore::TRAILBLAZER, sc, rank);
    if (act == MorseGridScore::PLAY_AGAIN) {
        tbBeginMaze();
        tbState = TB_PLAYING;
    } else {
        tbState = TB_EXIT;
    }
}

// Multiplayer end-of-maze: no NVS high score (MP results are transient, like
// Morsel's) — report the result and show the shared ranking screen instead.
static void stateMpSolved() {
    unsigned long elapsed = millis() - tbStartMs;
    int steps = MorseGridEngine::pathLength() - 1;      // moves = correct entries

    drawSolved();
    delay(800);                                         // brief acknowledgment

    MorseGridNet::After a = MorseGridNet::results(canvas, elapsed, tbWrong, steps);
    if (a == MorseGridNet::AGAIN) {
        tbResumeMp = true;                              // back to the MP lobby
        tbState = TB_MODESEL;
    } else {
        tbState = TB_EXIT;
    }
}

//=============================================================================
// Entry point
//=============================================================================

void MorseTrailblazer::run() {
    canvas = MorseGameMode::enterLandscape(MorsePreferences::leftHanded, 8);
    if (!canvas) return;

    MorseOutput::resetTOT();
    clearPaddleLatches();
    gameMode = true;
    gameCharBuffer = 0;
    tbState = TB_MODESEL;
    tbEnc = TB_ENC_SPEED;
    tbResumeMp = false;

    while (tbState != TB_EXIT) {
        switch (tbState) {
            case TB_MODESEL:   stateModeSel();  break;
            case TB_READY:     stateReady();    break;
            case TB_PLAYING:   statePlaying();  break;
            case TB_SOLVED:    stateSolved();   break;
            case TB_MP_SOLVED: stateMpSolved(); break;
            default:           tbState = TB_EXIT; break;
        }
    }

    MorseGridNet::teardown();   // stop packet routing + ESP-NOW, restore Koch
    gameMode = false;
    gameCharBuffer = 0;
    MorsePreferences::writePreferences("morserino");   // persist any speed/volume
                                                        // change — changeSpeedValue/
                                                        // changeVolumeValue don't
    MorseGameMode::exit();
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks = 0;
}

#endif  // CONFIG_CW_GAME

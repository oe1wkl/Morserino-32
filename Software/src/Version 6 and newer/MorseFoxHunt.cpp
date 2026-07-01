/******************************************************************************************************************************
 *  Fox Hunt — implementation (see header for scope).
 *****************************************************************************************************************************/

#include "MorseFoxHunt.h"
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
#include "MorseCwEngine.h"

#include <LovyanGFX.hpp>

// ---- External references (same ad-hoc pattern every other game file uses) ----
extern int         checkEncoder();
extern void        checkShutDown(boolean);
extern void        serialEvent();
extern boolean     checkPaddles();
extern void        clearPaddleLatches();
extern boolean     doPaddleIambic(boolean, boolean);
extern boolean     leftKey, rightKey;
extern KEYERSTATES keyerState;

//=============================================================================
// Layout constants — own copies, matching every other game file's convention.
//=============================================================================

#define FH_SCREEN_W   304
#define FH_SCREEN_H   170
#define FH_HUD_H       20

#define FH_IDLE_REPLAY_MS  5500UL   // tunable — on-device feedback: 4s felt
                                     // too short, 7s felt too long
#define FH_OK_PAUSE_MS      300UL   // tunable — deliberate breath between the
                                     // OK tone and the next clue so they don't
                                     // run together

//=============================================================================
// Module state
//=============================================================================

enum FhState : uint8_t { FH_READY, FH_PLAYING, FH_SOLVED, FH_EXIT };
enum FhEnc   : uint8_t { FH_ENC_SPEED, FH_ENC_VOLUME };

static LGFX_Sprite *canvas = nullptr;
static FhState fhState;
static FhEnc   fhEnc;
static unsigned long fhNextReplayAt;

static unsigned long fhStartMs;    // maze start time, for the CPM score
static int           fhWrong;      // wrong entries this maze

//=============================================================================
// CW clue playback — MorseCwEngine is the shared non-blocking player for
// game modes. wpm=0 plays at the player's own live keyer speed (like Koch/
// Generator), not a fixed clue speed. Case is passed through unmodified —
// generateCWword (called internally) treats uppercase letters as prosign
// markers, and the grid can contain them (Koch S/A/N/K/E/B/+), so
// lowercasing here would corrupt a prosign into a plain letter.
//
// MorseCwEngine's own built-in mute (keyerState != IDLE_STATE) only PAUSES
// playback and resumes the same partial character once the player goes back
// to idle — the right behaviour for Pileup/Radio Cave's repeating-beacon
// style playback, but wrong here: once the player has started answering,
// resuming a stale partial clue afterward is confusing ("why did it make
// more noise, I already answered"). pollKeyedChar() below explicitly calls
// playStop() the instant it sees the keyer become active, cancelling the
// clue outright instead of letting the engine pause-and-resume it.
//=============================================================================

// Same half-tone shift already used to distinguish incoming CW from the
// player's own sidetone in Echo Trainer / Trx modes (m32_v6.ino's KEY_START
// case) — reused here so the played clue is distinguishable by ear from the
// player's own keying, the same model Willi pointed at.
static uint16_t cluePitchHz() {
    uint16_t pitch = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    if (MorsePreferences::pliste[posEchoToneShift].value != 0) {
        pitch = (MorsePreferences::pliste[posEchoToneShift].value == 1) ? pitch * 18 / 17 : pitch * 17 / 18;
    }
    return pitch;
}

static void playClue() {
    char buf[2] = { MorseGridEngine::nextChar(), '\0' };
    MorseCwEngine::PlayOpts opts = {
        cluePitchHz(),
        /*wpm*/           0,
        /*loop*/          false,
        /*resumeGapDits*/ 0,
        /*extraMute*/     nullptr,
    };
    MorseCwEngine::playStart(String(buf), opts);
    fhNextReplayAt = millis() + FH_IDLE_REPLAY_MS;
}

//=============================================================================
// Keyer input — identical to Trailblazer's pollKeyedChar(), including the
// filter for the decoder's trailing inter-word space: displayDecodedMorse()
// (m32_v6.ino) writes every decoded symbol into gameCharBuffer while
// gameMode is true, including that trailing space as its own event right
// after a completed letter. Left unfiltered it arrives on a later tick and
// gets treated as a second (always-wrong) answer. See the
// gamecharbuffer-trailing-space memory note — this fix must travel with any
// game that polls gameCharBuffer for one answer at a time.
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

    // Cancel (not pause-and-resume) any in-flight clue the instant the
    // keyer leaves idle — checked against the same keyerState the engine's
    // own built-in mute uses, so this fires at least as early as that mute
    // would have. See the comment above playClue() for why cancel, not resume.
    if (keyerState != IDLE_STATE && MorseCwEngine::isPlaying()) MorseCwEngine::playStop();

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

static void drawHud() {
    canvas->fillRect(0, 0, FH_SCREEN_W, FH_HUD_H, PAL_BLACK);
    canvas->drawFastHLine(0, FH_HUD_H, FH_SCREEN_W, PAL_DARKGREY);

    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextDatum(lgfx::middle_left);
    canvas->setTextColor(PAL_CYAN, PAL_BLACK);
    canvas->drawString("FOX HUNT", 6, FH_HUD_H / 2 + 1);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::middle_right);
    char wbuf[16], vbuf[16];
    snprintf(wbuf, sizeof(wbuf), "%d wpm", MorsePreferences::wpm);
    snprintf(vbuf, sizeof(vbuf), "vol %d", MorsePreferences::sidetoneVolume);

    int wpmX = FH_SCREEN_W - 6;
    canvas->setTextColor(fhEnc == FH_ENC_SPEED ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(wbuf, wpmX, FH_HUD_H / 2 + 1);

    int volX = wpmX - canvas->textWidth(wbuf) - 16;
    canvas->setTextColor(fhEnc == FH_ENC_VOLUME ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BLACK);
    canvas->drawString(vbuf, volX, FH_HUD_H / 2 + 1);
}

static void drawReady() {
    canvas->fillSprite(PAL_BG);
    drawHud();
    MorseGameMode::drawCentred(canvas, FH_SCREEN_W / 2, 60, "FOX HUNT", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::drawCentred(canvas, FH_SCREEN_W / 2, 100, "Key to start", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);
    char buf[32];
    snprintf(buf, sizeof(buf), "Koch lesson %d", MorsePreferences::kochFilter);
    MorseGameMode::drawCentred(canvas, FH_SCREEN_W / 2, 130, buf, PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::drawCentred(canvas, FH_SCREEN_W / 2, 152, "click = high scores", PAL_GREY, PAL_BG, &fonts::Font0);
    MorseGameMode::pushFrame();
}

static void drawPlay() {
    canvas->fillSprite(PAL_BG);
    MorseGridEngine::drawBoard(canvas, FH_HUD_H + 2, FH_SCREEN_H - GRIDENG_LEGEND_H - 2,
                               /*highlightNext=*/false, PAL_CYAN);
    MorseGridEngine::drawLegend(canvas, FH_SCREEN_H - GRIDENG_LEGEND_H, GRIDENG_LEGEND_H);
    drawHud();
    MorseGameMode::pushFrame();
}

static void drawSolved() {
    canvas->fillSprite(PAL_BG);
    drawHud();
    MorseGameMode::drawCentred(canvas, FH_SCREEN_W / 2, FH_HUD_H + (FH_SCREEN_H - FH_HUD_H) / 2,
                               "Solved!", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::pushFrame();
}

//=============================================================================
// States
//=============================================================================

// Generate a fresh maze, play its first clue, and reset the per-maze score.
static void fhBeginMaze() {
    MorseGridEngine::generate();
    fhStartMs = millis();
    fhWrong   = 0;
    playClue();
}

static void stateReady() {
    drawReady();
    clearPaddleLatches();
    while (fhState == FH_READY) {
        if (checkPaddles()) {                  // start is paddle/key only,
            MorseOutput::resetTOT();           // matching the unified games
            while (checkPaddles()) delay(5);   // start gesture (Morsel's lobby)
            clearPaddleLatches();
            fhBeginMaze();
            fhState = FH_PLAYING;
            return;
        }

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            if (fhEnc == FH_ENC_SPEED) changeSpeedValue(enc); else changeVolumeValue(enc);
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawReady();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { fhState = FH_EXIT; return; }
        if (Buttons::modeButton.clicks == 1) {          // black click: high scores
            MorseGridScore::viewHiscores(canvas, MorseGridScore::FOXHUNT);
            drawReady();
        }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // toggle encoder target
            fhEnc = (fhEnc == FH_ENC_SPEED) ? FH_ENC_VOLUME : FH_ENC_SPEED;
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
    while (fhState == FH_PLAYING) {
        MorseCwEngine::playTick();

        bool busy = false;
        char c = pollKeyedChar(busy);
        if (c != 0) {
            MorseGridEngine::DirInfo dirs[4];
            MorseGridEngine::directionLegend(dirs);
            int matched = -1;
            for (int d = 0; d < 4; d++) if (dirs[d].ltr == c) { matched = d; break; }

            if (matched >= 0 && MorseGridEngine::directionMatchesNext((MorseGridEngine::Dir)matched)) {
                MorseOutput::soundSignalOK();
                MorseGridEngine::advance();
                if (MorseGridEngine::atEnd()) {
                    MorseCwEngine::playStop();
                    fhState = FH_SOLVED;
                    return;
                }
                delay(FH_OK_PAUSE_MS);   // deliberate breath — otherwise the OK
                                          // tone and the next clue run together
                playClue();
                drawPlay();
            } else {
                MorseOutput::soundSignalERR();   // wrong direction, or not a
                fhWrong++;                        // legend letter at all — same
            }                                     // treatment; costs the CPM score
        }

        // Very tight loop while the player is actively keying: skip the idle
        // delay AND the idle-only housekeeping below, so the keyer's own
        // element/gap timers aren't skewed. Mirror of the classic loop's
        // "we are busy keying and so need a very tight loop" (m32_v6.ino).
        if (busy) continue;

        // Idle-input auto-replay (guarded off while keying by the continue above).
        if (!MorseCwEngine::isPlaying() && millis() >= fhNextReplayAt) playClue();

        int enc = checkEncoder();
        if (enc != 0) {
            MorseOutput::resetTOT();
            if (fhEnc == FH_ENC_SPEED) changeSpeedValue(enc); else changeVolumeValue(enc);
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) { fhState = FH_EXIT; return; }
        if (Buttons::modeButton.clicks == 1) {          // on-demand replay
            playClue();
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == 1) {           // toggle encoder target
            fhEnc = (fhEnc == FH_ENC_SPEED) ? FH_ENC_VOLUME : FH_ENC_SPEED;
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            drawPlay();
        }

        serialEvent();
        checkShutDown(false);

        // Only pause when fully idle. While a clue is still sounding, keep a
        // tight loop so MorseCwEngine's own playback scheduling isn't skewed
        // by the delay either (same reason as the keying tight-loop above).
        if (!MorseCwEngine::isPlaying())
            delay(10);
    }
}

static void stateSolved() {
    unsigned long elapsed = millis() - fhStartMs;
    int steps = MorseGridEngine::pathLength() - 1;      // moves = correct entries

    drawSolved();
    delay(800);                                         // brief acknowledgment

    MorseGridScore::GridScore sc;
    int rank = MorseGridScore::record(MorseGridScore::FOXHUNT, elapsed, steps,
                                      fhWrong, MorsePreferences::kochFilter, sc);
    MorseGridScore::Action act =
        MorseGridScore::showResultsAndHiscores(canvas, "FOX HUNT",
                                               MorseGridScore::FOXHUNT, sc, rank);
    if (act == MorseGridScore::PLAY_AGAIN) {
        fhBeginMaze();
        fhState = FH_PLAYING;
    } else {
        fhState = FH_EXIT;
    }
}

//=============================================================================
// Entry point
//=============================================================================

void MorseFoxHunt::run() {
    canvas = MorseGameMode::enterLandscape(MorsePreferences::leftHanded, 8);
    if (!canvas) return;

    MorseOutput::resetTOT();
    clearPaddleLatches();
    gameMode = true;
    gameCharBuffer = 0;
    fhState = FH_READY;
    fhEnc = FH_ENC_SPEED;

    while (fhState != FH_EXIT) {
        switch (fhState) {
            case FH_READY:   stateReady();   break;
            case FH_PLAYING: statePlaying(); break;
            case FH_SOLVED:  stateSolved();  break;
            default:         fhState = FH_EXIT; break;
        }
    }

    MorseCwEngine::playStop();
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

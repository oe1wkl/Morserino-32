/******************************************************************************************************************************
 *  MorseGridScore — implementation (see header for scope).
 *****************************************************************************************************************************/

#include "MorseGridScore.h"
#include "MorseGameMode.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "morsedefs.h"
#include "ClickButton.h"
#include "GamePalette.h"

#include <Preferences.h>
#include <LovyanGFX.hpp>

// ---- External references (same ad-hoc pattern every game file uses) ----
extern int     checkEncoder();
extern void    checkShutDown(boolean);
extern void    serialEvent();
extern boolean checkPaddles();
extern void    clearPaddleLatches();

//=============================================================================
// Tunables / NVS layout
//=============================================================================

#define GRIDSCORE_HI_N              7        // table size (fits the screen, like Morsel)
#define GRIDSCORE_HI_VER           1        // bump if GridScore layout changes
#define GRIDSCORE_WRONG_PENALTY_MS 5000UL   // adjusted-time penalty per wrong entry
#define GRIDSCORE_NS               "gridgame"

#define GS_W   304          // the fixed landscape sprite width (shared by all games)

//=============================================================================
// In-RAM tables, indexed by Game (TRAILBLAZER=0, FOXHUNT=1)
//=============================================================================

using MorseGridScore::GridScore;
using MorseGridScore::Game;
using MorseGridScore::Action;

static GridScore   tables[2][GRIDSCORE_HI_N];
static bool        loaded[2] = { false, false };
static const char* kData[2]  = { "tb",  "fh"  };
static const char* kVer[2]   = { "tbv", "fhv" };

static void clearTable(int g) {
    for (int i = 0; i < GRIDSCORE_HI_N; ++i)
        tables[g][i] = GridScore{};                    // all-zero, empty = cpm 0
}

static void ensureLoaded(int g) {
    if (loaded[g]) return;
    clearTable(g);
    Preferences p;
    p.begin(GRIDSCORE_NS, true);
    if (p.getUChar(kVer[g], 0) == GRIDSCORE_HI_VER)   // absent/old stamp -> stay empty
        p.getBytes(kData[g], tables[g], sizeof(tables[g]));
    p.end();
    loaded[g] = true;
}

static void saveTable(int g) {
    Preferences p;
    p.begin(GRIDSCORE_NS, false);
    p.putBytes(kData[g], tables[g], sizeof(tables[g]));
    p.putUChar(kVer[g], GRIDSCORE_HI_VER);
    p.end();
}

//=============================================================================
// Scoring
//=============================================================================

unsigned long MorseGridScore::adjustedMs(unsigned long elapsedMs, int wrong) {
    return elapsedMs + (unsigned long)wrong * GRIDSCORE_WRONG_PENALTY_MS;
}

uint16_t MorseGridScore::cpm(unsigned long elapsedMs, int steps, int wrong) {
    unsigned long adjMs = adjustedMs(elapsedMs, wrong);
    if (adjMs == 0) adjMs = 1;                                   // guard div-by-zero
    return (uint16_t)(((unsigned long)steps * 60000UL + adjMs / 2) / adjMs);
}

int MorseGridScore::record(Game g, unsigned long elapsedMs, int steps, int wrong,
                           int koch, GridScore &out) {
    ensureLoaded(g);

    GridScore s;
    s.elapsedMs = elapsedMs;
    s.cpm       = cpm(elapsedMs, steps, wrong);
    s.wrong     = (uint16_t)wrong;
    s.steps     = (uint8_t)steps;
    s.koch      = (uint8_t)koch;
    out = s;

    int rank = -1;
    for (int i = 0; i < GRIDSCORE_HI_N; ++i) {
        if (s.cpm > tables[g][i].cpm) {                         // higher cpm ranks higher
            for (int j = GRIDSCORE_HI_N - 1; j > i; --j) tables[g][j] = tables[g][j - 1];
            tables[g][i] = s;
            rank = i;
            saveTable(g);
            break;
        }
    }
    return rank;
}

//=============================================================================
// Drawing
//=============================================================================

static void drawResults(LGFX_Sprite *canvas, const char *title,
                        const GridScore &sc, int rank) {
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, GS_W / 2, 6, title, PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    canvas->drawFastHLine(20, 32, GS_W - 40, PAL_MIDBLUE);

    MorseGameMode::drawCentred(canvas, GS_W / 2, 38, "Solved!", PAL_GREEN, PAL_BG, &fonts::FreeSansBold12pt7b);

    char buf[40];
    snprintf(buf, sizeof(buf), "%u CPM", sc.cpm);
    MorseGameMode::drawCentred(canvas, GS_W / 2, 66, buf, PAL_YELLOW, PAL_BG, &fonts::FreeSansBold18pt7b);

    unsigned long secs = (sc.elapsedMs + 500) / 1000;
    snprintf(buf, sizeof(buf), "Steps %u    Wrong %u", sc.steps, sc.wrong);
    MorseGameMode::drawCentred(canvas, GS_W / 2, 108, buf, PAL_WHITE, PAL_BG, &fonts::FreeSans9pt7b);
    snprintf(buf, sizeof(buf), "Time %lu:%02lu    Koch %u", secs / 60, secs % 60, sc.koch);
    MorseGameMode::drawCentred(canvas, GS_W / 2, 126, buf, PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);

    if (rank >= 0) {
        snprintf(buf, sizeof(buf), "NEW HIGH SCORE  -  rank %d", rank + 1);
        MorseGameMode::drawCentred(canvas, GS_W / 2, 144, buf, PAL_GREEN, PAL_BG, &fonts::FreeSans9pt7b);
    }
    MorseGameMode::drawCentred(canvas, GS_W / 2, 161, "Click: High Scores    Long press: Exit",
                               PAL_LIGHTGREY, PAL_BG, &fonts::Font0);
    MorseGameMode::pushFrame();
}

// Column geometry for the high-score table.
#define GS_X_RANK   36      // right-aligned
#define GS_X_CPM   132      // right-aligned
#define GS_X_STEPS 224      // right-aligned
#define GS_X_KOCH  248      // left-aligned

static void hsRow(LGFX_Sprite *canvas, int y, const char *rk, const char *cpm,
                  const char *st, const char *kc, uint16_t col) {
    canvas->setTextColor(col, PAL_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(rk,  GS_X_RANK,  y);
    canvas->drawString(cpm, GS_X_CPM,   y);
    canvas->drawString(st,  GS_X_STEPS, y);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(kc,  GS_X_KOCH,  y);
}

static void drawHiscores(LGFX_Sprite *canvas, int g, int lastRank) {
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, GS_W / 2, 2, "HIGH SCORES", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);

    canvas->setFont(&fonts::FreeSans9pt7b);
    hsRow(canvas, 28, "#", "CPM", "Steps", "Koch", PAL_LIGHTGREY);
    canvas->drawFastHLine(8, 45, GS_W - 16, PAL_MIDBLUE);

    bool any = false;
    int y = 49;
    for (int i = 0; i < GRIDSCORE_HI_N; ++i) {
        if (tables[g][i].cpm == 0) continue;                   // empty slot
        any = true;
        char rk[4], cpm[8], st[6], kc[6];
        snprintf(rk,  sizeof(rk),  "%d",  i + 1);
        snprintf(cpm, sizeof(cpm), "%u",  tables[g][i].cpm);
        snprintf(st,  sizeof(st),  "%u",  tables[g][i].steps);
        snprintf(kc,  sizeof(kc),  "K%u", tables[g][i].koch);
        hsRow(canvas, y, rk, cpm, st, kc, i == lastRank ? PAL_GREEN : PAL_WHITE);
        y += 16;
    }
    if (!any) {
        canvas->setTextDatum(lgfx::top_center);
        canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
        canvas->drawString("No scores yet", GS_W / 2, 80);
    }
    canvas->setFont(&fonts::Font0);
    canvas->setTextDatum(lgfx::top_center);
    canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
    canvas->drawString("press to continue", GS_W / 2, 161);
    canvas->setTextDatum(lgfx::top_left);
    MorseGameMode::pushFrame();
}

//=============================================================================
// End-of-game UI loop
//=============================================================================

static void drainPaddles() {
    while (checkPaddles()) delay(5);
    clearPaddleLatches();
}

Action MorseGridScore::showResultsAndHiscores(LGFX_Sprite *canvas, const char *title,
                                              Game g, const GridScore &sc, int rank) {
    MorseOutput::resetTOT();
    clearPaddleLatches();

    // Phase 1: results screen — paddle/click advances to the table, long-press exits.
    drawResults(canvas, title, sc, rank);
    for (;;) {
        if (checkPaddles()) { MorseOutput::resetTOT(); drainPaddles(); break; }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  break;                 // -> table
        if (Buttons::modeButton.clicks == -1) return MorseGridScore::EXIT;

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }

    // Phase 2: high-score table — any press returns to a fresh game, long-press exits.
    MorseOutput::resetTOT();
    clearPaddleLatches();
    drawHiscores(canvas, g, rank);
    for (;;) {
        if (checkPaddles()) { MorseOutput::resetTOT(); drainPaddles(); return MorseGridScore::PLAY_AGAIN; }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  return MorseGridScore::PLAY_AGAIN;
        if (Buttons::modeButton.clicks == -1) return MorseGridScore::EXIT;

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

void MorseGridScore::viewHiscores(LGFX_Sprite *canvas, Game g) {
    ensureLoaded(g);
    MorseOutput::resetTOT();
    clearPaddleLatches();
    drawHiscores(canvas, g, -1);                 // -1 = nothing highlighted

    // Clear both buttons' click state on the way out so the click/long-press
    // that dismisses this viewer can't leak back into the caller's loop as a
    // second action (CLAUDE.md §3.8 — a lingering clicks == -1 otherwise reads
    // as an immediate exit). The caller Update()s before checking too, so this
    // is belt-and-suspenders.
    auto done = []() { Buttons::modeButton.clicks = 0; Buttons::volButton.clicks = 0; };

    for (;;) {
        if (checkPaddles()) { MorseOutput::resetTOT(); drainPaddles(); done(); return; }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) { MorseOutput::resetTOT(); done(); return; }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) { MorseOutput::resetTOT(); done(); return; }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

#endif  // CONFIG_CW_GAME

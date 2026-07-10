/******************************************************************************************************************************
 *  MorseGridEngine — implementation (see header for scope and design notes).
 *****************************************************************************************************************************/

#include "MorseGridEngine.h"

#ifdef CONFIG_CW_GAME

#include "MorsePreferences.h"
#include "morsedefs.h"
#include "GamePalette.h"
#include "DejaVuSansMono_Bold11pt7b.h"

#include <LovyanGFX.hpp>

// Internal-only layout: every game on this hardware draws into the same
// fixed 304-wide landscape sprite (MorseGameMode), so this isn't a
// per-caller parameter — only the vertical band (top/bottom) is.
#define GRIDENG_SCREEN_W   304
#define GRIDENG_MARGIN      14
#define GRIDENG_CELLS      (GRIDENG_COLS * GRIDENG_ROWS)

//=============================================================================
// State
//=============================================================================

static char    grid[GRIDENG_CELLS];
static uint8_t pathCol[GRIDENG_CELLS], pathRow[GRIDENG_CELLS];
static int     pathLen;
static bool    visited[GRIDENG_CELLS];
static int     pos;

static char    pool[64];
static int     poolN;

//=============================================================================
// Path generation — randomised self-avoiding DFS, start col 0, end col
// GRIDENG_COLS-1. No obstacles, fully connected grid, so a single
// exhaustive backtracking pass always succeeds; the straight-line fallback
// below is defensive only.
//=============================================================================

static bool walk(int c, int r) {
    visited[r * GRIDENG_COLS + c] = true;
    pathCol[pathLen] = c; pathRow[pathLen] = r; pathLen++;
    if (c == GRIDENG_COLS - 1) return true;

    int dc[4] = { 1, -1, 0, 0 };
    int dr[4] = { 0, 0, 1, -1 };
    int order[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) {
        int j = random(i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    for (int k = 0; k < 4; k++) {
        int nc = c + dc[order[k]], nr = r + dr[order[k]];
        if (nc < 0 || nc >= GRIDENG_COLS || nr < 0 || nr >= GRIDENG_ROWS) continue;
        if (visited[nr * GRIDENG_COLS + nc]) continue;
        if (walk(nc, nr)) return true;
    }
    pathLen--;
    visited[r * GRIDENG_COLS + c] = false;
    return false;
}

static void genPath() {
    memset(visited, 0, sizeof(visited));
    pathLen = 0;
    int startRow = random(GRIDENG_ROWS);
    if (!walk(0, startRow)) {                     // defensive fallback only
        pathLen = 0;
        for (int c = 0; c < GRIDENG_COLS; c++) {
            pathCol[pathLen] = c; pathRow[pathLen] = startRow; pathLen++;
        }
    }
}

//=============================================================================
// Grid fill — every cell drawn from the current Koch lesson set, verbatim
// (prosign codes S/A/N/K/E/B/+ included once the lesson naturally reaches
// them — nothing forces one to appear; that was prototype-only test
// scaffolding for checking prosign-cell legibility, not real behavior).
//=============================================================================

static bool isProsign(char c) {
    return c == 'S' || c == 'A' || c == 'N' || c == 'K' || c == 'E' || c == 'B' || c == '+';
}

static void buildPool() {
    String set = koch.getCharSet();
    poolN = min((int)set.length(), (int)sizeof(pool) - 1);
    for (int i = 0; i < poolN; i++) pool[i] = set.charAt(i);
    if (poolN == 0) { pool[0] = 'M'; poolN = 1; }
}

static void fillGrid() {
    for (int i = 0; i < GRIDENG_CELLS; i++)
        grid[i] = pool[random(poolN)];
}

// Fox Hunt plays the next path cell's character and expects the player to
// key the direction toward it — but if another neighbour of the current
// cell shares that same character, a correctly decoded character still
// leaves the player unable to tell which direction is right. This is common
// at low Koch lessons (small pool vs up to 4 neighbours), not a rare edge
// case — confirmed on real hardware while prototyping this engine. Runs a
// few passes over all path cells since a fix at one cell can occasionally
// reintroduce a collision at a neighbour-sharing cell visited earlier in
// the same pass.
static void fixNeighbourCollisions() {
    for (int pass = 0; pass < 3; pass++) {
        for (int p = 0; p < pathLen; p++) {
            int c = pathCol[p], r = pathRow[p];
            int ni[4], n = 0;
            if (r > 0)                ni[n++] = (r - 1) * GRIDENG_COLS + c;
            if (r < GRIDENG_ROWS - 1) ni[n++] = (r + 1) * GRIDENG_COLS + c;
            if (c > 0)                ni[n++] = r * GRIDENG_COLS + (c - 1);
            if (c < GRIDENG_COLS - 1) ni[n++] = r * GRIDENG_COLS + (c + 1);

            for (int i = 1; i < n; i++) {
                for (int j = 0; j < i; j++) {
                    if (grid[ni[i]] != grid[ni[j]]) continue;
                    // Prefer to keep a prosign in place and reroll the plainer twin.
                    int victim = isProsign(grid[ni[i]]) ? j : i;
                    for (int tries = 0; tries < 40; tries++) {
                        char cand = pool[random(poolN)];
                        bool clash = false;
                        for (int k = 0; k < n; k++)
                            if (k != victim && grid[ni[k]] == cand) { clash = true; break; }
                        if (!clash) { grid[ni[victim]] = cand; break; }
                    }
                }
            }
        }
    }
}

void MorseGridEngine::generate() {
    buildPool();
    genPath();
    fillGrid();
    fixNeighbourCollisions();
    pos = 0;
}

//=============================================================================
// Multiplayer maze transfer — layout: [0]=pathLen, [1..CELLS]=grid chars,
// then pathLen path cells as row*COLS+col. See header for why the maze
// travels verbatim rather than a seed.
//=============================================================================

int MorseGridEngine::exportState(uint8_t *buf, int maxLen) {
    int need = 1 + GRIDENG_CELLS + pathLen;
    if (maxLen < need) return 0;
    buf[0] = (uint8_t)pathLen;
    memcpy(buf + 1, grid, GRIDENG_CELLS);
    for (int i = 0; i < pathLen; i++)
        buf[1 + GRIDENG_CELLS + i] = (uint8_t)(pathRow[i] * GRIDENG_COLS + pathCol[i]);
    return need;
}

bool MorseGridEngine::importState(const uint8_t *buf, int len) {
    if (len < 1) return false;
    int pl = buf[0];
    if (pl < 2 || pl > GRIDENG_CELLS) return false;
    if (len < 1 + GRIDENG_CELLS + pl) return false;

    // Validate the path before touching state: cells in range, start on the
    // left edge, end on the right edge, consecutive cells orthogonally
    // adjacent (a corrupt packet must not become a walkable non-maze).
    uint8_t pc[GRIDENG_CELLS], pr[GRIDENG_CELLS];
    for (int i = 0; i < pl; i++) {
        uint8_t cell = buf[1 + GRIDENG_CELLS + i];
        if (cell >= GRIDENG_CELLS) return false;
        pc[i] = cell % GRIDENG_COLS;
        pr[i] = cell / GRIDENG_COLS;
        if (i > 0) {
            int dc = (int)pc[i] - pc[i - 1], dr = (int)pr[i] - pr[i - 1];
            if (abs(dc) + abs(dr) != 1) return false;
        }
    }
    if (pc[0] != 0 || pc[pl - 1] != GRIDENG_COLS - 1) return false;

    memcpy(grid, buf + 1, GRIDENG_CELLS);
    memcpy(pathCol, pc, pl);
    memcpy(pathRow, pr, pl);
    pathLen = pl;
    pos = 0;
    return true;
}

//=============================================================================
// Queries
//=============================================================================

int MorseGridEngine::pathLength()   { return pathLen; }
int MorseGridEngine::currentIndex() { return pos; }
bool MorseGridEngine::atEnd()       { return pos >= pathLen - 1; }

void MorseGridEngine::setCurrentIndex(int idx) {
    pos = constrain(idx, 0, pathLen - 1);
}

char MorseGridEngine::nextChar() {
    int idx = pos + 1;
    return grid[pathRow[idx] * GRIDENG_COLS + pathCol[idx]];
}

void MorseGridEngine::directionLegend(DirInfo out[4]) {
    String set = koch.getCharSet();
    char letters[52]; int n = 0;
    for (int i = 0; i < (int)set.length() && n < 51; i++) {
        char c = set.charAt(i);
        if (c >= 'a' && c <= 'z') letters[n++] = c;
    }
    static const char canon[4] = { 'n', 's', 'w', 'e' };   // indexed by Dir
    bool used[26] = { false };
    for (int d = 0; d < 4; d++) out[d].ltr = 0;

    for (int d = 0; d < 4; d++) {                          // pass 1: canonical if learned
        for (int i = 0; i < n; i++) {
            if (letters[i] == canon[d]) {
                out[d].ltr = canon[d]; out[d].substituted = false;
                used[canon[d] - 'a'] = true;
                break;
            }
        }
    }
    for (int d = 0; d < 4; d++) {                          // pass 2: substitute
        if (out[d].ltr) continue;
        out[d].canonical = canon[d];
        out[d].substituted = true;
        out[d].ltr = '?';
        for (int i = 0; i < n; i++) {
            if (!used[letters[i] - 'a']) { out[d].ltr = letters[i]; used[letters[i] - 'a'] = true; break; }
        }
    }
}

bool MorseGridEngine::directionMatchesNext(Dir d) {
    if (atEnd()) return false;
    int c = pathCol[pos], r = pathRow[pos];
    switch (d) {
        case DIR_N: r -= 1; break;
        case DIR_S: r += 1; break;
        case DIR_W: c -= 1; break;
        case DIR_E: c += 1; break;
    }
    int nidx = pos + 1;
    return c == pathCol[nidx] && r == pathRow[nidx];
}

void MorseGridEngine::advance() {
    if (pos < pathLen - 1) pos++;
}

//=============================================================================
// Display string for a grid cell — mirrors MorseGame.cpp's getDisplayStr(): a
// prosign renders as its conventional 2-letter abbreviation (with an overbar
// drawn separately in drawBoard()), respecting the posOutputCase preference
// like every other mode. Copied locally because the original is private to
// MorseGame.cpp.
//=============================================================================

static void displayString(char c, char *buf) {
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
            if (upper) buf[0] = (c >= 'a' && c <= 'z') ? c - 32 : c;
            else       buf[0] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            buf[1] = '\0';
    }
}

//=============================================================================
// Drawing — neither function clears the sprite first; see header contract.
//=============================================================================

void MorseGridEngine::drawBoard(LGFX_Sprite *canvas, int top, int bottom,
                               bool highlightNext, uint8_t trailColor) {
    float cw = (float)(GRIDENG_SCREEN_W - 2 * GRIDENG_MARGIN) / GRIDENG_COLS;
    float ch = (float)(bottom - top) / GRIDENG_ROWS;

    auto cx = [&](int c) -> int { return GRIDENG_MARGIN + (int)((c + 0.5f) * cw); };
    auto cy = [&](int r) -> int { return top + (int)((r + 0.5f) * ch); };

    canvas->fillRect(0, top, GRIDENG_SCREEN_W, bottom - top, PAL_BG);

    // Trail: only the WALKED segment (behind the player) is drawn, in
    // trailColor. The path ahead is never previewed — that's the design for
    // both games (concept doc §2.3, "revealed only as you walk it"), and it's
    // essential for Fox Hunt: a dim preview band let a test user follow the
    // maze by eye without decoding the audio at all, defeating the game.
    //
    // Deliberately NOT drawWideLine/drawSmoothLine — see header comment on
    // drawBoard(). Path segments are always axis-aligned (4-neighbourhood,
    // no diagonals), so a solid fillRect per segment is both crash-safe and
    // the more direct fit.
    float trailW = min(cw, ch) * 0.28f;
    if (trailW < 3.0f) trailW = 3.0f;
    int halfW = (int)(trailW / 2);
    auto drawSeg = [&](int i, uint8_t color) {
        int x1 = cx(pathCol[i]),     y1 = cy(pathRow[i]);
        int x2 = cx(pathCol[i + 1]), y2 = cy(pathRow[i + 1]);
        if (y1 == y2) {
            int xa = min(x1, x2), xb = max(x1, x2);
            canvas->fillRect(xa - halfW, y1 - halfW, (xb - xa) + trailW, trailW, color);
        } else {
            int ya = min(y1, y2), yb = max(y1, y2);
            canvas->fillRect(x1 - halfW, ya - halfW, trailW, (yb - ya) + trailW, color);
        }
    };
    if (pos > 0)
        for (int i = 0; i < pos; i++) drawSeg(i, trailColor);

    int nextIdx = (pos + 1 < pathLen) ? pos + 1 : -1;

    // Trailblazer: highlight the next path cell as the key-this-letter target.
    if (highlightNext && nextIdx >= 0) {
        int bx = cx(pathCol[nextIdx]), by = cy(pathRow[nextIdx]);
        canvas->fillRoundRect(bx - cw * 0.44f, by - ch * 0.42f, cw * 0.88f, ch * 0.84f, 3, PAL_YELLOW);
    }

    // Grid letters (skip the current cell — drawn separately as the token below).
    canvas->setFont(&DejaVuSansMono_Bold11pt7b);
    canvas->setTextDatum(lgfx::middle_center);
    for (int r = 0; r < GRIDENG_ROWS; r++) {
        for (int c = 0; c < GRIDENG_COLS; c++) {
            bool isCur  = (c == pathCol[pos] && r == pathRow[pos]);
            bool isNext = (highlightNext && nextIdx >= 0 && c == pathCol[nextIdx] && r == pathRow[nextIdx]);
            if (isCur) continue;
            char buf[4];
            displayString(grid[r * GRIDENG_COLS + c], buf);
            bool prosign = (strlen(buf) > 1);
            uint8_t fg = isNext ? PAL_BLACK : PAL_WHITE;
            uint8_t bg = isNext ? PAL_YELLOW : PAL_BG;
            canvas->setTextColor(fg, bg);
            int X = cx(c), Y = cy(r);
            canvas->drawString(buf, X, Y);
            if (prosign) {
                int w = canvas->textWidth(buf);
                canvas->drawFastHLine(X - w / 2, Y - (int)(ch * 0.32f), w, fg);
            }
        }
    }

    // Current-position token.
    {
        int X = cx(pathCol[pos]), Y = cy(pathRow[pos]);
        canvas->fillCircle(X, Y, (int)(min(cw, ch) * 0.40f), PAL_LIGHTGREY);
        char buf[4];
        displayString(grid[pathRow[pos] * GRIDENG_COLS + pathCol[pos]], buf);
        canvas->setTextColor(PAL_BLACK, PAL_LIGHTGREY);
        canvas->drawString(buf, X, Y);
    }

    // Start / end edge markers.
    {
        int sy = cy(pathRow[0]);
        canvas->fillTriangle(GRIDENG_MARGIN - 11, sy - 4, GRIDENG_MARGIN - 11, sy + 4, GRIDENG_MARGIN - 3, sy, PAL_RED);
        if (pathCol[pathLen - 1] == GRIDENG_COLS - 1) {
            int ey = cy(pathRow[pathLen - 1]);
            int ex = GRIDENG_SCREEN_W - GRIDENG_MARGIN;
            canvas->fillTriangle(ex + 3, ey - 4, ex + 3, ey + 4, ex + 11, ey, PAL_RED);
        }
    }
}

void MorseGridEngine::drawLegend(LGFX_Sprite *canvas, int top, int height) {
    DirInfo dirs[4];
    directionLegend(dirs);
    const char arrow[4] = { '^', 'v', '<', '>' };   // ASCII-safe; the bundled
                                                      // bitmap fonts don't carry
                                                      // unicode arrow glyphs
    int by0 = top + 2, bh = height - 5;
    int cwid = 40, gap = 6;
    int total = 4 * cwid + 3 * gap;
    int x0 = (GRIDENG_SCREEN_W - total) / 2;
    canvas->setFont(&DejaVuSansMono_Bold11pt7b);
    canvas->setTextDatum(lgfx::middle_center);
    for (int d = 0; d < 4; d++) {
        int X = x0 + d * (cwid + gap);
        uint8_t border = dirs[d].substituted ? PAL_YELLOW : PAL_MIDBLUE;
        canvas->fillRoundRect(X, by0, cwid, bh, 3, PAL_DARKGREY);
        canvas->drawRoundRect(X, by0, cwid, bh, 3, border);
        char a[2] = { arrow[d], '\0' };
        canvas->setTextColor(PAL_WHITE, PAL_DARKGREY);
        canvas->drawString(a, X + 12, by0 + bh / 2 + 1);
        char l[2] = { (char)toupper(dirs[d].ltr), '\0' };
        canvas->setTextColor(dirs[d].substituted ? PAL_YELLOW : PAL_WHITE, PAL_DARKGREY);
        canvas->drawString(l, X + 27, by0 + bh / 2 + 1);
    }
}

#endif  // CONFIG_CW_GAME

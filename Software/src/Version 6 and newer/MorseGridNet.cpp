/******************************************************************************************************************************
 *  MorseGridNet — implementation (see header for scope).
 *
 *  The netcode mirrors MorseMorsel's multiplayer almost verbatim (pure
 *  ESP-NOW broadcast, no pairing; receive ring filled in callback context and
 *  drained in the game loop; server roster keyed by sender MAC with a TTL;
 *  clients resend their result until it shows up in the broadcast ranking).
 *  What differs from Morsel: the START packet carries the serialised maze
 *  instead of a word list, and the ranking metric is effective CPM
 *  (MorseGridScore::cpm) instead of adjusted time.
 *****************************************************************************************************************************/

#include "MorseGridNet.h"
#include "MorseGameMode.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "morsedefs.h"
#include "ClickButton.h"
#include "GamePalette.h"
#include "MorseGridEngine.h"

#include <Preferences.h>
#include <WiFi.h>                 // WiFi.macAddress() for the identity fallback
#include <LovyanGFX.hpp>

// ---- External references (same ad-hoc pattern every other game file uses) ----
extern int     checkEncoder();
extern void    checkShutDown(boolean);
extern void    serialEvent();
extern void    clearPaddleLatches();
extern bool    EspNowIsActive;    // owned by MorseMenu; ESP-NOW is up when true

//=============================================================================
// Protocol (ESP-NOW broadcast — magic "GRD")
//=============================================================================
//
// Packet layout: [0..2] 'G''R''D'  [3] proto ver  [4] type  [5..] payload.
// Every payload starts with the game id (MorseGridScore::Game), so a
// Trailblazer session and a Fox Hunt session on the same channel don't mix.

#define GRD_PROTO_VER   1
#define GRD_PKT_BEACON  1     // server -> all: game, koch, ident
#define GRD_PKT_JOIN    2     // client -> server: game, ident
#define GRD_PKT_START   3     // server -> all: game, koch, serialised maze
#define GRD_PKT_RESULT  4     // client -> server: game, elapsed, wrong, ident
#define GRD_PKT_SCORES  5     // server -> all: game, ranked result table

#define GRD_IDENT_LEN   8
#define GRD_PKTMAX     200    // must fit START: 5 + 2 + GRIDENG_STATE_MAX = 104
#define GRD_RING        8
#define GRD_MAXPEERS   20
#define GRD_RANK_BCAST 12     // max ranked entries broadcast (packet-size bound)
#define GRD_RANK_SHOW   6     // max ranked entries rendered on screen
#define GRD_SCORES_MS 1000UL  // server rebroadcast interval for the ranking
#define GRD_PEER_TTL  8000UL  // drop a peer not heard from in this long
#define GRD_BEACON_MS  700UL  // server beacon interval
#define GRD_JOIN_MS   1000UL  // client join/keepalive + result-resend interval

#define GRD_SCREEN_W  304     // the fixed landscape sprite width (all games)

// A multiplayer server session starts at the full-alphabet lesson regardless
// of the operator's personal one (dial back with the encoder if wanted) —
// the same default Morsel's server lobby uses.
#define GRD_SERVER_KOCH_DEFAULT 41

namespace MorseGridNet { bool gridNetMode = false; }

//=============================================================================
// Module state
//=============================================================================

enum GrdRole : uint8_t { GRD_NONE, GRD_SERVER, GRD_CLIENT };
static GrdRole      role = GRD_NONE;
static uint8_t      curGame;                    // MorseGridScore::Game of this session
static bool         mpGame = false;             // between FLOW_MP_PLAY and the ranking
static char         myIdent[GRD_IDENT_LEN + 1];
static int          savedKoch = -1;             // player's real lesson; -1 = nothing saved
static uint8_t      menuSel = 0;                // shared two-item picker index

// Lock-free-ish receive ring: producer = ESP-NOW RX task, consumer = game loop.
struct GrdRx { uint8_t mac[6]; uint8_t len; uint8_t d[GRD_PKTMAX]; };
static volatile GrdRx   ring[GRD_RING];
static volatile uint8_t ringHead = 0, ringTail = 0;

// Server-side roster of joined clients (keyed by MAC) + their reported result.
struct GrdPeer { uint8_t mac[6]; char ident[GRD_IDENT_LEN + 1];
                 unsigned long lastHeard; bool used;
                 bool reported; uint32_t resElapsedMs; uint16_t resWrong; };
static GrdPeer      peers[GRD_MAXPEERS];

// Client-side view of the server's broadcast settings.
static bool          srvSeen = false;
static unsigned long srvLast = 0;
static uint8_t       srvKoch = 0;
static char          srvIdent[GRD_IDENT_LEN + 1] = "";

// Ranked result table. The server fills it from its own result + every
// reported peer and broadcasts it; clients store the last received broadcast.
// Both sides render from the same array. Raw elapsed/wrong travel (not a
// precomputed CPM): steps are identical on every device (same maze), so each
// side derives the CPM locally via the shared MorseGridScore::cpm().
struct GrdRank { uint32_t elapsedMs; uint16_t wrong; char ident[GRD_IDENT_LEN + 1]; };
static GrdRank       rank[GRD_RANK_BCAST];
static uint8_t       rankN = 0;
static bool          rankSeen = false;          // any ranking received/built yet?
static bool          rankDirty = false;         // server: rebroadcast pending
static unsigned long rankLastBcast = 0;         // server: last SCORES tx

// Own result while on the ranking screen (server ranks itself with these;
// client resends them until they appear in the broadcast table).
static uint32_t      myElapsedMs = 0;
static uint16_t      myWrong = 0;

void MorseGridNet::onRecv(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    uint8_t h = ringHead;
    uint8_t nxt = (h + 1) % GRD_RING;
    if (nxt == ringTail) return;               // ring full: drop (senders repeat)
    memcpy((void*)ring[h].mac, mac, 6);
    uint8_t n = len > GRD_PKTMAX ? GRD_PKTMAX : len;
    ring[h].len = n;
    memcpy((void*)ring[h].d, data, n);
    ringHead = nxt;
}

//=============================================================================
// Identity + net housekeeping (mirrors Morsel)
//=============================================================================

static void loadIdent() {
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
    strncpy(myIdent, id.c_str(), GRD_IDENT_LEN);
    myIdent[GRD_IDENT_LEN] = '\0';
}

static void netReset() {
    ringHead = ringTail = 0;
    for (int i = 0; i < GRD_MAXPEERS; ++i) peers[i].used = false;
    srvSeen = false;
    rankN = 0;
    rankSeen = false;
    rankDirty = false;
    rankLastBcast = 0;
}

static void grdSend(uint8_t type, const uint8_t* payload, uint8_t plen) {
    uint8_t buf[GRD_PKTMAX];
    buf[0] = 'G'; buf[1] = 'R'; buf[2] = 'D';
    buf[3] = GRD_PROTO_VER;
    buf[4] = type;
    uint8_t n = plen > (GRD_PKTMAX - 5) ? (GRD_PKTMAX - 5) : plen;
    if (n) memcpy(buf + 5, payload, n);
    quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, buf, 5 + n);
}

static void sendBeacon() {
    uint8_t pl[2 + GRD_IDENT_LEN + 1];
    pl[0] = curGame;
    pl[1] = MorsePreferences::kochFilter;
    memcpy(pl + 2, myIdent, GRD_IDENT_LEN + 1);
    grdSend(GRD_PKT_BEACON, pl, sizeof(pl));
}

static void sendJoin() {
    uint8_t pl[1 + GRD_IDENT_LEN + 1];
    pl[0] = curGame;
    memcpy(pl + 1, myIdent, GRD_IDENT_LEN + 1);
    grdSend(GRD_PKT_JOIN, pl, sizeof(pl));
}

// START carries the maze every device will race on: game, koch (for the
// clients' lobby/HUD display), then the engine's serialised grid + path.
static void sendStart() {
    uint8_t pl[2 + GRIDENG_STATE_MAX];
    pl[0] = curGame;
    pl[1] = MorsePreferences::kochFilter;
    int n = MorseGridEngine::exportState(pl + 2, GRIDENG_STATE_MAX);
    if (n <= 0) return;                        // can't happen: buffer is max-sized
    grdSend(GRD_PKT_START, pl, 2 + n);
}

// Client -> server final result: game, elapsedMs(4), wrong(2), ident.
static void sendResult() {
    uint8_t pl[1 + 4 + 2 + GRD_IDENT_LEN + 1];
    uint8_t n = 0;
    pl[n++] = curGame;
    pl[n++] = myElapsedMs & 0xFF;         pl[n++] = (myElapsedMs >> 8) & 0xFF;
    pl[n++] = (myElapsedMs >> 16) & 0xFF; pl[n++] = (myElapsedMs >> 24) & 0xFF;
    pl[n++] = myWrong & 0xFF;             pl[n++] = (myWrong >> 8) & 0xFF;
    memcpy(pl + n, myIdent, GRD_IDENT_LEN + 1);
    n += GRD_IDENT_LEN + 1;
    grdSend(GRD_PKT_RESULT, pl, n);
}

// Server: gather own result + all reported peers, sort, keep the best
// GRD_RANK_BCAST. The server competes as a regular player (Morsel's rule).
static void serverBuildRank() {
    GrdRank tmp[1 + GRD_MAXPEERS];
    int n = 0;
    tmp[n].elapsedMs = myElapsedMs;
    tmp[n].wrong     = myWrong;
    strncpy(tmp[n].ident, myIdent, GRD_IDENT_LEN);
    tmp[n].ident[GRD_IDENT_LEN] = '\0';
    ++n;
    for (int i = 0; i < GRD_MAXPEERS; ++i) {
        if (!peers[i].used || !peers[i].reported) continue;
        tmp[n].elapsedMs = peers[i].resElapsedMs;
        tmp[n].wrong     = peers[i].resWrong;
        strncpy(tmp[n].ident, peers[i].ident, GRD_IDENT_LEN);
        tmp[n].ident[GRD_IDENT_LEN] = '\0';
        ++n;
    }
    // Ascending adjusted time (MorseGridScore::adjustedMs) is the ranking
    // key: every device solved the same maze (equal steps), so it orders
    // exactly like descending CPM, minus CPM's rounding ties.
    for (int i = 1; i < n; ++i) {              // insertion sort, n <= 21
        GrdRank k = tmp[i]; int j = i - 1;
        while (j >= 0 && MorseGridScore::adjustedMs(tmp[j].elapsedMs, tmp[j].wrong) >
                         MorseGridScore::adjustedMs(k.elapsedMs, k.wrong)) { tmp[j + 1] = tmp[j]; --j; }
        tmp[j + 1] = k;
    }
    int keep = n < GRD_RANK_BCAST ? n : GRD_RANK_BCAST;
    for (int i = 0; i < keep; ++i) rank[i] = tmp[i];
    rankN = (uint8_t)keep;
    rankSeen = true;
}

// Server -> all: ranked table. Payload: game, count, then per entry
// elapsedMs(4) wrong(2) ident(9). 2 + 12*15 = 182 <= GRD_PKTMAX-5.
static void sendScores() {
    uint8_t pl[2 + GRD_RANK_BCAST * (4 + 2 + GRD_IDENT_LEN + 1)];
    uint8_t n = 0;
    pl[n++] = curGame;
    pl[n++] = rankN;
    for (int i = 0; i < rankN; ++i) {
        uint32_t e = rank[i].elapsedMs;
        pl[n++] = e & 0xFF;  pl[n++] = (e >> 8) & 0xFF;
        pl[n++] = (e >> 16) & 0xFF; pl[n++] = (e >> 24) & 0xFF;
        pl[n++] = rank[i].wrong & 0xFF;
        pl[n++] = (rank[i].wrong >> 8) & 0xFF;
        memcpy(pl + n, rank[i].ident, GRD_IDENT_LEN + 1);
        n += GRD_IDENT_LEN + 1;
    }
    grdSend(GRD_PKT_SCORES, pl, n);
}

static void rosterAdd(const uint8_t* mac, const char* ident) {
    for (int i = 0; i < GRD_MAXPEERS; ++i)
        if (peers[i].used && memcmp(peers[i].mac, mac, 6) == 0) {
            peers[i].lastHeard = millis();
            strncpy(peers[i].ident, ident, GRD_IDENT_LEN);
            peers[i].ident[GRD_IDENT_LEN] = '\0';
            return;
        }
    for (int i = 0; i < GRD_MAXPEERS; ++i)
        if (!peers[i].used) {
            peers[i].used = true;
            memcpy(peers[i].mac, mac, 6);
            strncpy(peers[i].ident, ident, GRD_IDENT_LEN);
            peers[i].ident[GRD_IDENT_LEN] = '\0';
            peers[i].lastHeard = millis();
            peers[i].reported = false;
            return;                            // (silently ignore > cap)
        }
}

static void rosterRecordResult(const uint8_t* mac, const char* ident,
                               uint32_t elapsedMs, uint16_t wrong) {
    rosterAdd(mac, ident);                     // ensure peer exists + refresh
    for (int i = 0; i < GRD_MAXPEERS; ++i)
        if (peers[i].used && memcmp(peers[i].mac, mac, 6) == 0) {
            peers[i].reported     = true;
            peers[i].resElapsedMs = elapsedMs;
            peers[i].resWrong     = wrong;
            rankDirty = true;
            return;
        }
}

static void rosterAge() {
    for (int i = 0; i < GRD_MAXPEERS; ++i)
        if (peers[i].used && millis() - peers[i].lastHeard > GRD_PEER_TTL)
            peers[i].used = false;
}

static int rosterCount() {
    int n = 0;
    for (int i = 0; i < GRD_MAXPEERS; ++i) if (peers[i].used) ++n;
    return n;
}

// Drain the receive ring, dropping anything that isn't our protocol/game.
// Returns true when a client imported a valid START maze (the race begins).
static bool netPump() {
    bool started = false;
    while (ringTail != ringHead) {
        uint8_t t = ringTail;
        uint8_t len = ring[t].len;
        uint8_t d[GRD_PKTMAX];
        uint8_t mac[6];
        memcpy(mac, (const void*)ring[t].mac, 6);
        memcpy(d,   (const void*)ring[t].d, len);
        ringTail = (t + 1) % GRD_RING;

        if (len < 6 || d[0] != 'G' || d[1] != 'R' || d[2] != 'D') continue;
        if (d[3] != GRD_PROTO_VER) continue;
        uint8_t type = d[4];
        const uint8_t* pl = d + 5;
        uint8_t pn = len - 5;
        if (pl[0] != curGame) continue;        // other grid game's session

        if (role == GRD_SERVER && type == GRD_PKT_JOIN && pn >= 2) {
            char id[GRD_IDENT_LEN + 1];
            strncpy(id, (const char*)(pl + 1), GRD_IDENT_LEN);
            id[GRD_IDENT_LEN] = '\0';
            rosterAdd(mac, id);
        } else if (role == GRD_CLIENT && type == GRD_PKT_BEACON && pn >= 2) {
            srvSeen = true;
            srvLast = millis();
            srvKoch = pl[1];
            if (pn >= 3) {
                strncpy(srvIdent, (const char*)(pl + 2), GRD_IDENT_LEN);
                srvIdent[GRD_IDENT_LEN] = '\0';
            }
        } else if (role == GRD_CLIENT && type == GRD_PKT_START && pn >= 3) {
            if (MorseGridEngine::importState(pl + 2, pn - 2)) {
                srvKoch = pl[1];
                // Adopt the server's lesson for the race (restored on leaving
                // the multiplayer flow): the maze itself travels in the packet,
                // but Fox Hunt's direction legend is computed against the
                // local koch.getCharSet() — left at a low local lesson, a
                // client would race with shorter substituted direction letters
                // than the server's canonical N/E/S/W.
                if (savedKoch < 0) savedKoch = MorsePreferences::kochFilter;
                MorsePreferences::kochFilter =
                    constrain((int)srvKoch, (int)MorsePreferences::kochMinimum,
                                            (int)MorsePreferences::kochMaximum);
                started = true;
            }                                  // corrupt maze: ignore, keep waiting
        } else if (role == GRD_SERVER && type == GRD_PKT_RESULT && pn >= 7) {
            uint32_t e = (uint32_t)pl[1] | ((uint32_t)pl[2] << 8) |
                         ((uint32_t)pl[3] << 16) | ((uint32_t)pl[4] << 24);
            uint16_t w = (uint16_t)pl[5] | ((uint16_t)pl[6] << 8);
            char id[GRD_IDENT_LEN + 1] = "";
            if (pn >= 8) {
                strncpy(id, (const char*)(pl + 7), GRD_IDENT_LEN);
                id[GRD_IDENT_LEN] = '\0';
            }
            rosterRecordResult(mac, id, e, w);
        } else if (role == GRD_CLIENT && type == GRD_PKT_SCORES && pn >= 2) {
            uint8_t cnt = pl[1];
            if (cnt > GRD_RANK_BCAST) cnt = GRD_RANK_BCAST;
            uint16_t off = 2;
            uint8_t k = 0;
            for (uint8_t i = 0; i < cnt; ++i) {
                if ((uint16_t)(off + 6 + GRD_IDENT_LEN + 1) > pn) break;
                rank[k].elapsedMs = (uint32_t)pl[off] | ((uint32_t)pl[off+1] << 8) |
                                    ((uint32_t)pl[off+2] << 16) | ((uint32_t)pl[off+3] << 24);
                off += 4;
                rank[k].wrong = (uint16_t)pl[off] | ((uint16_t)pl[off+1] << 8);
                off += 2;
                memcpy(rank[k].ident, pl + off, GRD_IDENT_LEN);
                rank[k].ident[GRD_IDENT_LEN] = '\0';
                off += GRD_IDENT_LEN + 1;
                ++k;
            }
            rankN = k;
            rankSeen = true;
        }
    }
    return started;
}

//=============================================================================
// Drawing
//=============================================================================

static void drawMenuList(LGFX_Sprite *canvas, const char *title,
                         const char *a, const char *b, uint8_t sel) {
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 16, title, PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 64, a,
                               sel == 0 ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans12pt7b);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 98, b,
                               sel == 1 ? PAL_YELLOW : PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans12pt7b);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 140, "turn=move  click=ok  hold=back",
                               PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::pushFrame();
}

static void drawServerLobby(LGFX_Sprite *canvas) {
    canvas->fillSprite(PAL_BG);
    char b[32];
    snprintf(b, sizeof(b), "Server  -  Koch %d", MorsePreferences::kochFilter);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 8, b, PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::top_left);
    int n = rosterCount();
    snprintf(b, sizeof(b), "Joined: %d", n);
    canvas->setTextColor(n ? PAL_GREEN : PAL_LIGHTGREY, PAL_BG);
    canvas->drawString(b, 10, 40);

    int shown = 0;                              // roster idents, two columns
    for (int i = 0; i < GRD_MAXPEERS && shown < 10; ++i) {
        if (!peers[i].used) continue;
        int col = shown % 2, row = shown / 2;
        canvas->setTextColor(PAL_WHITE, PAL_BG);
        canvas->drawString(peers[i].ident, 14 + col * 150, 62 + row * 18);
        ++shown;
    }
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 150, "click=START  turn=Koch  hold=back",
                               PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::pushFrame();
}

static void drawClientWait(LGFX_Sprite *canvas) {
    canvas->fillSprite(PAL_BG);
    char b[36];
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 12, "Join a game", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);
    if (!srvSeen || millis() - srvLast > GRD_PEER_TTL) {
        MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 64, "Searching for server...",
                                   PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans12pt7b);
    } else {
        snprintf(b, sizeof(b), "Server: %s", srvIdent);
        MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 54, b, PAL_GREEN, PAL_BG, &fonts::FreeSans12pt7b);
        snprintf(b, sizeof(b), "Koch %d", srvKoch);
        MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 90, b, PAL_WHITE, PAL_BG, &fonts::FreeSans9pt7b);
        MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 112, "waiting for server to start",
                                   PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    }
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 150, "hold = back",
                               PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::pushFrame();
}

static void drawRanking(LGFX_Sprite *canvas, int steps) {
    canvas->fillSprite(PAL_BG);
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 4, "RANKING", PAL_CYAN, PAL_BG, &fonts::FreeSansBold12pt7b);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::top_left);

    if (!rankSeen) {
        MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 78,
                                   role == GRD_SERVER ? "waiting for players..."
                                                      : "waiting for ranking...",
                                   PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    } else {
        int shown = rankN < GRD_RANK_SHOW ? rankN : GRD_RANK_SHOW;
        int y = 32;
        char b[48];
        for (int i = 0; i < shown; ++i) {
            bool isSelf = strncmp(rank[i].ident, myIdent, GRD_IDENT_LEN) == 0;
            unsigned long s = (rank[i].elapsedMs + 500) / 1000;
            uint16_t c = MorseGridScore::cpm(rank[i].elapsedMs, steps, rank[i].wrong);
            snprintf(b, sizeof(b), "%2d  %-8s  %3u CPM  %lu:%02lu  %uw",
                     i + 1, rank[i].ident, c, s / 60, s % 60, rank[i].wrong);
            canvas->setTextColor(isSelf ? PAL_YELLOW : PAL_WHITE, PAL_BG);
            canvas->drawString(b, 10, y);
            y += 18;
        }
        if (rankN > shown) {
            snprintf(b, sizeof(b), "+%d more", rankN - shown);
            canvas->setTextColor(PAL_LIGHTGREY, PAL_BG);
            canvas->drawString(b, 10, y);
        }
    }
    MorseGameMode::drawCentred(canvas, GRD_SCREEN_W / 2, 152, "Click: Lobby    Long press: Exit",
                               PAL_LIGHTGREY, PAL_BG, &fonts::FreeSans9pt7b);
    MorseGameMode::pushFrame();
}

//=============================================================================
// Koch lesson save/restore — the multiplayer session temporarily overrides
// the operator's lesson: a server starts at GRD_SERVER_KOCH_DEFAULT
// (encoder-adjustable), a client adopts the server's lesson when the race
// starts (see the START handler in netPump()). Restored whenever the flow
// leaves the role, and again in teardown() as a safety net (covers exiting
// mid-game via black-long).
//=============================================================================

static void kochOverrideBegin() {
    if (savedKoch < 0) savedKoch = MorsePreferences::kochFilter;
    MorsePreferences::kochFilter =
        constrain(GRD_SERVER_KOCH_DEFAULT, (int)MorsePreferences::kochMinimum,
                                           (int)MorsePreferences::kochMaximum);
}

static void kochRestore() {
    if (savedKoch >= 0) {
        MorsePreferences::kochFilter = savedKoch;
        savedKoch = -1;
    }
}

//=============================================================================
// Entry flow — mode select -> role pick -> server roster / client wait.
// One blocking function with an internal state machine; black-long steps up
// one level and exits at the top (UX_CONVENTIONS.md §12).
//=============================================================================

enum GrdScreen : uint8_t { SCR_MODESEL, SCR_ROLESEL, SCR_SERVER, SCR_CLIENT };

bool MorseGridNet::isMpGame() { return mpGame; }

MorseGridNet::Flow MorseGridNet::enter(LGFX_Sprite *canvas, const char *title,
                                       MorseGridScore::Game game, bool resume) {
    curGame = (uint8_t)game;
    mpGame  = false;
    clearPaddleLatches();

    GrdScreen scr;
    if (resume && role == GRD_SERVER)      scr = SCR_SERVER;
    else if (resume && role == GRD_CLIENT) { netReset(); scr = SCR_CLIENT; }
    else                                   { role = GRD_NONE; scr = SCR_MODESEL; }

    unsigned long lastBeacon = 0, lastJoin = 0, lastDraw = 0;
    bool dirty = true;
    menuSel = 0;

    for (;;) {
        switch (scr) {

        case SCR_MODESEL: {
            if (dirty) { drawMenuList(canvas, title, "Single player", "Multiplayer", menuSel);
                         dirty = false; }
            int e = checkEncoder();
            if (e != 0) { MorseOutput::resetTOT(); menuSel ^= 1; dirty = true;
                          MorseOutput::pwmClick(MorsePreferences::sidetoneVolume); }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == 1) {
                if (menuSel == 0) return FLOW_SINGLE;
                // Multiplayer: bring ESP-NOW up. Radio stays up until
                // teardown() at game exit (Morsel's model) — no mid-session
                // WiFi cycles.
                if (!EspNowIsActive) MorseMenu::setupESPNow();
                gridNetMode = true;
                loadIdent();
                netReset();
                role = GRD_NONE;
                menuSel = 0; dirty = true;
                scr = SCR_ROLESEL;
            }
            if (Buttons::modeButton.clicks == -1) return FLOW_EXIT;
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
            break;
        }

        case SCR_ROLESEL: {
            if (dirty) { drawMenuList(canvas, "Multiplayer", "Start as Server", "Join a game", menuSel);
                         dirty = false; }
            int e = checkEncoder();
            if (e != 0) { MorseOutput::resetTOT(); menuSel ^= 1; dirty = true;
                          MorseOutput::pwmClick(MorsePreferences::sidetoneVolume); }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == 1) {
                netReset();
                if (menuSel == 0) { role = GRD_SERVER; kochOverrideBegin(); scr = SCR_SERVER; }
                else              { role = GRD_CLIENT; scr = SCR_CLIENT; }
                lastBeacon = lastJoin = 0; dirty = true;
            }
            if (Buttons::modeButton.clicks == -1) { menuSel = 0; dirty = true; scr = SCR_MODESEL; }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
            break;
        }

        case SCR_SERVER: {
            netPump();                          // collect JOINs
            rosterAge();
            if (millis() - lastBeacon > GRD_BEACON_MS) {
                sendBeacon();
                lastBeacon = millis();
            }
            int e = checkEncoder();
            if (e != 0) {
                MorseOutput::resetTOT();
                int v = constrain((int)MorsePreferences::kochFilter + e,
                                  (int)MorsePreferences::kochMinimum,
                                  (int)MorsePreferences::kochMaximum);
                MorsePreferences::kochFilter = v;
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                dirty = true;
            }
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == 1) {          // START the race
                for (int i = 0; i < GRD_MAXPEERS; ++i)      // clear last game's results
                    peers[i].reported = false;
                MorseGridEngine::generate();                // under the server's Koch set
                sendStart();
                mpGame = true;
                clearPaddleLatches();
                return FLOW_MP_PLAY;
            }
            if (Buttons::modeButton.clicks == -1) {         // back to role pick
                kochRestore();
                role = GRD_NONE; menuSel = 0; dirty = true; scr = SCR_ROLESEL; break;
            }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
            if (dirty || millis() - lastDraw > 500) {
                drawServerLobby(canvas); dirty = false; lastDraw = millis();
            }
            break;
        }

        case SCR_CLIENT: {
            bool started = netPump();
            if (started) {                                  // server began the race
                mpGame = true;
                clearPaddleLatches();
                return FLOW_MP_PLAY;
            }
            if (millis() - lastJoin > GRD_JOIN_MS) {
                sendJoin();
                lastJoin = millis();
            }
            if (checkEncoder() != 0) MorseOutput::resetTOT();
            Buttons::modeButton.Update();
            if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
            if (Buttons::modeButton.clicks == -1) {         // back to role pick
                kochRestore();                              // drop an adopted lesson
                role = GRD_NONE; menuSel = 0; dirty = true; scr = SCR_ROLESEL; break;
            }
            Buttons::volButton.Update();
            if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
            if (dirty || millis() - lastDraw > 400) {
                drawClientWait(canvas); dirty = false; lastDraw = millis();
            }
            break;
        }
        }

        serialEvent();
        checkShutDown(false);
        delay(8);
    }
}

//=============================================================================
// Ranking screen
//=============================================================================

MorseGridNet::After MorseGridNet::results(LGFX_Sprite *canvas,
                                          unsigned long elapsedMs, int wrong,
                                          int steps) {
    myElapsedMs = (uint32_t)elapsedMs;
    myWrong     = (uint16_t)wrong;
    mpGame      = false;
    clearPaddleLatches();
    MorseOutput::resetTOT();

    unsigned long lastDraw = 0, lastResend = 0;

    // Server seeds an initial ranking containing only its own result so
    // something shows immediately; it grows as RESULTs trickle in.
    if (role == GRD_SERVER) {
        serverBuildRank();
        sendScores();
        rankLastBcast = millis();
        rankDirty = false;
    }
    drawRanking(canvas, steps);
    lastDraw = millis();

    for (;;) {
        netPump();                              // server: collect client RESULTs
        // Client resends its result periodically in case one was lost.
        if (role == GRD_CLIENT && millis() - lastResend > GRD_JOIN_MS) {
            sendResult();
            lastResend = millis();
        }
        // Server: rebuild + rebroadcast on new arrivals, or periodically as a
        // keepalive in case the last broadcast was lost.
        if (role == GRD_SERVER &&
            (rankDirty || millis() - rankLastBcast > GRD_SCORES_MS)) {
            if (rankDirty) serverBuildRank();
            sendScores();
            rankLastBcast = millis();
            rankDirty = false;
            drawRanking(canvas, steps);
            lastDraw = millis();
        }
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1)  return AGAIN;
        if (Buttons::modeButton.clicks == -1) return LEAVE;
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();

        if (millis() - lastDraw > 400) { drawRanking(canvas, steps); lastDraw = millis(); }
        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}

//=============================================================================
// Teardown — same reasoning as Morsel's run() exit: returning from a game
// keeps us inside the still-running menu_() loop, so its entry-time
// WiFi/ESP-NOW teardown does NOT re-run between two game launches. If
// ESP-NOW were left up, the next game's sprite allocation would land on a
// fragmented heap and fail into a reboot.
//=============================================================================

void MorseGridNet::teardown() {
    gridNetMode = false;
    mpGame = false;
    role = GRD_NONE;
    kochRestore();
    if (EspNowIsActive) {
        quickEspNow.stop();
        EspNowIsActive = false;
        WiFi.mode(WIFI_OFF);
    }
}

#endif  // CONFIG_CW_GAME

/******************************************************************************************************************************
 *  Fight the Pileup — Multiplayer CW game for Morserino-32 Pocket
 *  Copyright (C) 2025  Willi Kraml, OE1WKL
 *  Programmed with assistance by Claude (Opus 4.x)
 *
 *  Part of the Morserino-32 firmware. See main license.
 *****************************************************************************************************************************/

#include "MorsePileup.h"
#include "MorseGameMode.h"

bool pileupMode = false;
String pileupRxText = "";
volatile bool pileupRxReady = false;

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseCwEngine.h"      // shared non-blocking CW player
#include "morsedefs.h"
#include "ClickButton.h"
#include "DisplayWrapper.h"
#include <LovyanGFX.hpp>
#include <ESP32Encoder.h>
#include <Preferences.h>
#include <WiFi.h>

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
// 320 panel rows, but the sprite is trimmed by MORSE_GAMEMODE_SPRITE_TRIM
// (see MorseGameMode.cpp) to leave heap headroom for ESP-NOW/BT/WiFi.
// The bottom 16 px of the panel are not drawn; UI must lay out within FTP_H.
#define FTP_H  304
// The game sprite is 8-bpp indexed (see GamePalette.h): these names map to
// shared palette indices, not RGB565 values. The palette entries hold the
// exact RGB565 colours used before, so the on-screen result is unchanged.
#include "GamePalette.h"
#define FTP_BG     PAL_BG        // 0x0841
#define FTP_TEXT   PAL_WHITE     // 0xFFFF
#define FTP_ACCENT PAL_CYAN      // 0x07FF
#define FTP_OK     PAL_GREEN     // 0x07E0
#define FTP_WARN   PAL_RED       // 0xF800
#define FTP_TITLE  PAL_YELLOW    // 0xFFE0
#define FTP_DIM    PAL_GREY      // 0x7BEF
#define FTP_INPUT  PAL_BLUE      // 0x001F blue for input text
#define FTP_ATTACK PAL_MAGENTA   // 0xF81F magenta for attack prompt
#define FTP_BAND   PAL_DARKGREY  // 0x2104 dark grey status/footer band

// Difficulty presets
//                              label       timeout  spawnMax spawnMin initCal dropsPerLife playsReveal
static const FtpDifficulty difficulties[FTP_NUM_DIFFICULTIES] = {
    { "EASY",      45000,  12000,    5000,    1,      5,      3 },
    { "NORMAL",    30000,   8000,    3000,    1,      4,      3 },
    { "HARD",      20000,   5000,    2000,    2,      3,      2 },
    { "EXPERT",    12000,   3500,    1500,    3,      2,      1 },
};

static const FtpDifficulty& diff() { return difficulties[ftp.difficulty]; }

// Word-gap timeout: 7 dit-lengths of silence → submit input
#define FTP_WORDGAP_FACTOR 7

// CW playback: plays before the callsign is shown is per-difficulty now
// (FtpDifficulty::playsBeforeReveal — fewer on harder levels).

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
static void   ftpHandleMessage(const uint8_t* mac, char* line);
static const uint8_t* ftpMyMac();
static void   ftpMacToHex(const uint8_t* mac, char* out);
static bool   ftpHexToMac(const char* s, uint8_t* mac);
static int    ftpPickVictim();
static void   sendAttack(int victimIdx, const char* call);
static void   spawnNetworkAttack(const char* call, uint8_t senderIdx);
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
// Thin wrappers around MorseCwEngine. Pileup's call-site convention:
// pitch derived from the player's pitch pref (offset by
// FTP_ATTACK_PITCH_RATIO so the attack is distinguishable from the
// sidetone); WPM = 0 means the engine uses live keyer timings
// (ditLength/dahLength/interCharacterSpace), so the attack tracks the
// player's speed changes mid-game. The challenge loops with an inter-loop
// gap, and the engine settles for 2 dits after the mute predicate
// releases — both preserve the original cwPlayerUpdate behaviour.

static int getAttackPitch() {
    int basePitch = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    return basePitch * FTP_ATTACK_PITCH_RATIO_NUM / FTP_ATTACK_PITCH_RATIO_DEN;
}

// Mute predicate: silence playback whenever the user is typing a response
// or a Pileup sound effect is sounding. (keyerState != IDLE is handled
// by the engine itself.)
static bool pileupExtraMute() {
    return (ftp.inputPos > 0) || ftpSoundPlaying;
}

static void cwPlayerStart(const char* callsign) {
    String lower = callsign;
    lower.toLowerCase();                                 // generateCWword expects lowercase
    MorseCwEngine::PlayOpts opts = {
        /*pitchHz       */ (uint16_t) getAttackPitch(),
        /*wpm           */ 0,                            // use live ditLength globals
        /*loop          */ true,
        /*resumeGapDits */ 2,
        /*extraMute     */ pileupExtraMute,
    };
    MorseCwEngine::playStart(lower, opts);
}

static void cwPlayerStop()    { MorseCwEngine::playStop(); }
static void cwPlayerUpdate()  { MorseCwEngine::playTick(); }


//=== Drawing helpers ===

static void pushFrame() {
    MorseGameMode::pushFrame();
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

//=== Networking — plain-text /ftp/ packets over ESP-NOW broadcast ===
//
// Wire format (Option A from the handoff): human-readable, pipe-delimited
// ASCII so it is trivially parseable and debuggable. The RX *transport* is a
// lock-free-ish ring filled by the ESP-NOW callback and drained by the lobby
// loop; ftpHandleMessage() parses each drained line and updates the roster.
//
//   /ftp/B|<ident>|<lives>|<score>|<inPileup>    Beacon (every 5 s)

#define FTP_RX_RING   8
#define FTP_RX_MAX    80          // keep packets well under the 250 B ESP-NOW cap

// Producer = ESP-NOW RX callback (ftpNetOnRecv); consumer = lobby/game loop.
// The sender MAC travels with the payload — the roster is keyed by MAC.
struct FtpRx { uint8_t mac[6]; uint8_t len; char d[FTP_RX_MAX]; };
static volatile FtpRx   ftpRxRing[FTP_RX_RING];
static volatile uint8_t ftpRxHead = 0, ftpRxTail = 0;

// RX callback context — do the minimum: copy bytes, advance head, return.
// No parsing, no String, no Serial here (constraint #1 / ISR safety).
void MorsePileup::ftpNetOnRecv(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    uint8_t h = ftpRxHead;
    uint8_t nxt = (h + 1) % FTP_RX_RING;
    if (nxt == ftpRxTail) return;                       // ring full: drop
    memcpy((void*)ftpRxRing[h].mac, mac, 6);
    uint8_t n = (len < FTP_RX_MAX) ? len : (FTP_RX_MAX - 1);
    memcpy((void*)ftpRxRing[h].d, data, n);
    ftpRxRing[h].d[n] = '\0';
    ftpRxRing[h].len = n;
    ftpRxHead = nxt;
}

// Broadcast a beacon. Called from the lobby idle frame every FTP_BEACON_INTERVAL.
static void sendBeacon() {
    ftp.lastBeaconSent = millis();
    char buf[FTP_RX_MAX];
    int n = snprintf(buf, sizeof(buf), "%sB|%s|%u|%lu|%d",
                     FTP_MAGIC, getPlayerIdent(),
                     (unsigned)ftp.lives, (unsigned long)ftp.score,
                     (ftp.state == FTP_PILEUP) ? 1 : 0);
    if (n > 0)
        quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t*)buf, (size_t)n);
}

// This device's own STA MAC (the address peers see as the packet source).
static const uint8_t* ftpMyMac() {
    static uint8_t mac[6];
    static bool valid = false;
    if (!valid) { WiFi.macAddress(mac); valid = true; }
    return mac;
}

static void ftpMacToHex(const uint8_t* mac, char* out) {   // out: >= 13 bytes
    static const char* H = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        out[i * 2]     = H[(mac[i] >> 4) & 0x0F];
        out[i * 2 + 1] = H[mac[i] & 0x0F];
    }
    out[12] = '\0';
}

static int ftpHexNib(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool ftpHexToMac(const char* s, uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        int hi = ftpHexNib(s[i * 2]), lo = ftpHexNib(s[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Pick a random active, non-eliminated peer to attack; -1 if there are none.
static int ftpPickVictim() {
    int elig[FTP_MAX_PLAYERS], n = 0;
    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
        if (ftp.players[i].active && !ftp.players[i].eliminated) elig[n++] = i;
    if (n == 0) return -1;
    return elig[random(n)];
}

// Broadcast an attack addressed (by MAC) to one peer: /ftp/A|from|toMac|call
static void sendAttack(int victimIdx, const char* call) {
    if (victimIdx < 0 || victimIdx >= FTP_MAX_PLAYERS) return;
    char macHex[13];
    ftpMacToHex(ftp.players[victimIdx].mac, macHex);
    char buf[FTP_RX_MAX];
    int n = snprintf(buf, sizeof(buf), "%sA|%s|%s|%s",
                     FTP_MAGIC, getPlayerIdent(), macHex, call);
    if (n > 0)
        quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t*)buf, (size_t)n);
}

// Drain the RX ring on an idle lobby frame and dispatch each line.
static void checkReceivedMessages() {
    while (ftpRxTail != ftpRxHead) {
        uint8_t t = ftpRxTail;
        uint8_t len = ftpRxRing[t].len;
        if (len >= FTP_RX_MAX) len = FTP_RX_MAX - 1;
        char    line[FTP_RX_MAX];
        uint8_t mac[6];
        memcpy(line, (const void*)ftpRxRing[t].d, len);
        line[len] = '\0';
        memcpy(mac, (const void*)ftpRxRing[t].mac, 6);
        ftpRxTail = (t + 1) % FTP_RX_RING;
        ftpHandleMessage(mac, line);
    }
}

//=== Player roster (keyed by MAC) ===

// Split s in place on delim, preserving empty fields. Returns field count.
static int ftpSplit(char* s, char delim, char** out, int maxOut) {
    int n = 0;
    if (maxOut <= 0) return 0;
    out[n++] = s;
    for (char* p = s; *p && n < maxOut; ++p)
        if (*p == delim) { *p = '\0'; out[n++] = p + 1; }
    return n;
}

// Add or refresh a roster entry, keyed by MAC (two devices may share a
// callsign, so ident is display-only). Stores the latest lives/score/inPileup.
static void rosterUpdateBeacon(const uint8_t* mac, const char* ident,
                               uint8_t lives, uint32_t score, bool inPileup) {
    int slot = -1;
    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
        if (ftp.players[i].active && memcmp(ftp.players[i].mac, mac, 6) == 0) {
            slot = i; break;
        }
    if (slot < 0) {                              // new peer: claim a free slot
        for (int i = 0; i < FTP_MAX_PLAYERS; i++)
            if (!ftp.players[i].active) { slot = i; break; }
        if (slot < 0) return;                    // roster full: ignore
        memcpy(ftp.players[slot].mac, mac, 6);
        ftp.players[slot].active = true;
        ftp.playerCount++;
    }
    strncpy(ftp.players[slot].ident, ident, FTP_MAX_IDENT_LEN);
    ftp.players[slot].ident[FTP_MAX_IDENT_LEN] = '\0';
    ftp.players[slot].lives      = lives;
    ftp.players[slot].score      = score;
    ftp.players[slot].inPileup   = inPileup;
    ftp.players[slot].eliminated = (lives == 0);
    ftp.players[slot].lastBeacon = millis();
}

// Parse one received /ftp/ line (mutable; split in place) and dispatch by type.
static void ftpHandleMessage(const uint8_t* mac, char* line) {
    if (strncmp(line, FTP_MAGIC, FTP_MAGIC_LEN) != 0) return;
    char* body = line + FTP_MAGIC_LEN;           // e.g. "B|ident|lives|score|inP"
    char* f[6];
    int nf = ftpSplit(body, '|', f, 6);
    if (nf < 1 || f[0][0] == '\0') return;
    switch (f[0][0]) {
        case 'B':                                // Beacon
            if (nf >= 5)
                rosterUpdateBeacon(mac, f[1],
                                   (uint8_t)atoi(f[2]),
                                   (uint32_t)strtoul(f[3], nullptr, 10),
                                   atoi(f[4]) != 0);
            break;
        case 'A':                                // Attack: from | toMac | call
            if (nf >= 4) {
                uint8_t toMac[6];
                if (ftpHexToMac(f[2], toMac) &&
                    memcmp(toMac, ftpMyMac(), 6) == 0) {   // addressed to this device
                    int sidx = 0xFF;             // sender's roster slot (for the FROM label)
                    for (int i = 0; i < FTP_MAX_PLAYERS; i++)
                        if (ftp.players[i].active &&
                            memcmp(ftp.players[i].mac, mac, 6) == 0) { sidx = i; break; }
                    spawnNetworkAttack(f[3], (uint8_t)sidx);
                }
            }
            break;
        default:                                 // X / W arrive in later phases
            break;
    }
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
    ftp.callers[slot].queuedSince = millis();
    ftp.callers[slot].active = true;
    ftp.callers[slot].fromNetwork = false;
    ftp.callers[slot].senderIdx = 0;
    ftp.callers[slot].progress = 0.0f;
    ftp.callerCount++;
    ftp.lastSpawn = millis();
}

// Enqueue an attack received from another player (a specific callsign, not random).
// Does not touch lastSpawn — it must not perturb the bot-spawn cadence.
static void spawnNetworkAttack(const char* call, uint8_t senderIdx) {
    for (int i = 0; i < FTP_MAX_CALLERS; i++)        // ignore an identical active caller
        if (ftp.callers[i].active && strcmp(ftp.callers[i].call, call) == 0) return;
    int slot = -1;
    for (int i = FTP_MAX_CALLERS - 1; i >= 0; i--)
        if (!ftp.callers[i].active) { slot = i; break; }
    if (slot < 0) return;                            // queue full: drop
    strncpy(ftp.callers[slot].call, call, FTP_MAX_CALL_LEN);
    ftp.callers[slot].call[FTP_MAX_CALL_LEN] = '\0';
    ftp.callers[slot].spawnTime = millis();
    ftp.callers[slot].queuedSince = millis();
    ftp.callers[slot].active = true;
    ftp.callers[slot].fromNetwork = true;
    ftp.callers[slot].senderIdx = senderIdx;
    ftp.callers[slot].progress = 0.0f;
    ftp.callerCount++;
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
    ftp.singlePlayer = true;
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
    MorseCwEngine::playStop();
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
            canvas->setFont(&fonts::FreeSans9pt7b);
            drawCentredText(155, "MULTIPLAYER", FTP_TITLE);
            canvas->setFont(&fonts::Font0);
            int y = 178;
            int active2 = countActivePlayers();
            if (active2 == 0)
                drawCentredText(y, "Searching...", FTP_DIM);
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
            // Bring ESP-NOW up the first time the user enters multiplayer.
            // Teardown happens once on Pileup exit (mirrors MorseMorsel,
            // PR #172) — toggling back to single does not stop the radio.
            if (!ftp.singlePlayer && !EspNowIsActive) MorseMenu::setupESPNow();
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
    if (atk.fromNetwork) {
        const char* who = (atk.senderIdx < FTP_MAX_PLAYERS &&
                           ftp.players[atk.senderIdx].active)
                          ? ftp.players[atk.senderIdx].ident : "NET";
        char fb[24];
        snprintf(fb, sizeof(fb), "FROM %s", who);
        drawCentredText(y, fb, FTP_ATTACK);
    } else {
        drawCentredText(y, "DEFEND!", FTP_WARN);
    }

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

    // Show callsign text only after enough plays (fewer on harder levels).
    uint8_t playsToReveal = diff().playsBeforeReveal;
    if (MorseCwEngine::getPlayCount() >= playsToReveal) {
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        drawCentredText(y + 30, atk.call, atk.fromNetwork ? FTP_ATTACK : FTP_TEXT);
    } else {
        // Show play count
        char pbuf[20];
        snprintf(pbuf, sizeof(pbuf), "Listen... (%d/%d)",
                 MorseCwEngine::getPlayCount() + 1, playsToReveal);
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
    canvas->fillRect(0, 290, FTP_W, 30, FTP_BAND);
    canvas->setFont(&fonts::Font0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d wpm", ftp.wpm);
    canvas->setTextColor(FTP_TEXT, FTP_BAND);
    canvas->drawString(buf, 4, 298);

    if (ftp.singlePlayer) {
        canvas->setTextColor(FTP_TITLE, FTP_BAND);
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
        // Correct key — the attack is "sent".
        ftp.totalAttacksSent++;
        addScore(50);
        playSoundCorrect();
        attackPromptActive = false;
        if (!ftp.singlePlayer) {
            int v = ftpPickVictim();
            if (v >= 0) {
                sendAttack(v, expected);          // broadcast /ftp/A to that peer
                char fb[24];
                snprintf(fb, sizeof(fb), "ATTACK %s", ftp.players[v].ident);
                triggerFlash(FTP_ATTACK, fb);
            } else {
                triggerFlash(FTP_ATTACK, "NO TARGET");
            }
        } else {
            triggerFlash(FTP_ATTACK, "ATTACK SENT!");
        }
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

    currentAttackIdx = -1;
    attackPromptActive = false;
    clearAllCallers();
    for (int i = 0; i < diff().initialCallers && i < FTP_MAX_CALLERS; i++)
        spawnAttack();
    ftp.lastSpawn = millis();

    char challenge[FTP_MAX_CALL_LEN + 1] = "";   // active caller currently loaded into the CW player
    bool freshCaller = false;                    // true on the frame a new caller is loaded
    bool attackMode = false;                     // true while keying an earned attack (pileup paused)
    unsigned long attackModeStart = 0;           // millis() the pause began (freezes queue patience)
    bool waitingForResult = false;               // kept false: preserves the tuned tight-loop guard verbatim
    unsigned long lastSubmitTime = 0;
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
                        if (ftp.inputPos >= 1) {
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

                if (ftp.inputPos >= 1 && ftp.lastCharTime > 0 &&
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

        if (MorseCwEngine::isToneOn()) {
            cwPlayerUpdate();
            serialEvent();
            continue;
        }

        // Drain inbound /ftp/ packets (network attacks + beacons) on idle frames.
        // Not while an attack pause is active: callers enqueued during the pause
        // would be wrongly time-shifted on resume. They wait in the ring instead.
        if (!ftp.singlePlayer && !attackMode) checkReceivedMessages();

        // --- Earned-attack mode: after a correct defend the pileup pauses and the
        //     player keys ONE attack. No spawning / no timeouts while it is up. ---
        freshCaller = false;
        if (attackMode) {
            currentAttackIdx = -1;
            if (MorseCwEngine::isPlaying()) cwPlayerStop();
            challenge[0] = '\0';
            if (!attackPromptActive) generateAttackPrompt();
        } else {
            // Spawn bot callers on a timer (bots keep the pressure up).
            if (ftp.callerCount < FTP_MAX_CALLERS &&
                millis() - ftp.lastSpawn > ftp.spawnInterval) {
                spawnAttack();
            }
            // Lock onto one caller at a time. Pick a new one only when the
            // current is gone, and reset its timeout reference on activation so
            // EVERY caller gets its full defend window (queued callers wait
            // without ageing — only the active one counts down).
            attackPromptActive = false;
            if (currentAttackIdx < 0) {
                currentAttackIdx = findActiveAttack();
                if (currentAttackIdx >= 0)
                    ftp.callers[currentAttackIdx].spawnTime = millis();
            }
            if (currentAttackIdx >= 0) {
                // Load the active caller into the CW player when it changes.
                if (strcmp(challenge, ftp.callers[currentAttackIdx].call) != 0) {
                    strncpy(challenge, ftp.callers[currentAttackIdx].call, FTP_MAX_CALL_LEN);
                    challenge[FTP_MAX_CALL_LEN] = '\0';
                    cwPlayerStart(challenge);      // fresh caller: play from count 0
                    clearInput();
                    freshCaller = true;
                }
            } else {
                challenge[0] = '\0';
                if (MorseCwEngine::isPlaying()) cwPlayerStop();
            }
        }

        // --- CW player management for the active caller (tuned: stop-on-key, 2 s replay) ---
        if (currentAttackIdx >= 0) {
            if (ftp.inputPos > 0 || leftKey || rightKey || keyerState != IDLE_STATE) {
                if (MorseCwEngine::isPlaying()) cwPlayerStop();
            }
            if (keyerState == IDLE_STATE && ftp.inputPos == 0 &&
                !leftKey && !rightKey &&
                millis() - lastSubmitTime > 2000) {
                if (!MorseCwEngine::isPlaying() && challenge[0] != '\0' && !freshCaller) {
                    uint8_t saved = MorseCwEngine::getPlayCount();
                    cwPlayerStart(challenge);
                    MorseCwEngine::setPlayCount(saved);
                }
                cwPlayerUpdate();
                updateFtpSound();
            }
        }

        // --- Submission ---
        if (ftp.challengePos == 1) {
            ftp.challengePos = 0;
            if (attackMode) {
                handleAttackSubmit();              // single-player: scores only (P4b broadcasts /ftp/A)
                if (!attackPromptActive) {         // attack sent -> resume the pileup
                    // Unfreeze queue patience: callers did not age during the pause.
                    unsigned long paused = millis() - attackModeStart;
                    for (int i = 0; i < FTP_MAX_CALLERS; i++)
                        if (ftp.callers[i].active) ftp.callers[i].queuedSince += paused;
                    attackMode = false;
                    ftp.lastSpawn = millis();      // restart the bot-spawn cadence cleanly
                }
            } else if (currentAttackIdx >= 0) {
                handleDefendSubmit();              // correct: removeAttack + currentAttackIdx = -1
                if (currentAttackIdx < 0) {        // correct defend -> earn one keyed attack
                    challenge[0] = '\0';
                    attackMode = true;
                    attackModeStart = millis();
                    attackPromptActive = false;    // a fresh prompt is generated next frame
                }
            }
            lastSubmitTime = millis();
        }

        // --- Timeout of the active caller (counted from activation). Never while a
        //     response is in progress, so an answer is not cut off mid-key and the
        //     caller you are copying cannot be swapped out from under you. ---
        if (currentAttackIdx >= 0 && ftp.inputPos == 0 &&
            millis() - ftp.callers[currentAttackIdx].spawnTime > diff().callerTimeout) {
            cwPlayerStop();
            removeAttack(currentAttackIdx);
            currentAttackIdx = -1;
            challenge[0] = '\0';
            ftp.totalDropped++;
            ftp.streak = 0;
            addScore(FTP_SCORE_TIMEOUT);
            playSoundTimeout();
            triggerFlash(FTP_WARN, "TIMEOUT");
            clearInput();
            lastSubmitTime = millis();
        }

        // --- Backlog give-up: a caller waiting too long in the queue (not the one
        //     being defended) loses patience and leaves, counting as a drop. This is
        //     the pressure — fall behind and the pile thins itself at your expense. ---
        if (!attackMode) {
            unsigned long patience = diff().callerTimeout;
            for (int i = 0; i < FTP_MAX_CALLERS; i++) {
                if (!ftp.callers[i].active || i == currentAttackIdx) continue;
                if (millis() - ftp.callers[i].queuedSince > patience) {
                    removeAttack(i);
                    ftp.totalDropped++;
                    triggerFlash(FTP_WARN, "MISSED!");
                    break;                 // at most one give-up per frame (readable feedback)
                }
            }
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

        // --- Draw (revived queued DEFEND/ATTACK UI) ---
        canvas->fillSprite(FTP_BG);
        drawPileupHUD();        // lives | ident | score | streak
        drawDefendArea();       // active caller DEFEND, or "YOUR ATTACK" prompt, or waiting
        drawInputArea();        // keyed input, hints, wpm/SOLO status band
        drawFlash();            // transient OK/WRONG/TIMEOUT/LIFE LOST overlay
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
    canvas = MorseGameMode::enterPortrait(MorsePreferences::leftHanded, 8);
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
    // Tear down ESP-NOW if multiplayer brought it up. Mirrors MorseMorsel
    // (PR #172): menu_()'s WiFi/ESP-NOW teardown only re-runs on menu
    // *entry*, not between two game launches, so the radio would otherwise
    // stay up and fragment the heap against the next sprite allocation.
    if (EspNowIsActive) {
        quickEspNow.stop();
        EspNowIsActive = false;
        WiFi.mode(WIFI_OFF);
    }
    MorseGameMode::exit();
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks = 0;
}

#endif  // CONFIG_CW_GAME
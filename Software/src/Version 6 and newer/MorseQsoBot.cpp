// MorseQsoBot.cpp — see MorseQsoBot.h for the public interface.
//
// PR-3.5 scope (SOTA/POTA):
//   - Table-driven matcher (SlotGrammar) with noise filtering + concat.
//   - Dynamic opening: the bot waits ~5 s for the user to call CQ.
//       * user calls CQ  -> the user is the activator; the bot answers
//                            as a chaser (and does NOT send a location
//                            reference, except on a small S2S/P2P chance).
//       * user is silent -> the bot calls CQ as the activator, randomly
//                            choosing SOTA or POTA, and sends its ref.
//     (There is no "Bot Role" dependence any more — who calls CQ decides
//     who is the activator, matching real SOTA/POTA practice.)
//   - Deferred turn-taking: the bot does NOT act on a matched value until
//     the user signals end-of-over (k / <sk> / <ar> / 73 / 72) or goes
//     silent for a few seconds. This stops the bot talking over the user
//     mid-transmission.
//   - Cut numbers (0/1/9), partial-aware repeat (agn / rpt [slot]).
//
// Display: standard CW-Keyer layout. The user's keyed characters flow
// through the firmware decoder pipeline (displayDecodedMorse ->
// printToScroll, REGULAR); the bot's CW is shown via displayGeneratedMorse
// (BOLD), one char per audio character. morseState is morseQsoBot, which
// keeps RF transmission gated off.

#include "MorseQsoBot.h"

#ifdef CONFIG_QSO_BOT

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"            // MorseMenu::cleanupScreen, keyDecoder
#include "MorseCwEngine.h"
#include "MorseGame.h"            // qsoBotMode, gameCharBuffer
#include "morsedefs.h"
#include "ClickButton.h"
#include "qso_content.h"
#include "MorseQsoBotMatch.h"     // pure matcher primitives (unit-tested off-device)
#include <Preferences.h>
#include <ctype.h>

// ---- Externs from m32_v6.ino -------------------------------------------
extern int          checkEncoder();
extern void         serialEvent();
extern boolean      checkPaddles();
extern boolean      doPaddleIambic(boolean, boolean);
extern boolean      leftKey, rightKey;
extern void         clearPaddleLatches();
extern KEYERSTATES  keyerState;
extern unsigned int ditLength;
extern unsigned int interWordSpace;
extern String       getRandomCall(int maxLength);
extern void         displayCWspeed();
extern void         displayGeneratedMorse(FONT_ATTRIB style, const String& s);
extern void         changeSpeed(int t);
extern void         changeVolume(int t);
extern void         updateTopLine();
extern boolean      speedChanged;
extern encoderMode  encoderState;
extern uint8_t      lastGeneratedCallContinent;   // set by getRandomCall
extern uint8_t      lastGeneratedCallCqZone;       // set by getRandomCall

namespace {

using namespace MorseQsoBot;
using namespace QsoMatch;     // looksLikeCallsign, matchRST, noise tables, InfoParser, ...

// ---- Input accumulator (private to this module) ------------------------

constexpr int kInputMax = 31;
char           gInputBuf[kInputMax + 1] = "";
uint8_t        gInputLen = 0;
unsigned long  gLastCharTime = 0;   // 0 means "submit pending" or "idle"
bool           gEeeeDetected = false;

extern unsigned long gStateStart;   // defined in the runtime block; reset
                                    // from inputFeed so timers count from
                                    // the user's last keying activity.

void inputReset() {
    gInputBuf[0]  = '\0';
    gInputLen     = 0;
    gLastCharTime = 0;
}

void inputFeed(char c) {
    gStateStart = millis();         // activity refreshes the active timer
    if (c == ' ') {
        if (gInputLen > 0) gLastCharTime = 0;   // word boundary -> submit-ready
        return;
    }

    // <err> / <hh> (a run of dits) is the CW "disregard what I just sent"
    // signal. The decoder delivers it as the *uppercase* prosign code
    // 'R' (the letter R arrives as lowercase 'r'), so we can tell them
    // apart here, BEFORE the lowercase->uppercase fold below. Treat it
    // like "eeee": clear the current word and raise the retract flag so
    // the EXPECT handler drops any already-matched value for this slot
    // and listens for the corrected one.
    if (c == 'R') {
        gInputBuf[0]  = '\0';
        gInputLen     = 0;
        gLastCharTime = 0;
        gEeeeDetected = true;
        return;
    }

    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    if (gInputLen < kInputMax) {
        gInputBuf[gInputLen++] = c;
        gInputBuf[gInputLen] = '\0';
    }
    gLastCharTime = millis();

    // "eeee" retract: four consecutive E's mean "ignore what I sent".
    if (gInputLen >= 4 &&
        gInputBuf[gInputLen - 1] == 'E' && gInputBuf[gInputLen - 2] == 'E' &&
        gInputBuf[gInputLen - 3] == 'E' && gInputBuf[gInputLen - 4] == 'E') {
        gInputBuf[0]  = '\0';
        gInputLen     = 0;
        gLastCharTime = 0;
        gEeeeDetected = true;
    }
}

const char* inputPeek()     { return gInputBuf; }
bool        inputMidToken() { return gInputLen > 0 && gLastCharTime > 0; }

// True when the current word should be consumed (a space arrived or a
// word gap elapsed).
bool inputSubmitReady() {
    if (gInputLen == 0) return false;
    if (gLastCharTime == 0) return true;
    unsigned long wordGap = interWordSpace + ditLength;
    if (wordGap < 1200) wordGap = 1200;
    return (millis() - gLastCharTime > wordGap);
}

// ---- Actors filled at QSO start ---------------------------------------

struct QsoActors {
    String botCall;
    String botRef;
    String botName;
    String botQth;
    String botRig;
    String botAnt;
    String botWx;
    String botAge;
    String userCall;
    String userName;        // parsed from the user's Standard-QSO over (for echo)
};

QsoActors gActors;
uint8_t   gActivity         = 0;       // 0 = SOTA, 1 = POTA (bot-as-activator)
bool      gBotAlsoActivator = false;   // S2S/P2P: bot answers AND sends a ref
uint8_t   gContestType      = 0;       // 0 = CQ WW (zone), 1 = WPX/Sprint (serial)

// QSO Difficulty (posQsoBotLevel): how forgiving and how chatty the bot is.
// Read once at run() start. Several behaviours below scale with it: retry
// budget, patience timeouts, recovery-prompt tone, and (Beginner) spelling out
// cut-number RSTs. Future items (speed mismatch T2.2, formality T2.3) will read
// it too.
enum BotLevel : uint8_t { LVL_BEGINNER = 0, LVL_INTERMEDIATE = 1, LVL_ADVANCED = 2 };
uint8_t   gLevel            = LVL_INTERMEDIATE;

uint8_t   gBotZone          = 14;      // bot's CQ zone (CQ WW exchange)
uint16_t  gBotSerial        = 1;       // bot's serial (WPX/Sprint exchange)

// Independent bot "fist" speed (T2.2). 0 = track the user's live keyer speed
// (the original behaviour, preserving Farnsworth). When it differs from the
// user's current WpM, startBotTx keys at this fixed speed instead, so the user
// has to send qrs/qrq to bring the bot into copy range.
uint8_t   gBotWpm           = 0;
constexpr uint8_t kQrsStep  = 3;       // WpM step per qrs/qrq request

// (Re)pick the bot's per-QSO identity: a fresh callsign and everything
// derived from it (continent-matched ref, CQ zone), a name, the S2S
// chance, a per-QSO sending speed, and — for contests — a fresh random serial
// (each CQ is a different station). Called at session start and at every
// contest QSO loop so the user works/copies a new station each time.
void pickBotIdentity() {
    String call = getRandomCall(0);                    // sets lastGeneratedCall*
    const uint8_t cont = lastGeneratedCallContinent;
    gBotZone = lastGeneratedCallCqZone;
    if (gBotZone < 1 || gBotZone > 40) gBotZone = random(1, 41);
    call.toUpperCase();
    gActors.botCall  = call;
    gActivity        = random(2);
    gActors.botRef   = String(gActivity == 0 ? QsoContent::pickSotaRef(cont)
                                             : QsoContent::pickPotaRef(cont));
    gActors.botName  = String(QsoContent::pickName(cont));   // continent-matched
    gActors.botQth   = String(QsoContent::pickQth(cont));    // continent-matched
    gActors.botRig   = String(QsoContent::kRigs[random(QsoContent::kRigsCount)]);
    gActors.botAnt   = String(QsoContent::kAnts[random(QsoContent::kAntsCount)]);
    gActors.botWx    = String(QsoContent::kWxs[random(QsoContent::kWxsCount)]);
    gActors.botAge   = String(QsoContent::kAges[random(QsoContent::kAgesCount)]);
    gBotAlsoActivator = (random(100) < 15);
    gBotSerial       = random(1, 600);                 // random per station

    // Sending speed: default to the user's speed (gBotWpm == user WpM makes
    // startBotTx fall back to live timings, so no behaviour change). On
    // Intermediate/Advanced, sometimes give this station a fist a few WpM off
    // so the user must use qrs/qrq — Beginner never gets a mismatch.
    const uint8_t userWpm = MorsePreferences::wpm;
    gBotWpm = userWpm;
    if (gLevel != LVL_BEGINNER) {
        const int chance = (gLevel == LVL_ADVANCED) ? 50 : 30;        // % of QSOs
        if ((int) random(100) < chance) {
            const int delta = (gLevel == LVL_ADVANCED) ? (int) random(4, 9) : 4;  // ±4..8 / ±4
            const int dir   = random(2) ? 1 : -1;
            int w = constrain((int) userWpm + dir * delta,
                              (int) MorsePreferences::wpmMin, (int) MorsePreferences::wpmMax);
            if (w == (int) userWpm)        // clamp collapsed it -> try the other way
                w = constrain((int) userWpm - dir * delta,
                              (int) MorsePreferences::wpmMin, (int) MorsePreferences::wpmMax);
            gBotWpm = (uint8_t) w;
        }
    }
}

// qrs (slower) / qrq (faster): step the bot's fist toward what the user asked
// for, clamped to the keyer's legal range. Always honoured, at every level —
// it is a core CW skill, independent of whether this QSO started mismatched.
void adjustBotSpeed(int dir) {
    int w = (gBotWpm > 0) ? (int) gBotWpm : (int) MorsePreferences::wpm;
    w += dir * (int) kQrsStep;
    gBotWpm = (uint8_t) constrain(w, (int) MorsePreferences::wpmMin,
                                     (int) MorsePreferences::wpmMax);
}

void initActors() {
    pickBotIdentity();

    Preferences pref;
    pref.begin("morserino", true);
    String userCall = pref.getString("playerCall", "");
    pref.end();
    if (userCall.length() == 0) userCall = "OE1XXX";
    userCall.toUpperCase();
    gActors.userCall = userCall;
}

// Expand placeholders. Values inserted LOWERCASE (generateCWword treats
// uppercase SANKEBH as prosign markers). [S2SREF] is expanded before
// [BOTREF] because it itself contains [BOTREF].
String expand(const char* tmpl) {
    String out(tmpl);
    out.replace("[ACT]",    gActivity == 0 ? "sota" : "pota");
    out.replace("[S2SREF]", gBotAlsoActivator ? " = qth [BOTREF] [BOTREF]" : "");
    // Contest exchange: CQ zone (CQ WW) or 3-digit serial (WPX/Sprint).
    {
        String exch;
        if (gContestType == 0) { exch = String((int) gBotZone); }
        else { char b[8]; snprintf(b, sizeof(b), "%03u", (unsigned) gBotSerial); exch = b; }
        out.replace("[BOTEXCH]", exch);
    }
    String call(gActors.botCall);  call.toLowerCase();
    String ref(gActors.botRef);    ref.toLowerCase();
    String name(gActors.botName);  name.toLowerCase();
    String user(gActors.userCall); user.toLowerCase();
    String uname(gActors.userName.length() ? gActors.userName : String("om"));
    uname.toLowerCase();
    String qth(gActors.botQth);  qth.toLowerCase();
    String rig(gActors.botRig);  rig.toLowerCase();
    String ant(gActors.botAnt);  ant.toLowerCase();
    String wx(gActors.botWx);    wx.toLowerCase();
    String age(gActors.botAge);  age.toLowerCase();
    out.replace("[BOTCALL]",  call);
    out.replace("[BOTREF]",   ref);
    out.replace("[BOTNAME]",  name);
    out.replace("[BOTQTH]",   qth);
    out.replace("[BOTRIG]",   rig);
    out.replace("[BOTANT]",   ant);
    out.replace("[BOTWX]",    wx);
    out.replace("[BOTAGE]",   age);
    out.replace("[USERCALL]", user);
    out.replace("[USERNAME]", uname);
    return out;
}

// ---- Runtime state ----------------------------------------------------

enum RuntimeState { RT_OPENING, RT_BOT_TX, RT_EXPECT, RT_PAUSE, RT_DONE };

const QsoDescriptor* gDesc          = nullptr;
uint8_t              gPc             = 0;
RuntimeState         gState          = RT_DONE;
unsigned long        gStateStart     = 0;
uint8_t              gRetriesLeft    = 0;
bool                 gRecoveryActive = false;
String               gConcatBuf;

// Dynamic-opening state.
bool                 gUserStartedCalling = false;
bool                 gOpeningCallSeen    = false;   // a callsign captured this opening
unsigned long        gOpeningStart       = 0;
unsigned long        gOpeningActivity    = 0;
constexpr unsigned long kOpeningWaitMs    = 5000;
constexpr unsigned long kCallEndSilenceMs = 1800;

// Contest session state. A contest is a continuous run of short QSOs:
// whoever calls CQ keeps running, the bot uses a fresh identity each QSO,
// and the session ends only on ~15 s of inactivity.
bool                 gSessionMode = false;      // true for the Contest descriptor
constexpr unsigned long kSessionIdleMs = 15000; // CQ/answer inactivity -> end

// Per-WAIT behaviour (set by enterStep before entering RT_OPENING):
bool                 gWaitTimeoutEndsSession = false;  // timeout ends session?
uint8_t              gWaitTimeoutTarget      = 0;       // else jump here on timeout
uint8_t              gWaitUserCqTarget       = 0xFF;    // 0xFF = advance(pc+1);
                                                        // else re-init identity + jump here

// Per-over accumulators (EXPECT). The bot collects the user's whole over
// and only acts at end-of-over (explicit marker or silence), so it never
// cuts in while the user is still keying.
bool          gOverStarted     = false;   // user keyed something this over
bool          gOverMatched     = false;   // a word/concat filled the slot
String        gOverMatchedValue;
bool          gOverRepeat      = false;    // "agn"/"rpt" seen this over
String        gOverRepeatSlot;             // optional slot after "rpt"
unsigned long gOverActivity    = 0;        // last keying time this over
constexpr unsigned long kOptNoReplyMs     = 4000;   // optional ack, no reply

// Patience windows scale with QSO Difficulty: a Beginner gets more time to
// compose and is nagged later; an Advanced op is held to a tighter rhythm.
// (Intermediate keeps the original 2500 / 12000 ms values.)
unsigned long overEndSilenceMs() {                  // user finished (no marker)
    return gLevel == LVL_BEGINNER ? 3200 : gLevel == LVL_ADVANCED ? 2000 : 2500;
}
unsigned long noReplyMs() {                         // user never replied
    return gLevel == LVL_BEGINNER ? 16000 : gLevel == LVL_ADVANCED ? 9000 : 12000;
}

// Last real bot over (expanded), for full-repeat on "agn"/"rpt".
String        gLastBotTx;
String        gRepeatSlot;                 // used by executePendingRepeat

// RST received from the user (validity note at end of QSO).
bool          gRstReceived = false;

// Formality mirroring (T2.3). gQsoWarm becomes true once the user has sent a
// first info over (so openings stay formal); gUserInformal tracks whether that
// over dropped the "<call> de <call>" framing. The bot then drops its own
// preamble on mid-QSO overs — see maybeDropPreamble. Only Standard QSOs use
// info overs, so SOTA/POTA and Contest are unaffected.
bool          gQsoWarm     = false;
bool          gUserInformal = false;

// ---- Bot CW display (char-by-char via displayGeneratedMorse) ----------

String        gCachedBotText;
int           gDisplayPos = 0;

// CWchars uppercase S/A/N/K/E/B/H are prosign markers; display them as
// <xx>. Lowercase letters display as-is.
const char* prosignText(char c) {
    switch (c) {
        case 'S': return "<as>"; case 'A': return "<ka>";
        case 'N': return "<kn>"; case 'K': return "<sk>";
        case 'E': return "<ve>"; case 'B': return "<bk>";
        case 'H': return "<ch>"; default: return nullptr;
    }
}

void emitNextBotChar() {
    while (gDisplayPos < (int)gCachedBotText.length() &&
           gCachedBotText[gDisplayPos] == ' ') {
        displayGeneratedMorse(BOLD, " ");
        gDisplayPos++;
    }
    if (gDisplayPos < (int)gCachedBotText.length()) {
        char c = gCachedBotText[gDisplayPos++];
        const char* ps = prosignText(c);
        if (ps) displayGeneratedMorse(BOLD, String(ps));
        else    { char buf[2] = { c, '\0' }; displayGeneratedMorse(BOLD, String(buf)); }
    }
}

void onBotCharComplete() {
    if (gCachedBotText.length() == 0) return;
    emitNextBotChar();
    if (gDisplayPos >= (int)gCachedBotText.length()) {
        displayGeneratedMorse(BOLD, "\n");
        gCachedBotText = "";
    }
}

void renderInfo(const char* text) {
    String line = "\n[";
    line += text;
    line += "]\n";
    MorseOutput::printToScroll(REGULAR, line, true, encoderState == scrollMode);
}

// ---- Matcher ----------------------------------------------------------
//
// The pure token classifiers (looksLikeCallsign, normalizeCutNumbers,
// matchRST, matchZone, matchSerial, matchProsignTok, the noise tables,
// isEndOfOver/isSlotKeyword/isRepeatTrigger, and the SLOT_INFO parser) live
// in MorseQsoBotMatch.h so they can be unit-tested off-device. The three
// context-dependent matchers below are thin bridges that supply the per-QSO
// state (gActors / gContestType) to the two-argument forms in that header.

bool matchCallsign(const char* tok) { return QsoMatch::matchCallsign(tok, gActors.botCall); }
bool matchRef     (const char* tok) { return QsoMatch::matchRef(tok, gActors.botRef); }
bool matchExchange(const char* tok) { return QsoMatch::matchExchange(tok, gContestType); }

// SLOT_INFO keyword-field parser state for the current over (see InfoParser
// in MorseQsoBotMatch.h). One instance, reset per over.
QsoMatch::InfoParser gInfo;

typedef bool (*SlotAccept)(const String& tok, const String& expected);

struct SlotGrammar {
    SlotAccept         accept;
    const char* const* extraNoise;
    bool               allowConcat;
};

bool acceptCallsign(const String& tok, const String&)   { return matchCallsign(tok.c_str()); }
bool acceptRST     (const String& tok, const String&)   { return matchRST(tok.c_str()); }
bool acceptRef     (const String& tok, const String&)   { return matchRef(tok.c_str()); }
bool acceptProsign (const String& tok, const String& e) { return matchProsignTok(tok.c_str(), e.c_str()); }
bool acceptExchange(const String& tok, const String&)   { return matchExchange(tok.c_str()); }

// Noise tables (kCommonNoise, kCallsignNoise, ...) live in MorseQsoBotMatch.h
// and are visible here via `using namespace QsoMatch`.

const SlotGrammar& grammarFor(QsoSlotKind slot) {
    static const SlotGrammar kGrammars[] = {
        /* SLOT_NONE     */ { nullptr,        kProsignNoise,  false },
        /* SLOT_CALLSIGN */ { acceptCallsign, kCallsignNoise, true  },
        /* SLOT_RST      */ { acceptRST,      kRstNoise,      true  },
        /* SLOT_REF      */ { acceptRef,      kRefNoise,      true  },
        /* SLOT_PROSIGN  */ { acceptProsign,  kProsignNoise,  false },
        /* SLOT_EXCHANGE */ { acceptExchange, kExchangeNoise, true  },
        /* SLOT_INFO     */ { nullptr,        kProsignNoise,  false },  // parser-based
    };
    return kGrammars[slot];
}

// Noise check for a slot's grammar: delegates to the header (kCommonNoise +
// the slot's extra-noise list). isEndOfOver / isSlotKeyword / isRepeatTrigger
// and tokenInList also live in MorseQsoBotMatch.h (visible via using).
bool isNoise(const String& tok, const SlotGrammar& g, const String& expected) {
    return isNoiseToken(tok, g.extraNoise, expected);
}

bool matchSlot(QsoSlotKind kind, const char* tok, const char* expected) {
    const SlotGrammar& g = grammarFor(kind);
    if (!g.accept) return false;
    return g.accept(String(tok), String(expected ? expected : ""));
}

// ---- State transitions -----------------------------------------------

void startBotTx(const String& text);
void startExpect(uint16_t budget);
void reEnterExpect();
void advance();
void enterStep();
void startOpening();
void pickBotIdentity();

// Formality mirroring (T2.3): once a QSO has warmed up, drop the bot's leading
// "<usercall> de <botcall>" preamble to match the user's informality. Returns
// the text unchanged unless it actually starts with that preamble, so openings
// (which precede warm-up) and closings (preamble at the END) keep full
// formality automatically. Beginner is always formal; Intermediate mirrors the
// user (drops only when they dropped it); Advanced drops readily once warm.
String maybeDropPreamble(const String& text) {
    if (gLevel == LVL_BEGINNER || !gQsoWarm) return text;
    const bool drop = gUserInformal || (gLevel == LVL_ADVANCED);
    if (!drop) return text;
    String user(gActors.userCall); user.toLowerCase();
    String bot(gActors.botCall);   bot.toLowerCase();
    String pre = user + " de " + bot;
    if (!text.startsWith(pre)) return text;
    String rest = text.substring(pre.length());
    rest.trim();
    if (rest.startsWith("=")) { rest = rest.substring(1); rest.trim(); }
    return rest;
}

void startBotTx(const String& text) {
    String tx = maybeDropPreamble(text);
    // Beginner: spell RST out in full ("5nn" -> "599") — easier to copy than
    // the cut form. Safe to do globally here: "5nn" never occurs in a serial
    // exchange (those are plain digits). Advanced/Intermediate keep the cut
    // form the templates already use. Single choke point so it also covers
    // repeats and recovery prompts, and display matches what is keyed.
    if (gLevel == LVL_BEGINNER) tx.replace("5nn", "599");
    gCachedBotText = tx;
    gDisplayPos    = 0;
    emitNextBotChar();
    MorseCwEngine::PlayOpts opts = {
        /*pitchHz        */ (uint16_t) MorseOutput::notes[
                                MorsePreferences::pliste[posPitch].value],
        // Key at the bot's own fist only when it differs from the user's speed;
        // otherwise 0 = live timings (unchanged behaviour, keeps Farnsworth).
        /*wpm            */ (gBotWpm && gBotWpm != MorsePreferences::wpm) ? gBotWpm : (uint8_t) 0,
        /*loop           */ false,
        /*resumeGapDits  */ 2,
        /*extraMute      */ nullptr,
        /*onCharComplete */ onBotCharComplete,
    };
    MorseCwEngine::playStart(tx, opts);
    gState      = RT_BOT_TX;
    gStateStart = millis();
}

void resetOverAccumulators() {
    gOverStarted = false;
    gOverMatched = false;
    gOverMatchedValue = "";
    gOverRepeat  = false;
    gOverRepeatSlot = "";
    gOverActivity = millis();
    gConcatBuf   = "";
    gEeeeDetected = false;
    gInfo.reset();
}

void startExpect(uint16_t budget) {
    inputReset();
    resetOverAccumulators();
    uint8_t base;
    if (budget == 0) {
        const QsoStep& step = gDesc->steps[gPc];
        base = (step.slot == SLOT_CALLSIGN) ? 4 : 2;
    } else {
        base = (uint8_t) budget;
    }
    // Difficulty: a Beginner gets an extra try, an Advanced op one fewer
    // (never below 1, so the bot always asks at least once before giving up).
    int adj = (int) base + (gLevel == LVL_BEGINNER ? 1 : gLevel == LVL_ADVANCED ? -1 : 0);
    gRetriesLeft = (uint8_t) (adj < 1 ? 1 : adj);
    gState      = RT_EXPECT;
    gStateStart = millis();
}

void reEnterExpect() {
    inputReset();
    resetOverAccumulators();
    gState      = RT_EXPECT;
    gStateStart = millis();
}

void startOpening() {
    inputReset();
    gConcatBuf          = "";
    gUserStartedCalling = false;
    gOpeningCallSeen    = false;
    gOpeningStart       = millis();
    gOpeningActivity    = millis();
    gState              = RT_OPENING;
}

void enterStep() {
    if (gPc >= gDesc->stepCount) { renderInfo("QSO complete"); gState = RT_DONE; return; }
    const QsoStep& step = gDesc->steps[gPc];
    switch (step.kind) {
        case STEP_END:
            renderInfo(gRstReceived ? "QSO complete" : "QSO complete (no RST)");
            gState = RT_DONE;
            break;
        case STEP_BOT_TX:
            gLastBotTx = expand(step.tmpl);     // remember for full-repeat
            startBotTx(gLastBotTx);
            break;
        case STEP_EXPECT:
        case STEP_EXPECT_OPT:
            startExpect(step.arg);
            break;
        case STEP_WAIT_USER_CQ:
            // Initial opening: timeout -> jump to the bot-init branch
            // (step.arg); a user CQ falls through to pc+1.
            gWaitTimeoutEndsSession = false;
            gWaitTimeoutTarget      = (uint8_t) step.arg;
            gWaitUserCqTarget       = 0xFF;
            startOpening();
            break;
        case STEP_WAIT_CQ_LOOP:
            // Contest loop wait: a user CQ re-inits the bot identity and
            // jumps to step.arg; timeout ends the session.
            gWaitTimeoutEndsSession = true;
            gWaitUserCqTarget       = (uint8_t) step.arg;
            startOpening();
            break;
        case STEP_LOOP:
            pickBotIdentity();                  // new station for the next QSO
            gPc = (uint8_t) step.arg;
            enterStep();
            break;
        case STEP_PAUSE_MS:
            gState = RT_PAUSE; gStateStart = millis();
            break;
    }
}

void advance() { gPc++; enterStep(); }

// Resolve the end of a STEP_WAIT_* opening once the user has called CQ.
void finishOpening() {
    if (gWaitUserCqTarget != 0xFF) {
        pickBotIdentity();                      // a fresh station answers/runs
        gPc = gWaitUserCqTarget;
        enterStep();
    } else {
        advance();                              // fall through to pc+1
    }
}

// Varied recovery prompts (T1.3): instead of looping the identical nag, the
// bot picks one at random. All are short (OLED status budget), lowercase (no
// uppercase SANKEBH prosign markers), and unambiguous CW abbreviations.
//
// The tone also scales with QSO Difficulty: a Beginner gets clearer, calmer
// prompts; an Advanced op gets the curt forms a seasoned operator would send.
// (Intermediate keeps the original mixed pools.)
const char* const kCallRecovery[]    = { "qrz?", "agn agn", "call?", "pse agn" };
const char* const kCallRecoveryBeg[] = { "qrz?", "pse agn", "ur call agn?" };
const char* const kCallRecoveryAdv[] = { "qrz?", "agn", "call?" };

const char* const kGenRecovery[]     = { "agn agn", "agn?", "pse rpt", "hw?" };
const char* const kGenRecoveryBeg[]  = { "pse agn", "agn agn", "pse rpt" };
const char* const kGenRecoveryAdv[]  = { "agn", "agn?", "?" };

const char* const kRstRecovery[]     = { "pse ur rst?", "rst?", "ur rst agn?", "pse rpt rst" };
const char* const kRstRecoveryBeg[]  = { "pse ur rst?", "ur rst agn?" };
const char* const kRstRecoveryAdv[]  = { "rst?", "rst pse" };

template <int N>
const char* pickPrompt(const char* const (&pool)[N]) { return pool[random(N)]; }

// Recovery prompt for a failed EXPECT: a callsign-specific pool (the bot is
// asking who is calling) or the generic "didn't copy" pool, by difficulty.
const char* recoveryPrompt(QsoSlotKind slot) {
    if (slot == SLOT_CALLSIGN) {
        return gLevel == LVL_BEGINNER ? pickPrompt(kCallRecoveryBeg)
             : gLevel == LVL_ADVANCED ? pickPrompt(kCallRecoveryAdv)
                                      : pickPrompt(kCallRecovery);
    }
    return gLevel == LVL_BEGINNER ? pickPrompt(kGenRecoveryBeg)
         : gLevel == LVL_ADVANCED ? pickPrompt(kGenRecoveryAdv)
                                  : pickPrompt(kGenRecovery);
}

// Recovery prompt when a Standard-QSO over arrived without the required RST.
const char* rstRecoveryPrompt() {
    return gLevel == LVL_BEGINNER ? pickPrompt(kRstRecoveryBeg)
         : gLevel == LVL_ADVANCED ? pickPrompt(kRstRecoveryAdv)
                                  : pickPrompt(kRstRecovery);
}

// Send a recovery / repeat prompt without advancing; on completion we
// re-enter the same EXPECT (does not spend a retry).
void enterRecovery(const char* prompt) {
    gRecoveryActive = true;
    startBotTx(String(prompt));
}

String repeatSnippetFor(const String& kw) {
    String k(kw); k.toLowerCase();
    if (k == "rst") return String("ur 5nn 5nn");
    if (k == "call" || k == "cl" || k == "ur")
        { String c(gActors.botCall); c.toLowerCase(); return c + " " + c; }
    if (k == "qth" || k == "loc" || k == "ref" || k == "sota" ||
        k == "pota" || k == "summit")
        { String r(gActors.botRef); r.toLowerCase(); return "qth " + r + " " + r; }
    if (k == "name" || k == "op" || k == "nm")
        { String n(gActors.botName); n.toLowerCase(); return "name " + n; }
    return "";
}

void executePendingRepeat() {
    String snippet = gRepeatSlot.length() ? repeatSnippetFor(gRepeatSlot) : String("");
    if (snippet.length() == 0) snippet = gLastBotTx;     // full repeat
    if (snippet.length() == 0) snippet = String("agn");
    gRepeatSlot = "";
    enterRecovery(snippet.c_str());
}

// Act on a completed user over for the current EXPECT step.
void processOver(const QsoStep& step) {
    if (gOverRepeat) {
        gRepeatSlot = gOverRepeatSlot;
        executePendingRepeat();
        return;
    }
    if (step.slot == SLOT_INFO) {
        // Standard QSO: capture the user's name for echoing back.
        if (gInfo.name.length()) gActors.userName = gInfo.name;
        // Formality mirroring (T2.3): the QSO has warmed up, and we note
        // whether this over kept or dropped the formal call framing.
        gQsoWarm      = true;
        gUserInformal = !gInfo.framed;
        if (step.kind == STEP_EXPECT_OPT) { advance(); return; }   // layer 2: just ack
        // Layer 1 is required to carry the RST.
        if (gInfo.rst) { gRstReceived = true; advance(); return; }
        if (gRetriesLeft > 0) { gRetriesLeft--; enterRecovery(rstRecoveryPrompt()); return; }
        advance();                          // asked enough; proceed anyway
        return;
    }
    if (gOverMatched) {
        if (step.slot == SLOT_CALLSIGN) gActors.userCall = gOverMatchedValue;
        if (step.slot == SLOT_RST)      gRstReceived = true;
        advance();
        return;
    }
    if (step.kind == STEP_EXPECT_OPT) { advance(); return; }
    if (gRetriesLeft > 0) {
        gRetriesLeft--;
        enterRecovery(recoveryPrompt(step.slot));
    } else {
        renderInfo("Giving up");
        gState = RT_DONE;
    }
}

}  // namespace

namespace MorseQsoBot {

// ===========================================================================
// SOTA/POTA descriptor (PR-3.5)
// ===========================================================================
//
// Casing: lowercase = ordinary letters; uppercase SANKEBH = prosign
// markers. Inter-over turns use lowercase "k"/"bk"; the final sign-off
// uses uppercase K = <sk>.
//
// The descriptor opens with STEP_WAIT_USER_CQ (arg = pc of the bot-init
// branch). Falling through = the user called CQ (bot answers as chaser);
// jumping = the user was silent (bot calls CQ as activator).
//
//   [ACT]    -> "sota" / "pota" (the bot's randomly chosen activity)
//   [S2SREF] -> "" normally, or " = qth <ref> <ref>" on the S2S chance
//               (so an answered CQ occasionally reveals the bot is also
//                an activator — Summit/Park-to-Summit/Park).

static const QsoStep kSotaPotaSteps[] = {
    /* 0 */ { STEP_WAIT_USER_CQ, nullptr,                                       SLOT_NONE,     5 },
    // user-initiated: user = activator, bot = chaser (no ref of its own,
    // unless the S2S chance fired -> [S2SREF]).
    /* 1 */ { STEP_BOT_TX,  "[BOTCALL] [BOTCALL]",                              SLOT_NONE,     0 },
    /* 2 */ { STEP_EXPECT,  "5NN",                                             SLOT_RST,      0 },
    /* 3 */ { STEP_BOT_TX,  "r ur 5nn 5nn[S2SREF] tu 73 e e K",                 SLOT_NONE,     0 },
    /* 4 */ { STEP_END,     nullptr,                                            SLOT_NONE,     0 },
    // bot-initiated: bot = activator, calls CQ and sends its ref.
    /* 5 */ { STEP_BOT_TX,  "cq [ACT] cq [ACT] de [BOTCALL] [BOTCALL] k",       SLOT_NONE,     0 },
    /* 6 */ { STEP_EXPECT,  "[USERCALL]",                                       SLOT_CALLSIGN, 4 },
    /* 7 */ { STEP_BOT_TX,  "[USERCALL] de [BOTCALL] ur 5nn 5nn bk",            SLOT_NONE,     0 },
    /* 8 */ { STEP_EXPECT,  "5NN",                                             SLOT_RST,      0 },
    /* 9 */ { STEP_BOT_TX,  "r qth [BOTREF] [BOTREF] bk",                       SLOT_NONE,     0 },
    /* 10*/ { STEP_EXPECT_OPT, "r",                                             SLOT_PROSIGN,  0 },
    /* 11*/ { STEP_BOT_TX,  "tu 73 e e de [BOTCALL] K",                         SLOT_NONE,     0 },
    /* 12*/ { STEP_END,     nullptr,                                            SLOT_NONE,     0 },
};

static const QsoDescriptor kSotaPota = {
    "SOTA/POTA",
    kSotaPotaSteps,
    sizeof(kSotaPotaSteps) / sizeof(QsoStep),
    /*features*/ 0,
    /*defaultWpm*/ 18,
};

// ===========================================================================
// Contest descriptor (PR-4)
// ===========================================================================
//
// A contest is a continuous session of very short QSOs. The opening
// decides the (sticky) running role; thereafter whoever runs keeps
// calling CQ, the bot takes a fresh identity each QSO, and the session
// ends on ~15 s of inactivity. The exchange is RST (always 5nn) plus the
// CQ zone (CQ WW) or a serial (WPX/Sprint), per the Contest Type pref.
//
//   WAIT_USER_CQ arg=5 : user CQ -> pc1 (user runs); silence -> pc5 (bot runs)
//   WAIT_CQ_LOOP arg=1 : next user CQ -> new bot identity + pc1; idle -> end
//   LOOP        arg=5  : new bot identity + pc5 (bot calls CQ again)
static const QsoStep kContestSteps[] = {
    /* 0 */ { STEP_WAIT_USER_CQ, nullptr,                               SLOT_NONE,     5 },
    // --- user runs, bot is search & pounce ---
    /* 1 */ { STEP_BOT_TX,  "[BOTCALL]",                                SLOT_NONE,     0 },
    /* 2 */ { STEP_EXPECT,  "",                                         SLOT_EXCHANGE, 3 },
    /* 3 */ { STEP_BOT_TX,  "5nn [BOTEXCH]",                            SLOT_NONE,     0 },
    /* 4 */ { STEP_WAIT_CQ_LOOP, nullptr,                              SLOT_NONE,     1 },
    // --- bot runs (calls CQ), user is search & pounce ---
    /* 5 */ { STEP_BOT_TX,  "cq test de [BOTCALL] [BOTCALL] test",      SLOT_NONE,     0 },
    /* 6 */ { STEP_EXPECT,  "",                                         SLOT_CALLSIGN, 4 },
    /* 7 */ { STEP_BOT_TX,  "[USERCALL] 5nn [BOTEXCH]",                 SLOT_NONE,     0 },
    /* 8 */ { STEP_EXPECT,  "",                                         SLOT_EXCHANGE, 3 },
    /* 9 */ { STEP_BOT_TX,  "tu",                                       SLOT_NONE,     0 },
    /* 10*/ { STEP_LOOP,    nullptr,                                    SLOT_NONE,     5 },
};

static const QsoDescriptor kContest = {
    "Contest",
    kContestSteps,
    sizeof(kContestSteps) / sizeof(QsoStep),
    /*features*/ 0,
    /*defaultWpm*/ 24,
};

// ===========================================================================
// Standard ("rubber-stamp") QSO descriptor (PR-4)
// ===========================================================================
//
// Three layers: (1) RST + name + QTH, (2) rig + ant + wx + age, (3) 73 /
// <sk> sign-off. The user's info overs are SLOT_INFO (keyword-delimited
// field parser): RST is required in layer 1, NAME/QTH are captured and
// the bot echoes the name ("fb dr willi"). The opening decides who runs:
// if the user calls CQ they send layer 1 first; if the bot runs it calls
// CQ and sends layer 1 first.
//
//   WAIT_USER_CQ arg=9 : user CQ -> pc1 (user runs); silence -> pc9 (bot runs)
static const QsoStep kStandardSteps[] = {
    /* 0 */ { STEP_WAIT_USER_CQ, nullptr,                                    SLOT_NONE,    9 },
    // --- user runs (called CQ): the user sends layer 1 first ---
    /* 1 */ { STEP_BOT_TX,  "[USERCALL] de [BOTCALL] [BOTCALL] k",           SLOT_NONE,    0 },
    /* 2 */ { STEP_EXPECT,  "",                                              SLOT_INFO,    3 },
    /* 3 */ { STEP_BOT_TX,  "[USERCALL] de [BOTCALL] = fb dr [USERNAME] = ur rst 599 599 = name [BOTNAME] [BOTNAME] = qth [BOTQTH] [BOTQTH] = hw? k", SLOT_NONE, 0 },
    /* 4 */ { STEP_EXPECT_OPT, "",                                           SLOT_INFO,    0 },
    /* 5 */ { STEP_BOT_TX,  "= all copy fb = rig [BOTRIG] = ant [BOTANT] = wx [BOTWX] = age [BOTAGE] = hw? bk", SLOT_NONE, 0 },
    /* 6 */ { STEP_EXPECT_OPT, "73",                                         SLOT_PROSIGN, 0 },
    /* 7 */ { STEP_BOT_TX,  "tnx fb qso dr [USERNAME] = 73 73 = [USERCALL] de [BOTCALL] K", SLOT_NONE, 0 },
    /* 8 */ { STEP_END,     nullptr,                                         SLOT_NONE,    0 },
    // --- bot runs (user silent): the bot calls CQ and sends layer 1 first ---
    /* 9 */ { STEP_BOT_TX,  "cq cq de [BOTCALL] [BOTCALL] k",                SLOT_NONE,    0 },
    /* 10*/ { STEP_EXPECT,  "",                                              SLOT_CALLSIGN,4 },
    /* 11*/ { STEP_BOT_TX,  "[USERCALL] de [BOTCALL] = gm dr om = ur rst 599 599 = name [BOTNAME] [BOTNAME] = qth [BOTQTH] [BOTQTH] = hw? k", SLOT_NONE, 0 },
    /* 12*/ { STEP_EXPECT,  "",                                              SLOT_INFO,    3 },
    /* 13*/ { STEP_BOT_TX,  "[USERCALL] de [BOTCALL] = fb dr [USERNAME] = rig [BOTRIG] = ant [BOTANT] = wx [BOTWX] = age [BOTAGE] = hw? bk", SLOT_NONE, 0 },
    /* 14*/ { STEP_EXPECT_OPT, "",                                           SLOT_INFO,    0 },
    /* 15*/ { STEP_BOT_TX,  "tnx fb qso dr [USERNAME] = 73 73 = [USERCALL] de [BOTCALL] K", SLOT_NONE, 0 },
    /* 16*/ { STEP_EXPECT_OPT, "73",                                         SLOT_PROSIGN, 0 },
    /* 17*/ { STEP_END,     nullptr,                                         SLOT_NONE,    0 },
};

static const QsoDescriptor kStandard = {
    "Standard",
    kStandardSteps,
    sizeof(kStandardSteps) / sizeof(QsoStep),
    /*features*/ 0,
    /*defaultWpm*/ 18,
};

static const QsoDescriptor* selectDescriptor(menuNo mode) {
    if (mode == _qsoContest) {
        gContestType = MorsePreferences::pliste[posQsoBotContestType].value;
        gSessionMode = true;
        return &kContest;
    }
    gSessionMode = false;
    if (mode == _qsoStandard) return &kStandard;
    return &kSotaPota;                  // SOTA/POTA
}

// ===========================================================================
// Main entry
// ===========================================================================

void run(menuNo mode) {
    gDesc = selectDescriptor(mode);
    if (!gDesc || gDesc->stepCount == 0) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(1, REGULAR, 0, "(no descriptor)");
        delay(1500);
        return;
    }

    // ---- Standard display setup (same as iCW/Ext Trx) ----
    speedChanged = true;
    delay(650);
    MorseMenu::cleanupScreen();
    MorseOutput::drawInputStatus(false);
    encoderState = speedSettingMode;
    displayCWspeed();
    MorseOutput::displayVolume(encoderState == speedSettingMode,
                               MorsePreferences::sidetoneVolume);
    keyDecoder.setup();

    // ---- Bot state init ----
    gLevel = MorsePreferences::pliste[posQsoBotLevel].value;   // QSO Difficulty
    initActors();
    inputReset();
    gPc             = 0;
    gRecoveryActive = false;
    gCachedBotText  = "";
    gDisplayPos     = 0;
    gConcatBuf      = "";
    gEeeeDetected   = false;
    gRstReceived    = false;
    gQsoWarm        = false;
    gUserInformal   = false;
    gLastBotTx      = "";
    gRepeatSlot     = "";
    resetOverAccumulators();

    qsoBotMode      = true;
    gameCharBuffer  = 0;
    clearPaddleLatches();

    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks  = 0;

    enterStep();                       // kick off step 0 (the WAIT opening)

    // ---- Main loop ----
    bool userAborted = false;
    while (gState != RT_DONE) {

        // Encoder rotation.
        int t = checkEncoder();
        if (t != 0) {
            switch (encoderState) {
                case speedSettingMode:  changeSpeed(t);  break;
                case volumeSettingMode: changeVolume(t); break;
                case scrollMode:
                    if (t == 1 && MorseOutput::relPos < MorseOutput::maxPos) {
                        ++MorseOutput::relPos;
                        MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    } else if (t == -1 && MorseOutput::relPos > 0) {
                        --MorseOutput::relPos;
                        MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    }
                    MorseOutput::displayScrollBar(true);
                    break;
                default: break;
            }
        }
        if (speedChanged) { speedChanged = false; displayCWspeed(); }

        // Encoder (mode) button: long-press exits; double-click = prefs.
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) { userAborted = true; break; }
        if (Buttons::modeButton.clicks == 2) {
            MorseCwEngine::playStop();
            MorsePreferences::setupPreferences(MorsePreferences::menuPtr);
            MorseOutput::clearDisplay();
            updateTopLine();
            MorseOutput::refreshScrollArea(MorseOutput::relPos);
            Buttons::modeButton.clicks = 0;
            Buttons::volButton.clicks  = 0;
        }

        // FN (vol) button: short = speed/vol toggle, long = scroll, double = dim.
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {
            if (encoderState == scrollMode) {
                encoderState = speedSettingMode;
                MorseOutput::relPos = MorseOutput::maxPos;
                MorseOutput::refreshScrollArea(MorseOutput::relPos);
                MorseOutput::displayScrollBar(false);
            } else {
                encoderState = (encoderState == speedSettingMode)
                                ? volumeSettingMode : speedSettingMode;
            }
            displayCWspeed();
            MorseOutput::displayVolume(encoderState == speedSettingMode,
                                       MorsePreferences::sidetoneVolume);
            Buttons::volButton.clicks = 0;
        }
        if (Buttons::volButton.clicks == -1) {
            if (encoderState == scrollMode) {
                encoderState = speedSettingMode;
                MorseOutput::relPos = MorseOutput::maxPos;
                MorseOutput::refreshScrollArea(MorseOutput::relPos);
                MorseOutput::displayScrollBar(false);
            } else {
                encoderState = scrollMode;
                MorseOutput::displayScrollBar(true);
            }
            Buttons::volButton.clicks = 0;
        }
        if (Buttons::volButton.clicks == 2) {
            MorseOutput::decreaseBrightness();
            Buttons::volButton.clicks = 0;
        }

        // Keyer scan + paddle iambic.
        switch (keyerState) {
            case DIT: case DAH: case KEY_START: break;
            default: checkPaddles(); break;
        }
        if (doPaddleIambic(leftKey, rightKey)) continue;

        // Drain one decoded char into the matcher accumulator.
        const char c = gameCharBuffer;
        if (c != 0) { gameCharBuffer = 0; inputFeed(c); }

        // ---- State machine ----
        if (gState == RT_OPENING) {
            if (inputSubmitReady()) {
                String token(inputPeek());
                String upper(token); upper.toUpperCase();
                inputReset();
                if (token.length()) {
                    gUserStartedCalling = true;
                    gOpeningActivity    = millis();
                    if (looksLikeCallsign(token.c_str())) {
                        String bot(gActors.botCall); bot.toUpperCase();
                        if (upper != bot) gActors.userCall = token;
                        gOpeningCallSeen = true;
                    }
                    // A clear end-of-over marker ends the user's CQ at once,
                    // but only once we've seen their call — otherwise a
                    // split-off leading "k" would finish the opening early.
                    if (isEndOfOver(upper) && gOpeningCallSeen) finishOpening();
                }
            }
            if (gState == RT_OPENING) {
                if (gUserStartedCalling) {
                    if (!inputMidToken() &&
                        millis() - gOpeningActivity > kCallEndSilenceMs)
                        finishOpening();
                } else {
                    const unsigned long to = gSessionMode ? kSessionIdleMs
                                                          : kOpeningWaitMs;
                    if (millis() - gOpeningStart > to) {
                        if (gWaitTimeoutEndsSession) {
                            renderInfo("session end");
                            gState = RT_DONE;
                        } else {
                            gPc = gWaitTimeoutTarget;     // -> bot-init branch
                            enterStep();
                        }
                    }
                }
            }
        }
        else if (gState == RT_BOT_TX) {
            MorseCwEngine::playTick();
            if (!MorseCwEngine::isPlaying()) {
                if (gRecoveryActive) { gRecoveryActive = false; reEnterExpect(); }
                else                 { advance(); }
            }
        }
        else if (gState == RT_EXPECT) {
            const QsoStep& step = gDesc->steps[gPc];
            const SlotGrammar& g = grammarFor(step.slot);
            const String expected(step.tmpl ? step.tmpl : "");

            // Consume one completed word; accumulate, but DEFER acting on
            // any match until the user signals end-of-over.
            if (inputSubmitReady()) {
                String token(inputPeek());
                String upper(token); upper.toUpperCase();
                inputReset();
                if (token.length() > 0) {
                    gOverStarted  = true;
                    gOverActivity = millis();

                    bool consumed = false;
                    if (upper == "QRS" || upper == "QRQ") {
                        // Speed request: nudge the bot's fist; the change is
                        // heard on the bot's next over. Consumed so it isn't
                        // matched as a value, but it does NOT end the over —
                        // other tokens in the same over still count.
                        adjustBotSpeed(upper == "QRS" ? -1 : +1);
                        consumed = true;
                    } else if (isRepeatTrigger(upper)) {
                        gOverRepeat = true; gOverRepeatSlot = ""; consumed = true;
                    } else if (gOverRepeat && gOverRepeatSlot.length() == 0 &&
                               isSlotKeyword(token)) {
                        gOverRepeatSlot = token; consumed = true;
                    }

                    if (!consumed) {
                        if (looksLikeCallsign(token.c_str())) {
                            String bot(gActors.botCall); bot.toUpperCase();
                            if (upper != bot) gActors.userCall = token;
                        }
                        if (step.slot == SLOT_INFO) {
                            // Keyword-field parser collects the whole over;
                            // gOverMatched tracks "RST seen" for processOver.
                            gInfo.feed(token);
                            if (gInfo.rst) gOverMatched = true;
                        } else if (step.slot == SLOT_EXCHANGE) {
                            // Numeric exchange: take the single token, but
                            // also accumulate consecutive digit tokens so a
                            // split "1" "4" reads as "14" (longest match
                            // wins). Non-numeric junk (the echoed callsign,
                            // 5nn, ...) is not concatenated.
                            if (matchSlot(SLOT_EXCHANGE, token.c_str(), expected.c_str())) {
                                gOverMatched = true; gOverMatchedValue = token;
                            }
                            String norm = normalizeCutNumbers(token);
                            if (allDigits(norm)) {
                                gConcatBuf += norm;
                                if (gConcatBuf.length() > 8)
                                    gConcatBuf = gConcatBuf.substring(gConcatBuf.length() - 8);
                                if (matchSlot(SLOT_EXCHANGE, gConcatBuf.c_str(), expected.c_str())) {
                                    gOverMatched = true; gOverMatchedValue = gConcatBuf;
                                }
                            }
                        } else {
                            // Simple slots (RST / CALLSIGN / REF / PROSIGN). A
                            // token that matches on its own is a complete value
                            // and OVERRIDES any earlier match in this over, so a
                            // correction like "599 = 579" or a re-sent callsign
                            // takes the later value (real CW practice). It also
                            // clears the concat buffer so the correction starts
                            // clean. Split tokens are still glued via concat, but
                            // only until the first match — afterwards a partial
                            // can't corrupt the captured value. (SLOT_EXCHANGE is
                            // handled above and deliberately keeps accumulating,
                            // since a lone digit is itself a valid zone.)
                            if (matchSlot(step.slot, token.c_str(), expected.c_str())) {
                                gOverMatched = true; gOverMatchedValue = token;
                                gConcatBuf = "";
                            } else if (isNoise(token, g, expected)) {
                                // drop, keep listening
                            } else if (!gOverMatched && g.allowConcat) {
                                gConcatBuf += token;
                                if (gConcatBuf.length() > 16)
                                    gConcatBuf = gConcatBuf.substring(gConcatBuf.length() - 16);
                                if (matchSlot(step.slot, gConcatBuf.c_str(), expected.c_str())) {
                                    gOverMatched = true; gOverMatchedValue = gConcatBuf;
                                }
                            }
                        }
                    }

                    // Honour an end-of-over marker only once the slot's
                    // value has actually been captured. A bare "k" before
                    // any match is almost always the split-off first letter
                    // of a callsign (e.g. "K2XYZ"), not the user signing
                    // the over — acting on it here would fire a premature
                    // "qrz?". If the marker really is the end of the over,
                    // the value matched earlier in the same over; if the
                    // user truly sent nothing matchable, the silence
                    // timeout (or no-reply timeout) handles it.
                    if (isEndOfOver(upper) && gOverMatched) processOver(step);
                }
            }

            // eeee / <err> retract.
            if (gState == RT_EXPECT && gEeeeDetected) {
                gEeeeDetected = false;
                if (step.slot == SLOT_INFO) {
                    // Drop only the last word of the field being keyed, so a
                    // long multi-field over isn't wiped to correct one word.
                    String* f = (gInfo.cur == F_QTH)  ? &gInfo.qth
                              : (gInfo.cur == F_NAME) ? &gInfo.name : nullptr;
                    if (f) {
                        int sp = f->lastIndexOf(' ');
                        *f = (sp < 0) ? String("") : f->substring(0, sp);
                    } else if (gInfo.cur == F_RST) {
                        gInfo.rst = false;          // retract a just-sent RST
                    }
                } else {
                    gOverMatched = false; gOverMatchedValue = "";
                    gOverRepeat  = false; gOverRepeatSlot = "";
                    gConcatBuf   = "";
                }
                gOverActivity = millis();
            }

            // End-of-over by silence (user keyed, no explicit marker).
            if (gState == RT_EXPECT && gOverStarted && !inputMidToken() &&
                millis() - gOverActivity > overEndSilenceMs()) {
                processOver(step);
            }

            // No reply at all from the user.
            if (gState == RT_EXPECT && !gOverStarted && !inputMidToken()) {
                // In a contest session, nobody answering the bot's CQ
                // (the callsign EXPECT) means the run is over — end the
                // session after the idle window rather than nagging "agn".
                const bool sessionGate = gSessionMode && step.slot == SLOT_CALLSIGN;
                unsigned long to;
                if (step.kind == STEP_EXPECT_OPT) to = kOptNoReplyMs;
                else if (sessionGate)             to = kSessionIdleMs;
                else                              to = noReplyMs();
                if (millis() - gStateStart > to) {
                    if (step.kind == STEP_EXPECT_OPT) advance();
                    else if (sessionGate) { renderInfo("session end"); gState = RT_DONE; }
                    else if (gRetriesLeft > 0) {
                        gRetriesLeft--;
                        enterRecovery(recoveryPrompt(step.slot));
                    } else { renderInfo("Giving up"); gState = RT_DONE; }
                }
            }
        }
        else if (gState == RT_PAUSE) {
            if (millis() - gStateStart >= gDesc->steps[gPc].arg) advance();
        }

        serialEvent();
        delay(2);
    }

    // ---- Teardown ----
    MorseCwEngine::playStop();
    qsoBotMode     = false;
    gameCharBuffer = 0;
    inputReset();
    clearPaddleLatches();

    if (userAborted) renderInfo("Aborted");
    MorseOutput::printToScroll(REGULAR, "(click to exit)\n",
                               true, encoderState == scrollMode);

    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks  = 0;
    delay(120);
    while (true) {
        Buttons::modeButton.Update();
        Buttons::volButton.Update();
        if (Buttons::modeButton.clicks != 0 || Buttons::volButton.clicks != 0) break;
        delay(20);
    }
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks  = 0;
}

}  // namespace MorseQsoBot

#endif  // CONFIG_QSO_BOT

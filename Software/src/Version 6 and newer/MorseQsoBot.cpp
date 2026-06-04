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

namespace {

using namespace MorseQsoBot;

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
    String userCall;
};

QsoActors gActors;
uint8_t   gActivity         = 0;       // 0 = SOTA, 1 = POTA (bot-as-activator)
bool      gBotAlsoActivator = false;   // S2S/P2P: bot answers AND sends a ref

void initActors() {
    String call = getRandomCall(0);
    const uint8_t cont = lastGeneratedCallContinent;   // side effect of getRandomCall
    call.toUpperCase();
    gActors.botCall = call;

    gActivity = random(2);             // bot randomly does SOTA or POTA
    gActors.botRef = String(gActivity == 0 ? QsoContent::pickSotaRef(cont)
                                           : QsoContent::pickPotaRef(cont));
    gActors.botName = String(QsoContent::kNames[random(QsoContent::kNamesCount)]);
    gBotAlsoActivator = (random(100) < 15);   // ~15% of answered CQs are S2S/P2P

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
    String call(gActors.botCall);  call.toLowerCase();
    String ref(gActors.botRef);    ref.toLowerCase();
    String name(gActors.botName);  name.toLowerCase();
    String user(gActors.userCall); user.toLowerCase();
    out.replace("[BOTCALL]",  call);
    out.replace("[BOTREF]",   ref);
    out.replace("[BOTNAME]",  name);
    out.replace("[USERCALL]", user);
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
unsigned long        gOpeningStart       = 0;
unsigned long        gOpeningActivity    = 0;
constexpr unsigned long kOpeningWaitMs    = 5000;
constexpr unsigned long kCallEndSilenceMs = 1800;

// Per-over accumulators (EXPECT). The bot collects the user's whole over
// and only acts at end-of-over (explicit marker or silence), so it never
// cuts in while the user is still keying.
bool          gOverStarted     = false;   // user keyed something this over
bool          gOverMatched     = false;   // a word/concat filled the slot
String        gOverMatchedValue;
bool          gOverRepeat      = false;    // "agn"/"rpt" seen this over
String        gOverRepeatSlot;             // optional slot after "rpt"
unsigned long gOverActivity    = 0;        // last keying time this over
constexpr unsigned long kOverEndSilenceMs = 4000;   // user finished (no marker)
constexpr unsigned long kNoReplyMs        = 12000;  // user never replied
constexpr unsigned long kOptNoReplyMs     = 4000;   // optional ack, no reply

// Last real bot over (expanded), for full-repeat on "agn"/"rpt".
String        gLastBotTx;
String        gRepeatSlot;                 // used by executePendingRepeat

// RST received from the user (validity note at end of QSO).
bool          gRstReceived = false;

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

bool looksLikeCallsign(const char* tok) {
    int len = strlen(tok);
    if (len < 3 || len > 10) return false;
    int digitPos = -1;
    for (int i = 0; i < len; i++)
        if (tok[i] >= '0' && tok[i] <= '9') { digitPos = i; break; }
    if (digitPos < 1 || digitPos > 2) return false;
    for (int i = 0; i < digitPos; i++)
        if (tok[i] < 'A' || tok[i] > 'Z') return false;
    int i = digitPos + 1, suffixLetters = 0;
    while (i < len && tok[i] >= 'A' && tok[i] <= 'Z') { suffixLetters++; i++; }
    if (suffixLetters < 1 || suffixLetters > 4) return false;
    if (i < len) {
        if (tok[i++] != '/') return false;
        while (i < len) {
            char c = tok[i++];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
        }
    }
    return true;
}

bool matchCallsign(const char* tok) {
    if (!looksLikeCallsign(tok)) return false;
    String t(tok);
    String bot(gActors.botCall); bot.toUpperCase();
    return t != bot;
}

// Cut-number normalisation for the three common cuts: T->0, A->1, N->9.
String normalizeCutNumbers(const String& tok) {
    String out; out.reserve(tok.length());
    for (unsigned i = 0; i < tok.length(); i++) {
        char c = tok[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        switch (c) {
            case 'T': out += '0'; break;
            case 'A': out += '1'; break;
            case 'N': out += '9'; break;
            default:  out += c;   break;
        }
    }
    return out;
}

bool matchRST(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    if (n.length() != 3) return false;
    if (n[0] < '3' || n[0] > '5') return false;
    return isdigit((unsigned char) n[1]) && isdigit((unsigned char) n[2]);
}

bool matchRef(const char* tok) {
    String t(tok);            t.toUpperCase();
    String r(gActors.botRef); r.toUpperCase();
    return t == r;
}

bool matchProsignTok(const char* tok, const char* expected) {
    if (!expected || !*expected) return false;
    String a(tok);      a.toUpperCase();
    String b(expected); b.toUpperCase();
    return a == b;
}

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

const char* const kCommonNoise[]   = { "de", "dr", "pse", "qsl", "tnx", "tu",
                                       "ok", "fb", "es", "qrl", "om", "oc",
                                       "dx", "hr", "gm", "ga", "ge", "gd",
                                       "cfm", "cpy", "ant", "rig", nullptr };
const char* const kCallsignNoise[] = { "bk", "k", "qrz", "qrz?", "cq", "sota",
                                       "pota", nullptr };
const char* const kRstNoise[]      = { "r", "rr", "ur", "qsa", "qrk", "bk",
                                       "rst", nullptr };
const char* const kRefNoise[]      = { "qth", "loc", "ref", "r", "rr", "bk",
                                       nullptr };
const char* const kProsignNoise[]  = { nullptr };

const SlotGrammar& grammarFor(QsoSlotKind slot) {
    static const SlotGrammar kGrammars[] = {
        /* SLOT_NONE     */ { nullptr,        kProsignNoise,  false },
        /* SLOT_CALLSIGN */ { acceptCallsign, kCallsignNoise, true  },
        /* SLOT_RST      */ { acceptRST,      kRstNoise,      true  },
        /* SLOT_REF      */ { acceptRef,      kRefNoise,      true  },
        /* SLOT_PROSIGN  */ { acceptProsign,  kProsignNoise,  false },
    };
    return kGrammars[slot];
}

bool tokenInList(const String& upper, const char* const* list) {
    if (!list) return false;
    for (int i = 0; list[i]; i++) {
        String n(list[i]); n.toUpperCase();
        if (upper == n) return true;
    }
    return false;
}

bool isNoise(const String& tok, const SlotGrammar& g, const String& expected) {
    String u(tok); u.toUpperCase();
    if (expected.length()) {
        String e(expected); e.toUpperCase();
        if (u == e) return false;
    }
    return tokenInList(u, kCommonNoise) || tokenInList(u, g.extraNoise);
}

bool matchSlot(QsoSlotKind kind, const char* tok, const char* expected) {
    const SlotGrammar& g = grammarFor(kind);
    if (!g.accept) return false;
    return g.accept(String(tok), String(expected ? expected : ""));
}

// End-of-over marker? Conservative set to avoid colliding with cut
// numbers (a lone "N"/"B" can be a cut 9/6 in a split RST). The silence
// fallback (kOverEndSilenceMs) catches everything else.
bool isEndOfOver(const String& upper) {
    return upper == "K" || upper == "+" || upper == "AR" ||
           upper == "73" || upper == "72" || upper == "BK" || upper == "KK";
}

bool isSlotKeyword(const String& tok) {
    String k(tok); k.toLowerCase();
    return k == "rst" || k == "call" || k == "cl" || k == "ur" ||
           k == "qth" || k == "loc" || k == "ref" || k == "sota" ||
           k == "pota" || k == "summit" || k == "name" || k == "op" || k == "nm";
}

bool isRepeatTrigger(const String& upper) {
    return upper == "AGN" || upper == "RPT";
}

// ---- State transitions -----------------------------------------------

void startBotTx(const String& text);
void startExpect(uint16_t budget);
void reEnterExpect();
void advance();
void enterStep();
void startOpening();

void startBotTx(const String& text) {
    gCachedBotText = text;
    gDisplayPos    = 0;
    emitNextBotChar();
    MorseCwEngine::PlayOpts opts = {
        /*pitchHz        */ (uint16_t) MorseOutput::notes[
                                MorsePreferences::pliste[posPitch].value],
        /*wpm            */ 0,
        /*loop           */ false,
        /*resumeGapDits  */ 2,
        /*extraMute      */ nullptr,
        /*onCharComplete */ onBotCharComplete,
    };
    MorseCwEngine::playStart(text, opts);
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
}

void startExpect(uint16_t budget) {
    inputReset();
    resetOverAccumulators();
    if (budget == 0) {
        const QsoStep& step = gDesc->steps[gPc];
        gRetriesLeft = (step.slot == SLOT_CALLSIGN) ? 4 : 2;
    } else {
        gRetriesLeft = (uint8_t) budget;
    }
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
            startOpening();
            break;
        case STEP_PAUSE_MS:
            gState = RT_PAUSE; gStateStart = millis();
            break;
    }
}

void advance() { gPc++; enterStep(); }

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
    if (gOverMatched) {
        if (step.slot == SLOT_CALLSIGN) gActors.userCall = gOverMatchedValue;
        if (step.slot == SLOT_RST)      gRstReceived = true;
        advance();
        return;
    }
    if (step.kind == STEP_EXPECT_OPT) { advance(); return; }
    if (gRetriesLeft > 0) {
        gRetriesLeft--;
        enterRecovery(step.slot == SLOT_CALLSIGN ? "qrz?" : "agn agn");
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

static const QsoDescriptor* selectDescriptor(menuNo /*mode*/) {
    // PR-3.5 ships the single dynamic SOTA/POTA QSO. All current menu
    // entries (SOTA, POTA, Standard, Contest) route here for now; the
    // dedicated Standard / Contest descriptors land in PR-4, and the
    // menu will be tidied to a single SOTA/POTA entry then.
    return &kSotaPota;
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
    initActors();
    inputReset();
    gPc             = 0;
    gRecoveryActive = false;
    gCachedBotText  = "";
    gDisplayPos     = 0;
    gConcatBuf      = "";
    gEeeeDetected   = false;
    gRstReceived    = false;
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
                    }
                    // a clear end-of-over marker ends the user's CQ at once
                    if (isEndOfOver(upper)) advance();   // -> user-init branch
                }
            }
            if (gState == RT_OPENING) {
                if (gUserStartedCalling) {
                    if (!inputMidToken() &&
                        millis() - gOpeningActivity > kCallEndSilenceMs)
                        advance();                       // -> user-init branch
                } else if (millis() - gOpeningStart > kOpeningWaitMs) {
                    gPc = gDesc->steps[gPc].arg;         // -> bot-init branch
                    enterStep();
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
                    if (isRepeatTrigger(upper)) {
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
                        if (!gOverMatched) {
                            if (matchSlot(step.slot, token.c_str(), expected.c_str())) {
                                gOverMatched = true; gOverMatchedValue = token;
                            } else if (isNoise(token, g, expected)) {
                                // drop, keep listening
                            } else if (g.allowConcat) {
                                gConcatBuf += token;
                                if (gConcatBuf.length() > 16)
                                    gConcatBuf = gConcatBuf.substring(gConcatBuf.length() - 16);
                                if (matchSlot(step.slot, gConcatBuf.c_str(), expected.c_str())) {
                                    gOverMatched = true; gOverMatchedValue = gConcatBuf;
                                }
                            }
                        }
                    }

                    if (isEndOfOver(upper)) processOver(step);
                }
            }

            // eeee retract: drop this over's accumulated match.
            if (gState == RT_EXPECT && gEeeeDetected) {
                gEeeeDetected = false;
                gOverMatched = false; gOverMatchedValue = "";
                gOverRepeat  = false; gOverRepeatSlot = "";
                gConcatBuf   = "";
                gOverActivity = millis();
            }

            // End-of-over by silence (user keyed, no explicit marker).
            if (gState == RT_EXPECT && gOverStarted && !inputMidToken() &&
                millis() - gOverActivity > kOverEndSilenceMs) {
                processOver(step);
            }

            // No reply at all from the user.
            if (gState == RT_EXPECT && !gOverStarted && !inputMidToken()) {
                const unsigned long to =
                    (step.kind == STEP_EXPECT_OPT) ? kOptNoReplyMs : kNoReplyMs;
                if (millis() - gStateStart > to) {
                    if (step.kind == STEP_EXPECT_OPT) advance();
                    else if (gRetriesLeft > 0) {
                        gRetriesLeft--;
                        enterRecovery(step.slot == SLOT_CALLSIGN ? "qrz?" : "agn agn");
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

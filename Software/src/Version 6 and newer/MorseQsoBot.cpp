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
extern uint8_t      lastGeneratedCallCqZone;       // set by getRandomCall

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
uint8_t   gBotZone          = 14;      // bot's CQ zone (CQ WW exchange)
uint16_t  gBotSerial        = 1;       // bot's serial (WPX/Sprint exchange)

// (Re)pick the bot's per-QSO identity: a fresh callsign and everything
// derived from it (continent-matched ref, CQ zone), a name, the S2S
// chance, and — for contests — a fresh random serial (each CQ is a
// different station). Called at session start and at every contest QSO
// loop so the user works/copies a new station each time.
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
    gActors.botName  = String(QsoContent::kNames[random(QsoContent::kNamesCount)]);
    gActors.botQth   = String(QsoContent::kQths[random(QsoContent::kQthsCount)]);
    gActors.botRig   = String(QsoContent::kRigs[random(QsoContent::kRigsCount)]);
    gActors.botAnt   = String(QsoContent::kAnts[random(QsoContent::kAntsCount)]);
    gActors.botWx    = String(QsoContent::kWxs[random(QsoContent::kWxsCount)]);
    gActors.botAge   = String(QsoContent::kAges[random(QsoContent::kAgesCount)]);
    gBotAlsoActivator = (random(100) < 15);
    gBotSerial       = random(1, 600);                 // random per station
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
constexpr unsigned long kOverEndSilenceMs = 2500;   // user finished (no marker)
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

bool allDigits(const String& s) {
    if (s.length() == 0) return false;
    for (unsigned i = 0; i < s.length(); i++)
        if (!isdigit((unsigned char) s[i])) return false;
    return true;
}

// CQ WW zone: 1-2 digits, value 1..40 (cut numbers allowed).
bool matchZone(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    if (!allDigits(n) || n.length() < 1 || n.length() > 2) return false;
    int v = n.toInt();
    return v >= 1 && v <= 40;
}

// WPX/Sprint serial: 1-4 digits (cut numbers allowed). The bot can't
// validate the user's own serial, so any plausible number is accepted.
bool matchSerial(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    return allDigits(n) && n.length() >= 1 && n.length() <= 4;
}

bool matchExchange(const char* tok) {
    return (gContestType == 0) ? matchZone(tok) : matchSerial(tok);
}

// ---- Standard-QSO keyword-field parser (SLOT_INFO) --------------------
//
// A Standard over carries several keyword-delimited fields with no fixed
// separator: "name willi willi qth vienna = rig ic7300 ant efhw". The
// parser tracks the "current field" (switched by a keyword), captures the
// value tokens that follow (NAME/QTH only; deduping the conventional
// doubling), notes whether RST and the layer-2 fields were present, and
// treats =, es, bt, hw and prosigns as boundaries.

enum FieldCur : uint8_t { F_NONE, F_NAME, F_QTH, F_RST, F_OTHER };
FieldCur gFieldCur   = F_NONE;
bool     gFieldRst   = false;
bool     gFieldOther = false;      // rig/ant/wx/age seen (acknowledged generically)
String   gFieldName;
String   gFieldQth;

void appendDedup(String& field, const String& tok) {
    int sp = field.lastIndexOf(' ');
    String last = (sp < 0) ? field : field.substring(sp + 1);
    if (last.equalsIgnoreCase(tok)) return;            // dedupe "willi willi"
    if (field.length()) field += " ";
    field += tok;
}

void parseInfoToken(const String& token) {
    String low(token); low.toLowerCase();
    // field keywords
    if (low == "name" || low == "op"  || low == "nm")    { gFieldCur = F_NAME;  return; }
    if (low == "qth"  || low == "loc" || low == "qra")   { gFieldCur = F_QTH;   return; }
    if (low == "rig"  || low == "tcvr"|| low == "radio" || low == "trx")
                                                         { gFieldCur = F_OTHER; gFieldOther = true; return; }
    if (low == "ant"  || low == "antenna" || low == "aerial")
                                                         { gFieldCur = F_OTHER; gFieldOther = true; return; }
    if (low == "wx"   || low == "temp")                  { gFieldCur = F_OTHER; gFieldOther = true; return; }
    if (low == "age"  || low == "yrs")                   { gFieldCur = F_OTHER; gFieldOther = true; return; }
    if (low == "rst"  || low == "ur"  || low == "urs")   { gFieldCur = F_RST;   return; }
    // separators / boundaries
    if (low == "=" || low == "es" || low == "bt" || low == "hw" ||
        low == "hw?" || low == "pse" || low == "de")     { gFieldCur = F_NONE;  return; }
    // RST digits anywhere in the over
    if (matchRST(token.c_str()))                         { gFieldRst = true; gFieldCur = F_NONE; return; }
    // value token for the current field
    if      (gFieldCur == F_NAME) appendDedup(gFieldName, token);
    else if (gFieldCur == F_QTH)  appendDedup(gFieldQth,  token);
    // F_OTHER / F_NONE: noted via gFieldOther, not captured verbatim
}

void resetInfoFields() {
    gFieldCur   = F_NONE;
    gFieldRst   = false;
    gFieldOther = false;
    gFieldName  = "";
    gFieldQth   = "";
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
bool acceptExchange(const String& tok, const String&)   { return matchExchange(tok.c_str()); }

const char* const kCommonNoise[]   = { "de", "dr", "pse", "qsl", "tnx", "tu",
                                       "ok", "fb", "es", "qrl", "om", "oc",
                                       "dx", "hr", "gm", "ga", "ge", "gd",
                                       "cfm", "cpy", "ant", "rig", nullptr };
// Note: "k" is deliberately NOT listed here. It is an end-of-over marker
// (see isEndOfOver) but also a very common callsign first letter; if the
// decoder splits a call's leading "K" into its own token (common before
// its speed estimate has settled at the start of an over), treating "k"
// as noise would drop the prefix and lose the call. Leaving it out lets
// the concat buffer glue "K" + "2XYZ" back into "K2XYZ". A trailing "k"
// after the call has already matched is handled by isEndOfOver instead.
const char* const kCallsignNoise[] = { "bk", "qrz", "qrz?", "cq", "sota",
                                       "pota", nullptr };
const char* const kRstNoise[]      = { "r", "rr", "ur", "qsa", "qrk", "bk",
                                       "rst", nullptr };
const char* const kRefNoise[]      = { "qth", "loc", "ref", "r", "rr", "bk",
                                       nullptr };
const char* const kProsignNoise[]  = { nullptr };
const char* const kExchangeNoise[] = { "5nn", "599", "ur", "r", "rr", "nr",
                                       "bk", "tu", nullptr };

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
void pickBotIdentity();

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
    resetInfoFields();
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
        if (gFieldName.length()) gActors.userName = gFieldName;
        if (step.kind == STEP_EXPECT_OPT) { advance(); return; }   // layer 2: just ack
        // Layer 1 is required to carry the RST.
        if (gFieldRst) { gRstReceived = true; advance(); return; }
        if (gRetriesLeft > 0) { gRetriesLeft--; enterRecovery("pse ur rst?"); return; }
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
                        if (step.slot == SLOT_INFO) {
                            // Keyword-field parser collects the whole over;
                            // gOverMatched tracks "RST seen" for processOver.
                            parseInfoToken(token);
                            if (gFieldRst) gOverMatched = true;
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
                        } else if (!gOverMatched) {
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
                    String* f = (gFieldCur == F_QTH)  ? &gFieldQth
                              : (gFieldCur == F_NAME) ? &gFieldName : nullptr;
                    if (f) {
                        int sp = f->lastIndexOf(' ');
                        *f = (sp < 0) ? String("") : f->substring(0, sp);
                    } else if (gFieldCur == F_RST) {
                        gFieldRst = false;          // retract a just-sent RST
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
                millis() - gOverActivity > kOverEndSilenceMs) {
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
                else                              to = kNoReplyMs;
                if (millis() - gStateStart > to) {
                    if (step.kind == STEP_EXPECT_OPT) advance();
                    else if (sessionGate) { renderInfo("session end"); gState = RT_DONE; }
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

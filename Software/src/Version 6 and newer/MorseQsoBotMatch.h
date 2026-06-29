// MorseQsoBotMatch.h — pure token-classification primitives for the QSO Bot
// matcher, extracted from MorseQsoBot.cpp so they can be unit-tested off the
// device (see Software/tests/qso_bot_matcher/).
//
// Everything here is *pure*: it depends only on its arguments and the static
// noise tables below — no module globals, no Arduino runtime (no millis(),
// random(), display, ...). The context-dependent matchers in MorseQsoBot.cpp
// (matchCallsign / matchRef / matchExchange) are thin wrappers that read the
// per-QSO state (gActors, gContestType) and delegate to the two-argument
// forms here.
//
// String requirement: the includer must already provide an Arduino-compatible
// `String` type. In the firmware that comes from <Arduino.h>; in the host test
// it comes from arduino_string_shim.h. This header pulls in only the freestanding
// C headers it needs.

#ifndef MORSEQSOBOTMATCH_H_
#define MORSEQSOBOTMATCH_H_

#include <ctype.h>
#include <string.h>
#include <stdint.h>

namespace QsoMatch {

// ---- Callsign shape ----------------------------------------------------

// True if `tok` has the shape of a callsign: 3..10 chars, a digit in
// position 1 or 2, only letters before it, 1..4 suffix letters, optional
// "/PORTABLE" tail. (Shape only — does not compare against any station.)
inline bool looksLikeCallsign(const char* tok) {
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

// A token is a usable callsign for slot-matching when it looks like one and
// is not the bot's own call (case-insensitive).
inline bool matchCallsign(const char* tok, const String& botCall) {
    if (!looksLikeCallsign(tok)) return false;
    String t(tok);
    String bot(botCall); bot.toUpperCase();
    return t != bot;
}

// ---- Numbers / RST / contest exchange ---------------------------------

// Cut-number normalisation for the three common cuts: T->0, A->1, N->9.
inline String normalizeCutNumbers(const String& tok) {
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

// RST: three digits after cut-number normalisation, first digit 3..5.
inline bool matchRST(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    if (n.length() != 3) return false;
    if (n[0] < '3' || n[0] > '5') return false;
    return isdigit((unsigned char) n[1]) && isdigit((unsigned char) n[2]);
}

inline bool allDigits(const String& s) {
    if (s.length() == 0) return false;
    for (unsigned i = 0; i < s.length(); i++)
        if (!isdigit((unsigned char) s[i])) return false;
    return true;
}

// CQ WW zone: 1-2 digits, value 1..40 (cut numbers allowed).
inline bool matchZone(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    if (!allDigits(n) || n.length() < 1 || n.length() > 2) return false;
    int v = n.toInt();
    return v >= 1 && v <= 40;
}

// WPX/Sprint serial: 1-4 digits (cut numbers allowed). The bot can't
// validate the user's own serial, so any plausible number is accepted.
inline bool matchSerial(const char* tok) {
    String n = normalizeCutNumbers(String(tok));
    return allDigits(n) && n.length() >= 1 && n.length() <= 4;
}

// Contest exchange resolved by contest type: 0 = CQ WW (zone), else serial.
inline bool matchExchange(const char* tok, uint8_t contestType) {
    return (contestType == 0) ? matchZone(tok) : matchSerial(tok);
}

// ---- Literal matchers -------------------------------------------------

// SOTA/POTA ref: literal, case-insensitive, against the bot's own ref.
inline bool matchRef(const char* tok, const String& botRef) {
    String t(tok);    t.toUpperCase();
    String r(botRef); r.toUpperCase();
    return t == r;
}

inline bool matchProsignTok(const char* tok, const char* expected) {
    if (!expected || !*expected) return false;
    String a(tok);      a.toUpperCase();
    String b(expected); b.toUpperCase();
    return a == b;
}

// ---- Noise tables + classification ------------------------------------

static const char* const kCommonNoise[]   = { "de", "dr", "pse", "qsl", "tnx", "tu",
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
static const char* const kCallsignNoise[] = { "bk", "qrz", "qrz?", "cq", "sota",
                                              "pota", nullptr };
static const char* const kRstNoise[]      = { "r", "rr", "ur", "qsa", "qrk", "bk",
                                              "rst", nullptr };
static const char* const kRefNoise[]      = { "qth", "loc", "ref", "r", "rr", "bk",
                                              nullptr };
static const char* const kProsignNoise[]  = { nullptr };
static const char* const kExchangeNoise[] = { "5nn", "599", "ur", "r", "rr", "nr",
                                              "bk", "tu", nullptr };

inline bool tokenInList(const String& upper, const char* const* list) {
    if (!list) return false;
    for (int i = 0; list[i]; i++) {
        String n(list[i]); n.toUpperCase();
        if (upper == n) return true;
    }
    return false;
}

// Is `tok` a droppable noise word for a slot whose extra-noise list is
// `extraNoise`? The expected literal (if any) is never noise.
inline bool isNoiseToken(const String& tok, const char* const* extraNoise,
                         const String& expected) {
    String u(tok); u.toUpperCase();
    if (expected.length()) {
        String e(expected); e.toUpperCase();
        if (u == e) return false;
    }
    return tokenInList(u, kCommonNoise) || tokenInList(u, extraNoise);
}

// ---- Over-control tokens ----------------------------------------------

// End-of-over marker? Conservative set to avoid colliding with cut
// numbers (a lone "N"/"B" can be a cut 9/6 in a split RST). The silence
// fallback catches everything else. `upper` must already be upper-cased.
inline bool isEndOfOver(const String& upper) {
    return upper == "K" || upper == "+" || upper == "AR" ||
           upper == "73" || upper == "72" || upper == "BK" || upper == "KK";
}

inline bool isSlotKeyword(const String& tok) {
    String k(tok); k.toLowerCase();
    return k == "rst" || k == "call" || k == "cl" || k == "ur" ||
           k == "qth" || k == "loc" || k == "ref" || k == "sota" ||
           k == "pota" || k == "summit" || k == "name" || k == "op" || k == "nm";
}

// "agn"/"rpt" request a repeat; a bare "?" is the classic CW "say again"
// and means a full repeat of the bot's last over. `upper` is already upper-cased
// by the caller. (Multi-char tokens like "qrz?"/"hw?" are not bare "?".)
inline bool isRepeatTrigger(const String& upper) {
    return upper == "AGN" || upper == "RPT" || upper == "?";
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

struct InfoParser {
    FieldCur cur   = F_NONE;
    bool     rst   = false;
    bool     other = false;      // rig/ant/wx/age seen (acknowledged generically)
    String   name;
    String   qth;

    void reset() {
        cur = F_NONE; rst = false; other = false; name = ""; qth = "";
    }

    void feed(const String& token) {
        String low(token); low.toLowerCase();
        // field keywords
        if (low == "name" || low == "op"  || low == "nm")    { cur = F_NAME;  return; }
        if (low == "qth"  || low == "loc" || low == "qra")   { cur = F_QTH;   return; }
        if (low == "rig"  || low == "tcvr"|| low == "radio" || low == "trx")
                                                             { cur = F_OTHER; other = true; return; }
        if (low == "ant"  || low == "antenna" || low == "aerial")
                                                             { cur = F_OTHER; other = true; return; }
        if (low == "wx"   || low == "temp")                  { cur = F_OTHER; other = true; return; }
        if (low == "age"  || low == "yrs")                   { cur = F_OTHER; other = true; return; }
        if (low == "rst"  || low == "ur"  || low == "urs")   { cur = F_RST;   return; }
        // separators / boundaries
        if (low == "=" || low == "es" || low == "bt" || low == "hw" ||
            low == "hw?" || low == "pse" || low == "de")     { cur = F_NONE;  return; }
        // RST digits anywhere in the over
        if (matchRST(token.c_str()))                         { rst = true; cur = F_NONE; return; }
        // value token for the current field
        if      (cur == F_NAME) appendDedup(name, token);
        else if (cur == F_QTH)  appendDedup(qth,  token);
        // F_OTHER / F_NONE: noted via `other`, not captured verbatim
    }

  private:
    static void appendDedup(String& field, const String& tok) {
        int sp = field.lastIndexOf(' ');
        String last = (sp < 0) ? field : field.substring(sp + 1);
        if (last.equalsIgnoreCase(tok)) return;            // dedupe "willi willi"
        if (field.length()) field += " ";
        field += tok;
    }
};

}  // namespace QsoMatch

#endif  // MORSEQSOBOTMATCH_H_

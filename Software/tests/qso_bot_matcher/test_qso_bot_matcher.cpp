// test_qso_bot_matcher.cpp — host-side unit tests for the QSO Bot matcher
// primitives in MorseQsoBotMatch.h. Build & run with the Makefile in this
// directory (`make` / `make run`), or directly:
//
//   c++ -std=c++11 -Wall test_qso_bot_matcher.cpp -o run_tests && ./run_tests
//
// These exercise the SAME code the firmware compiles (the header is included
// by MorseQsoBot.cpp), so a green run here means the firmware's token
// classification is unchanged. Runtime-only behaviour (timeouts, the concat
// buffer reassembly, turn-taking) is integration-level and not covered here;
// what IS covered is every pure decision the matcher makes per token.

#include "arduino_string_shim.h"
#include "../../src/Version 6 and newer/MorseQsoBotMatch.h"

#include <cstdio>

using namespace QsoMatch;

static int g_checks = 0;
static int g_fails  = 0;

static void check(bool cond, const char* expr, const char* file, int line) {
    g_checks++;
    if (!cond) {
        g_fails++;
        std::printf("  FAIL  %s:%d  %s\n", file, line, expr);
    }
}
#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// ---------------------------------------------------------------------------

static void test_callsign_shape() {
    // Inputs reach the matcher already upper-cased (inputFeed folds case).
    CHECK(looksLikeCallsign("K2XYZ"));      // K-prefix call (regression: 28c03f3)
    CHECK(looksLikeCallsign("W1AW"));
    CHECK(looksLikeCallsign("OE1WKL"));
    CHECK(looksLikeCallsign("DL2ABC"));
    CHECK(looksLikeCallsign("OE1ABC/P"));   // portable suffix

    CHECK(!looksLikeCallsign("ABC"));       // no digit
    CHECK(!looksLikeCallsign("K2"));        // too short
    CHECK(!looksLikeCallsign("599"));       // RST, not a call
    CHECK(!looksLikeCallsign("12AB"));      // leading digit (pos 0)
    // KNOWN LIMITATION (documented by this test, not a bug to "fix" here):
    // the matcher requires the digit in position 1 or 2, so leading-digit
    // prefixes (2E0xxx, 3D2xxx, ...) are NOT recognised. Changing this is a
    // behaviour change, out of scope for the test-harness task.
    CHECK(!looksLikeCallsign("2E0ABC"));
    CHECK(!looksLikeCallsign("CQ"));        // noise word
    CHECK(!looksLikeCallsign("K"));         // bare K (split prefix) is not a call
}

static void test_match_callsign_vs_botcall() {
    CHECK(matchCallsign("K2XYZ", String("OE1ABC")));     // a different station
    CHECK(!matchCallsign("OE1ABC", String("OE1ABC")));   // the bot's own call
    CHECK(!matchCallsign("OE1ABC", String("oe1abc")));   // own call, case-insensitive
    CHECK(!matchCallsign("CQ", String("OE1ABC")));       // not a call at all
}

static void test_rst() {
    CHECK(matchRST("599"));
    CHECK(matchRST("5NN"));     // cut numbers N->9
    CHECK(matchRST("5nn"));     // lowercase tolerated
    CHECK(matchRST("5T9"));     // T->0 -> 509
    CHECK(matchRST("339"));     // first digit 3 ok
    CHECK(matchRST("4N9"));

    CHECK(!matchRST("59"));     // too short
    CHECK(!matchRST("5999"));   // too long
    CHECK(!matchRST("279"));    // first digit < 3
    CHECK(!matchRST("699"));    // first digit > 5
    CHECK(!matchRST("ABC"));    // not digits (A->1 but B,C remain)
}

static void test_zone_serial_exchange() {
    // CQ WW zone: 1..40, 1-2 digits.
    CHECK(matchZone("14"));
    CHECK(matchZone("1"));
    CHECK(matchZone("40"));
    CHECK(matchZone("1T"));     // cut: 10
    CHECK(!matchZone("0"));
    CHECK(!matchZone("41"));
    CHECK(!matchZone("100"));   // 3 digits

    // Serial: 1..4 digits.
    CHECK(matchSerial("1"));
    CHECK(matchSerial("599"));
    CHECK(matchSerial("0001"));
    CHECK(matchSerial("12A"));  // cut A->1 -> 121
    CHECK(!matchSerial("12345"));

    // Exchange dispatch by contest type (0 = zone, else serial).
    CHECK(matchExchange("14", 0));
    CHECK(!matchExchange("41", 0));   // invalid zone
    CHECK(matchExchange("41", 1));    // valid serial
    CHECK(matchExchange("1234", 1));
}

static void test_prosign() {
    CHECK(matchProsignTok("K", "K"));
    CHECK(matchProsignTok("k", "K"));     // case-insensitive
    CHECK(matchProsignTok("R", "R"));
    CHECK(!matchProsignTok("K", "R"));
    CHECK(!matchProsignTok("K", ""));     // no expectation -> never matches
    CHECK(!matchProsignTok("", "K"));
}

static void test_noise() {
    // Common noise applies to every slot.
    CHECK(isNoiseToken(String("DE"),  kCallsignNoise, String("")));
    CHECK(isNoiseToken(String("TNX"), kRstNoise,      String("")));
    // Slot-specific noise.
    CHECK(isNoiseToken(String("CQ"),  kCallsignNoise, String("")));
    CHECK(isNoiseToken(String("RST"), kRstNoise,      String("")));
    CHECK(isNoiseToken(String("599"), kExchangeNoise, String("")));

    // Regression (28c03f3): a bare "K" must NOT be noise for the callsign
    // slot, or the leading letter of "K2XYZ" gets dropped and the concat
    // buffer can never rebuild the call.
    CHECK(!isNoiseToken(String("K"), kCallsignNoise, String("")));

    // The expected literal is never treated as noise.
    CHECK(!isNoiseToken(String("RST"), kRstNoise, String("RST")));
}

static void test_over_control_tokens() {
    CHECK(isEndOfOver(String("K")));
    CHECK(isEndOfOver(String("BK")));
    CHECK(isEndOfOver(String("AR")));
    CHECK(isEndOfOver(String("73")));
    CHECK(isEndOfOver(String("72")));
    CHECK(!isEndOfOver(String("N")));    // could be a cut 9 in a split RST
    CHECK(!isEndOfOver(String("R")));

    CHECK(isSlotKeyword(String("rst")));
    CHECK(isSlotKeyword(String("RST")));  // lower-cased internally
    CHECK(isSlotKeyword(String("name")));
    CHECK(!isSlotKeyword(String("foo")));

    // isRepeatTrigger is fed an already-upper-cased token by the runtime.
    CHECK(isRepeatTrigger(String("AGN")));
    CHECK(isRepeatTrigger(String("RPT")));
    CHECK(isRepeatTrigger(String("?")));     // bare "?" = say again (T1.2)
    CHECK(!isRepeatTrigger(String("agn")));
    CHECK(!isRepeatTrigger(String("QRZ?"))); // multi-char token, not bare "?"
    CHECK(!isRepeatTrigger(String("HW?")));
}

static void feedAll(InfoParser& p, std::initializer_list<const char*> toks) {
    for (const char* t : toks) p.feed(String(t));
}

static void test_info_parser() {
    {   // RST + name (doubled) + QTH in one over
        InfoParser p; p.reset();
        feedAll(p, {"name", "willi", "willi", "qth", "vienna", "=", "rst", "599"});
        CHECK(p.name == String("willi"));   // conventional doubling deduped
        CHECK(p.qth  == String("vienna"));
        CHECK(p.rst);
    }
    {   // multi-word QTH is preserved
        InfoParser p; p.reset();
        feedAll(p, {"qth", "new", "york"});
        CHECK(p.qth == String("new york"));
    }
    {   // RST keyed as "ur 5nn"
        InfoParser p; p.reset();
        feedAll(p, {"ur", "5nn"});
        CHECK(p.rst);
    }
    {   // layer-2 fields set the generic "other" flag, no name/qth captured
        InfoParser p; p.reset();
        feedAll(p, {"rig", "ic7300", "ant", "efhw", "wx", "sunny"});
        CHECK(p.other);
        CHECK(p.name == String(""));
        CHECK(p.qth  == String(""));
        CHECK(!p.rst);
    }
    {   // dedup is case-insensitive, keeps the first form
        InfoParser p; p.reset();
        feedAll(p, {"name", "tom", "TOM"});
        CHECK(p.name == String("tom"));
    }
    {   // reset clears everything
        InfoParser p;
        feedAll(p, {"name", "bob", "rst", "599"});
        p.reset();
        CHECK(p.name == String(""));
        CHECK(!p.rst);
        CHECK(!p.other);
        CHECK(!p.framed);
        CHECK(p.cur == F_NONE);
    }
    {   // formality signal (T2.3): "de" framing
        InfoParser p; p.reset();
        feedAll(p, {"oe5abc", "de", "dl3xyz", "rst", "599", "name", "bob"});
        CHECK(p.framed);
        CHECK(p.name == String("bob"));     // captured despite the framing
    }
    {   // informal over: no call, no "de" -> not framed
        InfoParser p; p.reset();
        feedAll(p, {"rst", "599", "name", "bob", "qth", "london"});
        CHECK(!p.framed);
    }
    {   // a lone callsign token also counts as framing
        InfoParser p; p.reset();
        feedAll(p, {"de", "w1aw"});
        CHECK(p.framed);
    }
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("QSO Bot matcher tests\n");
    test_callsign_shape();
    test_match_callsign_vs_botcall();
    test_rst();
    test_zone_serial_exchange();
    test_prosign();
    test_noise();
    test_over_control_tokens();
    test_info_parser();

    std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
    if (g_fails == 0) std::printf("OK\n");
    return g_fails == 0 ? 0 : 1;
}

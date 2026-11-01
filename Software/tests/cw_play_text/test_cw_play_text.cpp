// test_cw_play_text.cpp -- what the M32 sounds for `PUT cw/play/<text>`, run on the host against the
// firmware's own code (extract.py copies it out of m32_v6.ino). The loop below is fetchNewWord()'s
// playCW branch; each word becomes an element string ('1' dit, '2' dah, '0' between characters).
//   make run
#include "string_shim.h"
#include <cstdio>
#include <vector>

boolean repeatPlayCW = false;
String playCWBuffer;
uint8_t playCWIndex = 0;
uint8_t playCWMaxIndex = 0;

// C++ strchr() returns const char*; the firmware's C-style call wants char*
#define strchr(s, c) ((char*) ::strchr((s), (c)))
#include "extracted.inc"
#undef strchr

static int checks = 0, fails = 0;
static void check(bool ok, const char* what, const std::string& got, const std::string& want, int line) {
    checks++;
    if (!ok) { fails++; std::printf("  FAIL line %d: %s\n    got  %s\n    want %s\n", line, what, got.c_str(), want.c_str()); }
}
#define EQ(got, want) check((got) == (want), #got, (got), (want), __LINE__)

// fetchNewWord()'s playCW branch, word by word, as element strings ("<pause>" for \p)
static std::string play(const char* text) {
    playCWBuffer = text; playCWIndex = 0; playCWMaxIndex = playCWBuffer.length(); repeatPlayCW = false;
    std::string out;
    for (;;) {
        String w = getM32PWord();
        if (w == "") break;
        w.replace("T", "");
        if (!out.empty()) out += ' ';
        if (w.indexOf('P') != -1) { out += "<pause>"; continue; }
        out += generateCWword(w).str();
    }
    return out;
}

int main() {
    // letters, for reference: m -- 22, k -.- 212, a .- 12
    EQ(play("mk"), std::string("220212"));
    EQ(play("m k"), std::string("22 212"));

    // the prosigns of the Koch lessons, in the angle-bracket form GET kochlesson lists them
    EQ(play("<sk>"), std::string("111212"));      // ...-.-
    EQ(play("<as>"), std::string("12111"));       // .-...
    EQ(play("<kn>"), std::string("21221"));       // -.--.
    EQ(play("<ka>"), std::string("21212"));       // -.-.-
    EQ(play("<ve>"), std::string("11121"));       // ...-.
    EQ(play("<bk>"), std::string("2111212"));     // -...-.-
    EQ(play("+"),    std::string("12121"));       // .-.-.  (<ar>, a plain character)
    EQ(play("<ar>"), std::string("12121"));

    // inside a trainer group: one word, the prosign one character of it
    EQ(play("m<sk>k"), std::string("2201112120212"));
    EQ(play("<kn><ve>a"), std::string("2122101112101" "2"));

    // the other spellings the firmware accepts
    EQ(play("<SK>"), std::string("111212"));      // case does not matter (lowercased first)
    EQ(play("[sk]"), std::string("111212"));      // the File Player's bracket form
    EQ(play("\\sk"), std::string("111212"));      // backslash form

    // NOT a way to send one: the one-byte internal code is lowercased into the letter
    EQ(play("K"), std::string("212"));

    // the done event's look-ahead: more to play before the last word, nothing after it
    playCWBuffer = "m <sk>"; playCWIndex = 0; playCWMaxIndex = playCWBuffer.length(); repeatPlayCW = false;
    getM32PWord();
    check(playCWHasMore(), "playCWHasMore() before <sk>", "false", "true", __LINE__);
    getM32PWord();
    check(!playCWHasMore(), "playCWHasMore() after <sk>", "true", "false", __LINE__);

    std::printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

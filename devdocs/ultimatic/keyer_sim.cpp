// keyer_sim.cpp -- host-side simulation of the Morserino-32 paddle keyer state machine.
//
// Purpose: check the Ultimatic mode against the seven points of "Summary of Actions
// of the Keys" (QST May 1955, p.122/128, John Kaye W6SRY) without hardware in hand.
//
// This is a faithful transcription of doPaddleIambic() in m32_v6.ino -- state names,
// latch bits, timer arithmetic and the order of the INTER_ELEMENT decision are copied
// verbatim; only the display / LoRa / decoder side effects are dropped.  The loop()
// quirk that checkPaddles() is NOT called while keyerState is DIT, DAH or KEY_START
// is modelled too, because it decides when paddle levels can change.
//
// Build:  c++ -std=c++17 -O1 -o keyer_sim keyer_sim.cpp
// Run:    ./keyer_sim            (runs T1..T8 for both the legacy and the fixed logic)

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------- keyer model

enum KEYERSTATES { IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT };

#define DIT_L    0x01
#define DAH_L    0x02
#define DIT_LAST 0x04

enum CurtisMode { IAMBICA = 1, IAMBICB = 2, ULTIMATIC = 3, NONSQUEEZE = 4, STRAIGHTKEY = 5 };

// preferences (firmware defaults)
static int p_curtisMode   = ULTIMATIC;
static int p_curtisBDahT  = 45;   // %
static int p_curtisBDitT  = 75;   // %
static int p_acs          = 0;
static int p_latency      = 4;    // eighths of a dit

// which fix is compiled into the model
enum Logic { LEGACY, FIXED };
static Logic logicVariant = LEGACY;

static unsigned int ditLength = 100, dahLength = 300;   // 12 wpm -> 100 ms dit

static unsigned char keyerControl = 0;
static bool DIT_FIRST = false;                 // legacy: paddle that opened the character
enum Touched { TOUCHED_NONE, TOUCHED_DIT, TOUCHED_DAH };
static Touched lastTouched = TOUCHED_NONE;     // fixed: most recently *closed* paddle
static KEYERSTATES keyerState = IDLE_STATE;

static long now_ms = 0;                        // stands in for millis()
static long ktimer, curtistimer, latencytimer, corrTime, acsTimer = 0;

// captured output
struct Elem { char type; long start, end; };
static std::vector<Elem> elements;
static long keyDownAt = -1;

static void keyOut(bool on, char what) {
    if (on) keyDownAt = now_ms;
    else if (keyDownAt >= 0) { elements.push_back({what, keyDownAt, now_ms}); keyDownAt = -1; }
}

static void updatePaddleLatch(bool dit, bool dah) {
    if (dit) keyerControl |= DIT_L;
    if (dah) keyerControl |= DAH_L;
}
static void clearPaddleLatches() { keyerControl &= ~(DIT_L + DAH_L); }
static void setDITstate() { keyerState = DIT; }
static void setDAHstate() { keyerState = DAH; }

// ------------------------------------------------ doPaddleIambic() transcription

static char pendingElem = '?';   // which element KEY_START is about to sound

static bool doPaddleIambic(bool dit, bool dah) {

    if (logicVariant == FIXED) {
        // rising-edge tracking: Ultimatic control follows the most recent closure
        // (QST point 6/7), so levels alone are not enough.
        static bool prevDit = false, prevDah = false;
        if (dit && !prevDit) lastTouched = TOUCHED_DIT;
        if (dah && !prevDah) lastTouched = TOUCHED_DAH;   // dah wins a simultaneous closure, so a
        prevDit = dit; prevDah = dah;                     // squeeze still gives one dit then dahs
    }

    switch (keyerState) {
    case IDLE_STATE:
        if (dit || dah) {
            updatePaddleLatch(dit, dah);
            if (dit) { setDITstate(); DIT_FIRST = true;  }
            else     { setDAHstate(); DIT_FIRST = false; }
        } else return false;
        break;

    case DIT:
        if (p_acs > 0 && now_ms <= acsTimer) break;
        clearPaddleLatches();
        keyerControl |= DIT_LAST;
        ktimer = ditLength;
        pendingElem = '.';
        switch (p_curtisMode) {
            case ULTIMATIC: case IAMBICB: curtistimer = 2 + (ditLength * p_curtisBDitT / 100); break;
            case NONSQUEEZE:              curtistimer = 3; break;
            default:                      curtistimer = ditLength; break;
        }
        keyerState = KEY_START;
        break;

    case DAH:
        if (p_acs > 0 && now_ms <= acsTimer) break;
        clearPaddleLatches();
        keyerControl &= ~(DIT_LAST);
        ktimer = dahLength;
        pendingElem = '-';
        switch (p_curtisMode) {
            case ULTIMATIC: case IAMBICB: curtistimer = 2 + (dahLength * p_curtisBDahT / 100); break;
            case NONSQUEEZE:              curtistimer = 3; break;
            default:                      curtistimer = dahLength; break;
        }
        keyerState = KEY_START;
        break;

    case KEY_START:
        keyOut(true, pendingElem);
        corrTime = now_ms - 6;
        ktimer += corrTime;
        curtistimer += corrTime;
        keyerState = KEYED;
        break;

    case KEYED:
        if (now_ms >= ktimer) {
            keyOut(false, pendingElem);
            ktimer = now_ms + ditLength - 1;
            latencytimer = now_ms + (p_latency * ditLength / 8);
            keyerState = INTER_ELEMENT;
        } else if (now_ms >= curtistimer) {
            if (keyerControl & DIT_LAST) updatePaddleLatch(false, dah);
            else                         updatePaddleLatch(dit, false);
        }
        break;

    case INTER_ELEMENT:
        if (now_ms < latencytimer) {
            if (keyerControl & DIT_LAST) updatePaddleLatch(false, dah);
            else                         updatePaddleLatch(dit, false);
        } else {
            updatePaddleLatch(dit, dah);
            if (now_ms >= ktimer) {
                switch (keyerControl) {
                case 3: case 7:                                  // both paddles latched
                    switch (p_curtisMode) {
                    case STRAIGHTKEY: break;
                    case NONSQUEEZE:  if (DIT_FIRST) setDITstate(); else setDAHstate(); break;
                    case ULTIMATIC:
                        if (logicVariant == LEGACY) {
                            if (DIT_FIRST) setDAHstate(); else setDITstate();
                        } else {
                            if (lastTouched == TOUCHED_DIT) setDITstate(); else setDAHstate();
                        }
                        break;
                    default:          if (keyerControl & DIT_LAST) setDAHstate(); else setDITstate();
                    }
                    break;
                case 1: case 5: setDITstate(); break;
                case 2: case 6: setDAHstate(); break;
                case 0: case 4:
                    keyerState = IDLE_STATE;
                    keyerControl = 0;
                    break;
                }
            }
        }
    }
    return (keyerControl & 3) != 0;
}

// ------------------------------------------------------------------ test driver

struct Event { long t; char paddle; bool down; };   // paddle: 'd' = dit, 'D' = dah

static std::vector<Elem> run(const std::vector<Event> &script, long duration, Logic lv) {
    logicVariant = lv;
    keyerControl = 0; keyerState = IDLE_STATE; DIT_FIRST = false;
    lastTouched = TOUCHED_NONE; keyDownAt = -1; acsTimer = 0;
    elements.clear();

    bool ditLevel = false, dahLevel = false;    // raw paddle levels
    bool leftKey = false, rightKey = false;     // what checkPaddles() has published
    size_t next = 0;

    for (now_ms = 0; now_ms <= duration; ++now_ms) {
        while (next < script.size() && script[next].t <= now_ms) {
            if (script[next].paddle == 'd') ditLevel = script[next].down;
            else                            dahLevel = script[next].down;
            ++next;
        }
        // loop(): checkPaddles() is skipped while an element is being set up
        if (keyerState != DIT && keyerState != DAH && keyerState != KEY_START) {
            leftKey = ditLevel; rightKey = dahLevel;
        }
        doPaddleIambic(leftKey, rightKey);
    }
    return elements;
}

static std::string stream(const std::vector<Elem> &e) {
    std::string s; for (auto &x : e) s += x.type; return s;
}
// elements that START inside [t0, t1).  Everything from t1 on is ignored: releasing a
// paddle inside the Curtis-B early-latch window legitimately produces one more element,
// which is real firmware behaviour but says nothing about Ultimatic selection.
static std::string window(const std::vector<Elem> &e, long t0, long t1) {
    std::string s; for (auto &x : e) if (x.start >= t0 && x.start < t1) s += x.type; return s;
}
static bool allSame(const std::string &s, char c) {
    return !s.empty() && s.find_first_not_of(c) == std::string::npos;
}

struct Test {
    const char *id; const char *point; const char *what;
    std::vector<Event> script; long dur; long cut;   // cut = both paddles open again
    bool (*ok)(const std::string &);                 // verdict on window(elements, 0, cut)
};

int main() {
    // 12 wpm: dit 100 ms, dah 300 ms, latency 4/8 dit = 50 ms.
    std::vector<Test> tests = {
      {"T1a", "1", "tap dit once -> exactly one dit",
       {{0,'d',true},{40,'d',false}}, 1200, 1200,
       [](const std::string &s){ return s == "."; }},

      {"T1b", "1", "tap dah once -> exactly one dah",
       {{0,'D',true},{40,'D',false}}, 1200, 1200,
       [](const std::string &s){ return s == "-"; }},

      {"T2a", "2", "hold dit -> a run of dits",
       {{0,'d',true},{700,'d',false}}, 1500, 700,
       [](const std::string &s){ return allSame(s, '.') && s.size() >= 3; }},

      {"T2b", "2", "hold dah -> a run of dahs",
       {{0,'D',true},{1300,'D',false}}, 2000, 1300,
       [](const std::string &s){ return allSame(s, '-') && s.size() >= 3; }},

      {"T3",  "3", "dah opens, dit squeezed in -> dah then dit (letter N)",
       {{0,'D',true},{100,'d',true},{160,'D',false},{180,'d',false}}, 1200, 1200,
       [](const std::string &s){ return s == "-."; }},

      {"T4",  "3+4", "dah held, dit squeezed and held -> one dah, then only dits",
       {{0,'D',true},{100,'d',true},{700,'d',false},{700,'D',false}}, 1500, 700,
       [](const std::string &s){ return s.size() >= 3 && s[0] == '-' && allSame(s.substr(1), '.'); }},

      {"T5",  "5", "dit released with dah still closed -> output reverts to dah",
       {{0,'D',true},{100,'d',true},{500,'d',false},{1400,'D',false}}, 1800, 1400,
       [](const std::string &s){ return s.rfind("-.-", 0) == 0 && allSame(s.substr(2), '-'); }},

      {"T5b", "1+7", "two separate dit closures under a held dah -> two dits",
       {{0,'D',true},{300,'d',true},{340,'d',false},{550,'d',true},{590,'d',false},
        {1500,'D',false}}, 1900, 1500,
       [](const std::string &s){ return s.rfind("-..", 0) == 0; }},

      {"T8",  "6", "dah opened it, dit held throughout, dah FLICKED -> dah seizes back",
       {{0,'D',true},{100,'d',true},          // dah opens, dit squeezed and held
        {380,'D',false},{420,'D',true},       // the flick; dit never released
        {1200,'d',false},{1200,'D',false}}, 1800, 1200,
       [](const std::string &s){ return s.find('-', 1) != std::string::npos; }},

      {"T8b", "6", "mirror: dit opened it, dah held throughout, dit FLICKED -> dit seizes back",
       {{0,'d',true},{100,'D',true},
        {380,'d',false},{420,'d',true},
        {1400,'d',false},{1400,'D',false}}, 2000, 1400,
       [](const std::string &s){ return s.find('.', 1) != std::string::npos; }},

      {"TSQ", "3", "both paddles closed in the same instant -> one dit, then dahs (unchanged)",
       {{0,'d',true},{0,'D',true},{1400,'d',false},{1400,'D',false}}, 1800, 1400,
       [](const std::string &s){ return s.rfind(".-", 0) == 0 && allSame(s.substr(1), '-'); }},

      {"T8c", "6+7", "opening paddle released for good, then re-closed while the other is held",
       {{0,'D',true},{100,'d',true},          // dah opens, control passes to dit
        {350,'D',false},                      // dah released for a good while
        {800,'D',true},                       // ... and closed again: guarantees a dah
        {1500,'d',false},{1500,'D',false}}, 1900, 1500,
       [](const std::string &s){ return s.find('-', 1) != std::string::npos; }},
    };

    printf("%-5s %-4s %-62s %-10s %-14s %-14s\n", "test", "QST", "scenario", "legacy/fixed", "legacy", "fixed");
    printf("%s\n", std::string(116, '-').c_str());
    int failLegacy = 0, failFixed = 0;
    for (auto &t : tests) {
        auto a = run(t.script, t.dur, LEGACY);
        auto b = run(t.script, t.dur, FIXED);
        std::string sa = window(a, 0, t.cut), sb = window(b, 0, t.cut);
        bool okA = t.ok(sa), okB = t.ok(sb);
        if (!okA) ++failLegacy;
        if (!okB) ++failFixed;
        char verdict[24];
        snprintf(verdict, sizeof verdict, "%s / %s", okA ? "ok  " : "FAIL", okB ? "ok" : "FAIL");
        printf("%-5s %-4s %-62s %-10s %-14s %-14s\n", t.id, t.point, t.what, verdict, sa.c_str(), sb.c_str());
        if (getenv("VERBOSE")) {
            printf("        legacy:"); for (auto &x : a) printf(" %c[%ld-%ld]", x.type, x.start, x.end);
            printf("\n        fixed :"); for (auto &x : b) printf(" %c[%ld-%ld]", x.type, x.start, x.end);
            printf("\n");
        }
    }
    printf("\nlegacy (static DIT_FIRST):  %d of %zu scenarios violate the 1955 spec\n", failLegacy, tests.size());
    printf("fixed  (live lastTouched):  %d of %zu scenarios violate the 1955 spec\n", failFixed, tests.size());
    return 0;
}

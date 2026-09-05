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
enum Logic { LEGACY,   // before 54ce495: static DIT_FIRST
             FIXED,    // 54ce495: live lastTouched, still deciding from the sampled latches
             MEMORY }; // the Ultimatic memory model: edge-armed memories + live paddle levels
static Logic logicVariant = LEGACY;

static unsigned int ditLength = 100, dahLength = 300;   // 12 wpm -> 100 ms dit

static unsigned char keyerControl = 0;
static bool DIT_FIRST = false;                 // legacy: paddle that opened the character
enum Touched { TOUCHED_NONE, TOUCHED_DIT, TOUCHED_DAH };
static Touched lastTouched = TOUCHED_NONE;     // fixed: most recently *closed* paddle
static bool ultDitMemory = false, ultDahMemory = false;   // memory model: armed by closures
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

static void ultimaticSelect(bool dit, bool dah) {
    keyerControl &= ~(DIT_L + DAH_L);
    if (ultDitMemory && ultDahMemory) keyerControl |= (lastTouched == TOUCHED_DIT) ? DIT_L : DAH_L;
    else if (ultDitMemory)            keyerControl |= DIT_L;
    else if (ultDahMemory)            keyerControl |= DAH_L;
    else if (dit && dah)              keyerControl |= (lastTouched == TOUCHED_DIT) ? DIT_L : DAH_L;
    else if (dit)                     keyerControl |= DIT_L;
    else if (dah)                     keyerControl |= DAH_L;
}

static char pendingElem = '?';   // which element KEY_START is about to sound
static long ditOpenedAt = 0, dahOpenedAt = 0;   // when each paddle last went open
static const int CHATTER_GUARD_MS = 5;          // as in the firmware
static bool useLatencyMute = false;             // true = the first cut, which muted the whole
                                                //        Latency window (swallowed real re-taps)
static bool prevDit = false, prevDah = false;   // previous paddle levels, for edge detection
                                                // (file scope so run() can clear them: leaking
                                                //  them between runs would fake a rising edge)

static bool doPaddleIambic(bool dit, bool dah) {

    if (logicVariant != LEGACY) {
        // rising-edge tracking: Ultimatic control follows the most recent closure
        // (QST point 6/7), so levels alone are not enough.
        if (!dit && prevDit) ditOpenedAt = now_ms;     // remember when each paddle went open
        if (!dah && prevDah) dahOpenedAt = now_ms;
        bool muted = (now_ms < latencytimer);          // Latency window
        if (dit && !prevDit) {
            lastTouched = TOUCHED_DIT;
            bool bounce = (now_ms - ditOpenedAt) < CHATTER_GUARD_MS;
            bool drop = (useLatencyMute ? muted : bounce) && (keyerControl & DIT_LAST);
            if (!drop) ultDitMemory = true;
        }
        if (dah && !prevDah) {                            // dah wins a simultaneous closure, so a
            lastTouched = TOUCHED_DAH;                    // squeeze still gives one dit then dahs
            bool bounce = (now_ms - dahOpenedAt) < CHATTER_GUARD_MS;
            bool drop = (useLatencyMute ? muted : bounce) && !(keyerControl & DIT_LAST);
            if (!drop) ultDahMemory = true;
        }
        prevDit = dit; prevDah = dah;
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
        ultDitMemory = false;
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
        ultDahMemory = false;
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
                if (logicVariant == MEMORY && p_curtisMode == ULTIMATIC)
                    ultimaticSelect(dit, dah);
                switch (keyerControl) {
                case 3: case 7:                                  // both paddles latched
                    switch (p_curtisMode) {
                    case STRAIGHTKEY: break;
                    case NONSQUEEZE:  if (DIT_FIRST) setDITstate(); else setDAHstate(); break;
                    case ULTIMATIC:
                        if (logicVariant == LEGACY) {
                            if (DIT_FIRST) setDAHstate(); else setDITstate();
                        } else {   // MEMORY never reaches here: ultimaticSelect sets one bit at most
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
                    ultDitMemory = ultDahMemory = false;
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
    prevDit = prevDah = false; pendingElem = '?';
    ultDitMemory = ultDahMemory = false; latencytimer = 0;
    ditOpenedAt = dahOpenedAt = -100000;
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

// ---- follow-up scans (SP5DNA, Sept 2026): a plain squeeze, released at varying times ----
// Script: DAH opens the character, DIT squeezed in at +30 ms, BOTH released together at rel.
// No re-closure anywhere, so the point-6 fix is not involved.

static std::string squeeze(long rel, Logic lv, int mode, int cbDah, int cbDit, int lat) {
    p_curtisMode = mode; p_curtisBDahT = cbDah; p_curtisBDitT = cbDit; p_latency = lat;
    std::vector<Event> sc = {{0,'D',true},{30,'d',true},{rel,'D',false},{rel,'d',false}};
    return stream(run(sc, rel + 2500, lv));
}

// maximal runs of rel where pred(stream) holds, printed as ranges
static void ranges(const char *label, long lo, long hi, Logic lv, int mode,
                   int cbDah, int cbDit, int lat, bool (*pred)(const std::string &)) {
    printf("  %-46s", label);
    long start = -1; int shown = 0;
    for (long rel = lo; rel <= hi; ++rel) {
        bool bad = pred(squeeze(rel, lv, mode, cbDah, cbDit, lat));
        if (bad && start < 0) start = rel;
        if ((!bad || rel == hi) && start >= 0) {
            long end = bad ? rel : rel - 1;
            if (shown++ < 5) printf(" [%ld-%ld]%ldms", start, end, end - start + 1);
            start = -1;
        }
    }
    printf(shown == 0 ? "  none\n" : (shown > 5 ? " ...\n" : "\n"));
}

static bool lostTap (const std::string &s) { return s == "-"; }                       // Bug A
static bool extraDah(const std::string &s) { return s.find('-', 1) != std::string::npos; } // Bug B

static void followupScans() {
    ditLength = 240; dahLength = 720;              // 5 WPM, as reported
    printf("Follow-up scans - 5 WPM (dit 240 ms, dah 720 ms, latency 4/8 = 120 ms)\n");
    printf("Script: DAH down@0, DIT down@30, both released together at rel. No re-closure.\n\n");

    printf("Bug A - the squeezed-in dit is lost entirely (output is a bare dah):\n");
    ranges("Ultimatic, post-fix, CurtisB 45/75 (shipped)", 20, 500, FIXED,  ULTIMATIC, 45, 75, 4, lostTap);
    ranges("Ultimatic, PRE-fix (legacy DIT_FIRST)",         20, 500, LEGACY, ULTIMATIC, 45, 75, 4, lostTap);
    ranges("Iambic B, same input",                          20, 500, FIXED,  IAMBICB,   45, 75, 4, lostTap);
    ranges("Iambic A, same input",                          20, 500, FIXED,  IAMBICA,   45, 75, 4, lostTap);
    ranges("Ultimatic, CurtisB DahT% set to 0",             20, 500, FIXED,  ULTIMATIC,  0, 75, 4, lostTap);
    ranges("Ultimatic, DahT% 0 + DitT% 75 + Latency 0",      20, 500, FIXED,  ULTIMATIC,  0, 75, 0, lostTap);
    ranges("MEMORY MODEL, shipped defaults",                 20, 500, MEMORY, ULTIMATIC, 45, 75, 4, lostTap);

    printf("\nBug B - an unwanted dah after control has passed to the dit:\n");
    ranges("Ultimatic, post-fix, CurtisB 45/75 (shipped)", 600, 2600, FIXED,  ULTIMATIC, 45, 75, 4, extraDah);
    ranges("Ultimatic, PRE-fix (legacy DIT_FIRST)",         600, 2600, LEGACY, ULTIMATIC, 45, 75, 4, extraDah);
    ranges("Ultimatic, CurtisB DitT% set to 0",             600, 2600, FIXED,  ULTIMATIC, 45,  0, 4, extraDah);
    ranges("Ultimatic, Latency 0 (CurtisB left at 45/75)",  600, 2600, FIXED,  ULTIMATIC, 45, 75, 0, extraDah);
    ranges("Ultimatic, CurtisB 0/0 AND Latency 0",          600, 2600, FIXED,  ULTIMATIC,  0,  0, 0, extraDah);
    ranges("Ultimatic, DahT% 0 + DitT% 75 + Latency 0",      600, 2600, FIXED,  ULTIMATIC,  0, 75, 0, extraDah);
    ranges("MEMORY MODEL, shipped defaults",                 600, 2600, MEMORY, ULTIMATIC, 45, 75, 4, extraDah);
    ranges("MEMORY MODEL, Latency 7 (max)",                  600, 2600, MEMORY, ULTIMATIC, 45, 75, 7, extraDah);

    printf("\nRegression: do the other keyer modes change? (squeeze scan, rel 20..2600)\n");
    for (int mode : {IAMBICA, IAMBICB, NONSQUEEZE}) {
        long diffs = 0, first = -1;
        for (long rel = 20; rel <= 2600; ++rel) {
            std::string a = squeeze(rel, FIXED, mode, 45, 75, 4);
            std::string b = squeeze(rel, MEMORY, mode, 45, 75, 4);
            if (a != b) { ++diffs; if (first < 0) first = rel; }
        }
        printf("  %-12s  %ld of 2581 release times differ%s\n",
               mode == IAMBICA ? "Iambic A" : mode == IAMBICB ? "Iambic B" : "Non-squeeze",
               diffs, diffs ? "" : "  <- unchanged");
    }

    printf("\nDoes the Latency preference affect Ultimatic at all? (it must not - see latencyScan)\n");
    for (int lat : {0, 4, 7}) {
        long diffs = 0;
        for (long rel = 20; rel <= 2600; ++rel)
            if (squeeze(rel, MEMORY, ULTIMATIC, 45, 75, lat) !=
                squeeze(rel, MEMORY, ULTIMATIC, 45, 75, 4)) ++diffs;
        printf("  Latency %d/8 vs 4/8: %ld of 2581 release times differ%s\n",
               lat, diffs, diffs ? "" : "  <- no effect, as intended");
    }

    printf("\nSpot checks quoted in the report (latency 4/8):\n");
    for (long rel : {40L, 200L, 350L, 900L, 1134L, 1312L, 1609L, 1787L})
        printf("  rel=%-5ld ultimatic=%-8s iambicB=%-8s iambicA=%s\n", rel,
               squeeze(rel, FIXED, ULTIMATIC, 45, 75, 4).c_str(),
               squeeze(rel, FIXED, IAMBICB,   45, 75, 4).c_str(),
               squeeze(rel, FIXED, IAMBICA,   45, 75, 4).c_str());
    p_curtisMode = ULTIMATIC; p_curtisBDahT = 45; p_curtisBDitT = 75; p_latency = 4;
    ditLength = 100; dahLength = 300;
}

// ---- Does the Latency mute swallow deliberate taps? (SP5DNA's 40 WPM observation) ----
// Two separate closures of the SAME paddle - the paddle that is in control - with a varying
// open interval between them. Both are deliberate, so both must sound.
static void latencyScan() {
    ditLength = 30; dahLength = 90;                 // 40 WPM
    p_curtisMode = ULTIMATIC; p_curtisBDahT = 45; p_curtisBDitT = 75;
    const long C = 12;                              // a brisk tap at 40 WPM

    printf("Latency mute vs. deliberate taps - 40 WPM (dit 30 ms), Ultimatic\n");
    printf("Two dit taps of %ld ms, separated by an open interval of `gap` ms.\n", C);
    printf("Both taps are deliberate, so the correct answer is always two dits.\n\n");
    printf("  gap    first cut (Latency mute)   shipped (5 ms chatter guard)\n");
    printf("         lat 0     lat 4     lat 7    lat 0     lat 4     lat 7\n");
    for (long gap : {2L, 4L, 6L, 10L, 18L, 22L, 26L, 30L, 40L, 60L}) {
        std::vector<Event> sc = {{0,'d',true},{C,'d',false},
                                 {C+gap,'d',true},{C+gap+C,'d',false}};
        printf("  %-6ld", gap);
        for (bool mute : {true, false}) {
            useLatencyMute = mute;
            for (int lat : {0, 4, 7}) {
                p_latency = lat;
                printf(" %-9s", stream(run(sc, 1200, MEMORY)).c_str());
            }
        }
        printf("\n");
    }
    useLatencyMute = false;
    printf("\n  (two dits = both taps sounded, one dit = a deliberate tap was swallowed)\n");

    printf("\n  Does the guard still catch real contact bounce?\n");
    ditLength = 240; dahLength = 720; p_latency = 4;
    for (long open : {2L, 3L, 4L, 10L, 20L}) {
        std::vector<Event> ch = {{0,'d',true},{300,'d',false},{300+open,'d',true},{330+open,'d',false}};
        printf("    %2ld ms dropout mid-hold -> %-4s %s\n", open,
               stream(run(ch, 2000, MEMORY)).c_str(),
               open < 5 ? "(filtered - bounce)" : "(taken as a real release, which it is)");
    }
    ditLength = 100; dahLength = 300; p_latency = 4;
}

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "scan") { followupScans(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "latency") { latencyScan(); return 0; }
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

    printf("%-5s %-4s %-56s %-18s %-9s %-9s %-9s\n", "test", "QST", "scenario",
           "legacy/fixed/mem", "legacy", "fixed", "memory");
    printf("%s\n", std::string(122, '-').c_str());
    int fail[3] = {0, 0, 0};
    const Logic variants[3] = {LEGACY, FIXED, MEMORY};
    for (auto &t : tests) {
        std::string out[3]; bool ok[3];
        for (int i = 0; i < 3; ++i) {
            out[i] = window(run(t.script, t.dur, variants[i]), 0, t.cut);
            ok[i] = t.ok(out[i]);
            if (!ok[i]) ++fail[i];
        }
        char verdict[40];
        snprintf(verdict, sizeof verdict, "%s / %s / %s",
                 ok[0] ? "ok  " : "FAIL", ok[1] ? "ok  " : "FAIL", ok[2] ? "ok" : "FAIL");
        printf("%-5s %-4s %-56s %-18s %-9s %-9s %-9s\n", t.id, t.point, t.what, verdict,
               out[0].c_str(), out[1].c_str(), out[2].c_str());
    }
    printf("\nlegacy (static DIT_FIRST):        %d of %zu scenarios violate the 1955 spec\n", fail[0], tests.size());
    printf("fixed  (live lastTouched):        %d of %zu\n", fail[1], tests.size());
    printf("memory (edge memories + levels):  %d of %zu\n", fail[2], tests.size());
    return 0;
}

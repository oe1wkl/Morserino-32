// Host-side tests for ClickButton — the button decoder every M32 control gesture
// goes through. Compiles the *same* ClickButton.cpp the firmware builds.
//
// What is being guarded: a long click is reported while the button is still HELD,
// so the press it belongs to is not over when the caller acts on it. Callers act
// by leaving their loop, and the code they return to then polls the same button.
// If `clicks` is still showing that long press, the gesture is obeyed twice —
// which is how "leave a mode" also climbed several menu levels (#216), how one
// long press skipped a menu level (#216), and how leaving Preferences also left
// the mode underneath (#215).
//
//   make run

#include "arduino_pin_shim.h"
#include "../../src/Version 6 and newer/ClickButton.h"

#include <cstdio>
#include <cstring>

unsigned long sim_millis_value = 0;
int           sim_pin_value    = 1;      // open (active-low: not pressed)

static int failures = 0;

static void check(bool ok, const char* what) {
    printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) failures++;
}

// Replay one press through a button, polling every `stepMs` from t=0.
// Stops early the moment a non-zero `clicks` is seen (what a mode/menu/game loop
// does: it acts on the gesture and leaves), reporting what it saw and when.
struct Replay { int ownerSaw; unsigned long ownerAt; };

static Replay runOwner(ClickButton& b, unsigned long down, unsigned long up,
                       unsigned long until, unsigned long stepMs = 5) {
    for (unsigned long t = 0; t <= until; t += stepMs) {
        sim_millis_value = t;
        sim_pin_value    = (t >= down && t < up) ? 0 : 1;   // active-low
        b.Update();
        if (b.clicks) return { b.clicks, t };
    }
    return { 0, 0 };
}

// The caller resumes at `t` and reads once, exactly as menu_() or a game lobby does.
static int callerReadsAt(ClickButton& b, unsigned long t, unsigned long down, unsigned long up) {
    sim_millis_value = t;
    sim_pin_value    = (t >= down && t < up) ? 0 : 1;
    b.Update();
    return b.clicks;
}

static void test_long_click_is_delivered_once() {
    printf("long click, released before the caller resumes (the #215/#216 fault)\n");
    ClickButton b(0);
    Replay r = runOwner(b, 0, 1100, 1100);
    check(r.ownerSaw == -1, "the loop that owns the button sees the long click");
    check(callerReadsAt(b, 1150, 0, 1100) == 0,
          "the caller it returns to does NOT see it a second time");
}

static void test_long_click_still_held() {
    printf("long click, button still held when the caller resumes\n");
    ClickButton b(0);
    Replay r = runOwner(b, 0, 3000, 1100);
    check(r.ownerSaw == -1, "the owning loop sees the long click");
    check(callerReadsAt(b, 1150, 0, 3000) == 0,
          "the caller does not see it again (was already true before the fix)");
}

static void test_long_click_late_caller() {
    printf("long click, caller resumes long after the release\n");
    ClickButton b(0);
    Replay r = runOwner(b, 0, 1100, 1100);
    check(r.ownerSaw == -1, "the owning loop sees the long click");
    // Waiting does not help by itself: the caller's first Update() after an
    // unobserved release restarts the debounce timer, so the release branch
    // cannot fire on that same call however long it waited.
    check(callerReadsAt(b, 3000, 0, 1100) == 0,
          "still not delivered twice, 1.9 s after the release");
}

static void test_short_click_unaffected() {
    printf("short click - must be delivered exactly as before\n");
    ClickButton b(0);
    Replay r = runOwner(b, 0, 100, 900);
    check(r.ownerSaw == 1, "a short press reports 1 click");
    check(r.ownerAt >= 350, "reported only after the multi-click window closes");
    check(callerReadsAt(b, 900, 0, 100) == 0, "and is not delivered twice either");
}

static void test_double_click_unaffected() {
    printf("double click - must be delivered exactly as before\n");
    ClickButton b(0);
    int seen = 0;
    for (unsigned long t = 0; t <= 900; t += 5) {
        sim_millis_value = t;
        sim_pin_value = ((t >= 0 && t < 80) || (t >= 180 && t < 260)) ? 0 : 1;
        b.Update();
        if (b.clicks) { seen = b.clicks; break; }
    }
    check(seen == 2, "two presses inside multiclickTime report 2 clicks");
}

static void test_long_then_new_press() {
    printf("a fresh gesture after a long press must still get through\n");
    ClickButton b(0);
    runOwner(b, 0, 1100, 1100);          // long press, consumed by its owner
    (void) callerReadsAt(b, 1150, 0, 1100);   // caller polls once, sees nothing
    int seen = 0;                        // now the user presses again, briefly
    for (unsigned long t = 1200; t <= 2200; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 1300 && t < 1400) ? 0 : 1;
        b.Update();
        if (b.clicks) { seen = b.clicks; break; }
    }
    check(seen == 1, "the next short click is reported normally");
}

static void test_second_long_press() {
    printf("two long presses in a row - the second must be reported\n");
    ClickButton b(0);
    runOwner(b, 0, 1100, 1100);
    (void) callerReadsAt(b, 1150, 0, 1100);
    int seen = 0;
    for (unsigned long t = 1200; t <= 4000; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 1400 && t < 2800) ? 0 : 1;
        b.Update();
        if (b.clicks) { seen = b.clicks; break; }
    }
    check(seen == -1, "the second long press reports -1");
}

int main() {
    test_long_click_is_delivered_once();
    test_long_click_still_held();
    test_long_click_late_caller();
    test_short_click_unaffected();
    test_double_click_unaffected();
    test_long_then_new_press();
    test_second_long_press();
    printf("\n%s\n", failures ? "FAILURES" : "all ok");
    return failures ? 1 : 0;
}

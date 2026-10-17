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

// --- HeldLineFilter: FN held down by a serial line (classic M32, FN = GPIO0) ---------------
// Drives a button through the filter exactly as the firmware does: Update(), then filter(), every
// stepMs. Returns the first non-zero result and when it came (0/0 if none by `until`).
static Replay runFiltered(ClickButton& b, HeldLineFilter& f, unsigned long down, unsigned long up,
                          unsigned long until, unsigned long from = 0, unsigned long stepMs = 5) {
    for (unsigned long t = from; t <= until; t += stepMs) {
        sim_millis_value = t;
        sim_pin_value    = (t >= down && t < up) ? 0 : 1;
        b.Update();
        int c = f.filter(b.clicks, b.depressed);
        if (c) return { c, t };
    }
    return { 0, 0 };
}

static void test_filter_finger_long_press() {
    printf("filter: a finger's long press (1.5 s) acts on release, not after 1 s\n");
    ClickButton b(0); HeldLineFilter f;
    Replay r = runFiltered(b, f, 100, 1600, 3000);
    check(r.ownerSaw == -1, "the long press is delivered");
    check(r.ownerAt >= 1600, "at the release, not while held");
}

static void test_filter_line_hold_ignored() {
    printf("filter: a line holding the pin for 10 s does nothing, even at release\n");
    ClickButton b(0); HeldLineFilter f;
    Replay r = runFiltered(b, f, 100, 10100, 11000);
    check(r.ownerSaw == 0, "no gesture at all");
}

static void test_filter_down_at_reset() {
    printf("filter: a press already down at reset() is a line\n");
    ClickButton b(0); HeldLineFilter f;
    for (unsigned long t = 0; t <= 400; t += 5) {          // pin goes down before the filter starts
        sim_millis_value = t; sim_pin_value = (t >= 100) ? 0 : 1; b.Update();
    }
    f.reset();
    Replay r = runFiltered(b, f, 100, 1800, 2500, 405);     // released at 1.8 s: < 3 s, still ignored
    check(r.ownerSaw == 0, "ignored until released");
    Replay r2 = runFiltered(b, f, 2600, 2700, 3500, 2505);  // then a normal short click
    check(r2.ownerSaw == 1, "the next short click gets through");
}

static void test_filter_short_and_double() {
    printf("filter: short and double clicks pass unchanged\n");
    ClickButton b(0); HeldLineFilter f;
    Replay r = runFiltered(b, f, 0, 100, 900);
    check(r.ownerSaw == 1, "short click = 1");
    ClickButton b2(0); HeldLineFilter f2;
    int seen = 0;
    for (unsigned long t = 0; t <= 900; t += 5) {
        sim_millis_value = t;
        sim_pin_value = ((t < 80) || (t >= 180 && t < 260)) ? 0 : 1;
        b2.Update();
        int c = f2.filter(b2.clicks, b2.depressed);
        if (c) { seen = c; break; }
    }
    check(seen == 2, "double click = 2");
}

static void test_filter_written_back_once() {
    printf("filter: the delivered long press, written back into clicks, is not repeated\n");
    // What loop() does in WinKeyer mode: clicks = filter(clicks, depressed), every iteration.
    // Update() does not clear a -1 it did not produce until the multi-click window has passed,
    // so the next iterations would see the same -1 - the bench fault (Audio In Adj. starting and
    // stopping at once on the classic, 2026-10-16).
    ClickButton b(0); HeldLineFilter f;
    int delivered = 0;
    for (unsigned long t = 0; t <= 2500; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 100 && t < 1600) ? 0 : 1;
        b.Update();
        b.clicks = f.filter(b.clicks, b.depressed);
        if (b.clicks == -1) delivered++;
    }
    check(delivered == 1, "exactly one long press over the whole release window");
}

static void test_filter_short_line_marked() {
    printf("filter: a 2 s line hold, marked while a host connects, does nothing\n");
    // N1MM opening the classic's port: DTR on / RTS off for ~2 s, then both off (bench 2026-10-17).
    ClickButton b(0); HeldLineFilter f;
    int delivered = 0;
    for (unsigned long t = 0; t <= 3500; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 100 && t < 2100) ? 0 : 1;
        b.Update();
        if (t >= 300 && t < 2300 && b.depressed) f.markLine();   // what filterFn() does in the window
        b.clicks = f.filter(b.clicks, b.depressed);
        if (b.clicks) delivered++;
    }
    check(delivered == 0, "no long press delivered at the release");
}

static void test_filter_mark_while_up_is_harmless() {
    printf("filter: a mark while the button is up does not touch a later finger press\n");
    ClickButton b(0); HeldLineFilter f;
    sim_millis_value = 0; sim_pin_value = 1; b.Update();
    f.markLine();                                   // marked, but nothing is pressed
    f.filter(b.clicks, b.depressed);
    Replay r = runFiltered(b, f, 500, 2000, 3000, 5);
    check(r.ownerSaw == -1 && r.ownerAt >= 2000, "the 1.5 s finger press is delivered on release");
}

static void test_filter_line_twitch_no_click() {
    printf("filter: a 300 ms line twitch, marked while a host connects, yields no click\n");
    // N1MM connecting to the classic with both lines 'Always On' (bench 2026-10-17): the line twitched,
    // ClickButton reported a single FN click after the release, and the encoder went to volume.
    ClickButton b(0); HeldLineFilter f;
    int seen = 0;
    for (unsigned long t = 0; t <= 1500; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 100 && t < 400) ? 0 : 1;
        b.Update();
        if (b.depressed) f.markLine();
        b.clicks = f.filter(b.clicks, b.depressed);
        if (b.clicks) { seen = b.clicks; break; }
    }
    check(seen == 0, "no click delivered");
    int later = 0;                                   // and a real click right afterwards still counts
    for (unsigned long t = 1600; t <= 2600; t += 5) {
        sim_millis_value = t;
        sim_pin_value = (t >= 1700 && t < 1800) ? 0 : 1;
        b.Update();
        int c = f.filter(b.clicks, b.depressed);
        if (c) { later = c; break; }
    }
    check(later == 1, "a finger's click a moment later is delivered");
}

int main() {
    test_long_click_is_delivered_once();
    test_long_click_still_held();
    test_long_click_late_caller();
    test_short_click_unaffected();
    test_double_click_unaffected();
    test_long_then_new_press();
    test_second_long_press();
    test_filter_finger_long_press();
    test_filter_line_hold_ignored();
    test_filter_down_at_reset();
    test_filter_short_and_double();
    test_filter_written_back_once();
    test_filter_short_line_marked();
    test_filter_mark_while_up_is_harmless();
    test_filter_line_twitch_no_click();
    printf("\n%s\n", failures ? "FAILURES" : "all ok");
    return failures ? 1 : 0;
}

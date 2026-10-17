#ifndef ClickButton_H_
#define ClickButton_H_

#if (ARDUINO <  100)
#include <WProgram.h>
#else
#include <Arduino.h>
#endif


//#define CLICKBTN_PULLUP HIGH


class ClickButton
{
  public:
    ClickButton(uint8_t buttonPin);
//    ClickButton(uint8_t buttonPin, boolean active);
//    ClickButton(uint8_t buttonPin, boolean active, boolean internalPullup);
    void Update();
    int clicks;                   // button click counts to return
    boolean depressed;            // the currently debounced button (press) state (presumably it is not sad :)
    long debounceTime;
    long multiclickTime;
    long longClickTime;
    uint8_t pin;                  // Arduino pin connected to the button - now public to chaneg assignement at runtime
    boolean activeHigh;          // Type of button: Active-low = 0 or active-high = 1

  private:
    boolean _btnState;            // Current appearant button state
    boolean _lastState;           // previous button reading
    int _clickCount;              // Number of button clicks within multiclickTime milliseconds
    long _lastBounceTime;         // the last time the button input pin was toggled, due to noise or a press
    boolean _longClickPending;    // a long click was reported by the previous Update() and has now been seen
};

// A button that a serial line can hold down. On the classic M32, FN is GPIO0, which the CP2102's
// auto-reset circuit pulls low while DTR is on and RTS off - what WinKeyer loggers such as CQRLOG
// and flwkey do for as long as they have the port open (devdocs/winkeyer/BENCH.md). Feed it the
// button's clicks and debounced state right after each Update(); it returns the clicks to act on.
//
// ClickButton reports a long press after longClickTime while the button is still held - before a
// held line can be told from a finger. So a long press is held back and delivered at the release;
// a hold that outlasts lineMs, or that is already there when the filter is (re)started, is a line
// and does nothing until it is released. Short and multiple clicks, reported after the release
// anyway, pass unchanged. A caller that writes the result back into ClickButton::clicks must clear
// it before handing control to code that polls the same button (it would see the -1 again).
//
// The duration rule alone misses a line that is held only briefly: N1MM, opening the classic's
// port, passes through "DTR on, RTS off" for 1-3 s and then switches both lines off - a press that
// ends like a finger's (bench, 2026-10-17). So the caller, which can see a host connecting, calls
// markLine() while that happens: the press under way then counts as a line, however short. A line
// press yields no click either: ClickButton reports a short press's click only after the release
// (multi-click window), and a brief twitch of the line turned into an FN click on the bench.
class HeldLineFilter
{
  public:
    explicit HeldLineFilter(unsigned long lineMs = 3000) : _lineMs(lineMs) {}
    void reset() { _fresh = true; }     // judge afresh: a press already down counts as a line
    void markLine() { _lineHint = true; }   // a host is connecting: the press under way is a line
    int filter(int clicks, boolean down);
  private:
    unsigned long _lineMs;
    boolean _fresh = true, _prevDown = false, _ignore = false, _pendingLong = false, _lineHint = false;
    unsigned long _downSince = 0;
    unsigned long _quietUntil = 0;      // after a line press: swallow the click ClickButton reports for it
};

#endif /* #ifndef ClickButton_H_ */
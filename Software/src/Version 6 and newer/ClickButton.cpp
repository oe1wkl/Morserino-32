/*    ClickButton
 Based on an Arduino librery by raronzen@gmail.com that decodes multiple clicks on one button.
 Also copes with long clicks and click-and-hold.
 
 Usage: ClickButton buttonObject(pin [LOW/HIGH, [CLICKBTN_PULLUP]]);
 
  where LOW/HIGH denotes active LOW or HIGH button (default is LOW)
  CLICKBTN_PULLUP is only possible with active low buttons.
 

 Returned click counts:

   A positive number denotes the number of (short) clicks after a released button
   A negative number denotes the number of "long" clicks
 
 Copyright (C) 2010,2012, 2013 raron

 GNU GPLv3 license
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

Contact: raronzen@gmail.com

*/

#include "ClickButton.h"


ClickButton::ClickButton(uint8_t buttonPin)
{
  pin            = buttonPin;
  activeHigh    = LOW;           // Assume active-low button
  _btnState      = !activeHigh;  // initial button state in active-high logic
  _lastState     = _btnState;
  _clickCount    = 0;
  clicks         = 0;
  _longClickPending = false;
  depressed      = false;
  _lastBounceTime= 0;
  debounceTime   = 20;            // Debounce timer in ms
  multiclickTime = 250;           // Time limit for multi clicks
  longClickTime  = 1000;          // time until long clicks register
#ifdef INTERNAL_PULLUP
  pinMode(pin, INPUT_PULLUP);
#else
  pinMode(pin, INPUT);
#endif
}

/*
ClickButton::ClickButton(uint8_t buttonPin, boolean activeType)
{
  pin            = buttonPin;
  _activeHigh    = activeType;
  _btnState      = !_activeHigh;  // initial button state in active-high logic
  _lastState     = _btnState;
  _clickCount    = 0;
  clicks         = 0;
  depressed      = 0;
  _lastBounceTime= 0;
  debounceTime   = 20;            // Debounce timer in ms
  multiclickTime = 250;           // Time limit for multi clicks
  longClickTime  = 1000;          // time until long clicks register
  pinMode(pin, INPUT);
}

ClickButton::ClickButton(uint8_t buttonPin, boolean activeType, boolean internalPullup)
{
  pin            = buttonPin;
  _activeHigh    = activeType;
  _btnState      = !_activeHigh;  // initial button state in active-high logic
  _lastState     = _btnState;
  _clickCount    = 0;
  clicks         = 0;
  depressed      = 0;
  _lastBounceTime= 0;
  debounceTime   = 20;            // Debounce timer in ms
  multiclickTime = 250;           // Time limit for multi clicks
  longClickTime  = 1000;          // time until "long" click register
  pinMode(pin, INPUT);
  // Turn on internal pullup resistor if applicable
  if (_activeHigh == LOW && internalPullup == CLICKBTN_PULLUP) digitalWrite(pin,HIGH);
}

*/

void ClickButton::Update()
{
  long now = (long)millis();      // get current time

  // A long click is reported while the button is still HELD (see the long-click
  // branch below), so unlike a short click - which is only reported once the
  // release has settled - the press it belongs to is not over when the caller
  // acts on it. Callers act on it by leaving whatever loop they were in, and
  // nothing calls Update() again until the code they returned to starts polling.
  // By then the release edge has restarted _lastBounceTime, which makes the
  // long-click branch skip itself (its timer is young again) while the release
  // branch is not yet old enough to fire - so `clicks` stayed frozen at the long
  // click for up to multiclickTime, and the next loop to poll this button read
  // the same press a second time. That is one root cause behind a whole family
  // of reported faults: leaving a mode also climbing several menu levels, a
  // single long press skipping a menu level, a game screen falling back two
  // steps at once.
  //
  // So: a long click is visible from the Update() that detects it until the next
  // Update(), and no longer. That is not a new rule - it is what already happened
  // whenever the button stayed held, because the next Update() re-entered the
  // long-click branch and computed `0 - _clickCount` with _clickCount already
  // zeroed, i.e. 0. This only extends the same self-clearing to the release
  // window, where the branch cannot re-enter. Every caller in this firmware
  // reads `clicks` immediately after its own Update(), which is exactly the
  // window this preserves; the per-site `clicks = 0` swallows dotted around the
  // menu, the games and the QSO Bot become belt and braces rather than the fix.
  if (_longClickPending)
  {
    clicks = 0;
    _longClickPending = false;
  }

  _btnState = digitalRead(pin);  // current appearant button state

  // Make the button logic active-high in code
  if (!activeHigh) _btnState = !_btnState;

  // If the switch changed, due to noise or a button press, reset the debounce timer
  if (_btnState != _lastState) _lastBounceTime = now;


  // debounce the button (Check if a stable, changed state has occured)
  if (now - _lastBounceTime > debounceTime && _btnState != depressed)
  {
    depressed = _btnState;
    if (depressed) _clickCount++;
  }

  // If the button released state is stable, report nr of clicks and start new cycle
  if (!depressed && (now - _lastBounceTime) > multiclickTime)
  {
    // positive count for released buttons
    clicks = _clickCount;
    _clickCount = 0;
  }

  // Check for "long click"
  if (depressed && (now - _lastBounceTime > longClickTime))
  {
    // negative count for long clicks
    clicks = 0 - _clickCount;
    _clickCount = 0;
    // Only arm the one-shot when there is actually something to report. While the
    // button goes on being held this branch runs again every Update() and computes
    // 0 - 0, which must not re-arm it: that would clear a genuine click reported by
    // some later Update() before its caller ever saw it.
    if (clicks) _longClickPending = true;
  }

  _lastState = _btnState;
}

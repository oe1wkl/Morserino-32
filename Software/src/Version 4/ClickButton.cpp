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

// extern void DEBUG (String s);


ClickButton::ClickButton(uint8_t buttonPin)
{
  pin            = buttonPin;
  activeHigh    = LOW;           // Assume active-low button
  _btnState      = !activeHigh;  // initial button state in active-high logic
  _lastState     = _btnState;
  _clickCount    = 0;
  clicks         = 0;
  depressed      = false;
  _lastBounceTime= 0;
  debounceTime   = 20;            // Debounce timer in ms
  multiclickTime = 250;           // Time limit for multi clicks
  longClickTime  = 1000;          // time until long clicks register
  pinMode(pin, INPUT);
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
  }

  _lastState = _btnState;
}

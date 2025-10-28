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
};

#endif /* #ifndef ClickButton_H_ */
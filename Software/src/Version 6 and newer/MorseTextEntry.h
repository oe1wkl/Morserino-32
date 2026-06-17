#ifndef MORSETEXTENTRY_H_
#define MORSETEXTENTRY_H_

/******************************************************************************************************************************
 *  MorseTextEntry — reusable, cross-variant on-screen text entry for the Morserino-32.
 *
 *  Renders through MorseOutput's status/scroll lines, so it works unchanged on
 *  both the OLED (classic) and the TFT (Pocket). The user turns the ENCODER to
 *  pick a character, clicks the ENCODER to append it, clicks FN to delete the
 *  last character, and long-presses either button to finish.
 *
 *  The character set is a parameter, so the same widget can serve player call
 *  sign / name (now), and later WiFi credentials and CW keyer memories.
 *****************************************************************************************************************************/

#include <Arduino.h>

namespace MorseTextEntry
{
  // Common character sets (NUL-terminated). The encoder cycles through these.
  extern const char *const CHARSET_CALLSIGN;   // A-Z 0-9 /
  extern const char *const CHARSET_NAME;       // A-Z and space

  // Encoder-driven text entry. `result` must hold at least maxLen+1 bytes and
  // is pre-filled from `initial` (may be nullptr/empty). The current text fits
  // the 14-char OLED when maxLen is small (player identity = 8). Blocks until
  // the user long-presses to finish; returns the (NUL-terminated) result in
  // `result`. Resets the time-out on every input and services serialEvent().
  void enterText(const String &prompt, char *result, uint8_t maxLen,
                 const char *charSet, const char *initial);
}

#endif /* #ifndef MORSETEXTENTRY_H_ */

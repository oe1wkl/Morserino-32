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
  //
  // `noDuplicates`: when true, characters already present in `result` are
  // skipped while scrolling and a click on one is a no-op — for pickers where
  // each candidate makes sense at most once (e.g. a practice character set),
  // as opposed to free text (call sign, name) where repeats are normal.
  //
  // `displayXform`: optional, applied only to what's drawn on screen (the
  // candidate glyph and the accumulated text) — never to what's stored in
  // `result` or matched against `charSet`. Returns a String (not a single
  // char) so a source character can expand to a multi-char glyph, e.g. a
  // prosign tag like "<sk>" — important when charSet mixes plain letters
  // with uppercase single-char prosign codes that reuse the same letters
  // (a, n, k, s, e, b): rendering both as a bare uppercase letter would make
  // them visually indistinguishable. Use this for cosmetic display only
  // (e.g. the "Output Case" preference, prosign expansion); never to alter
  // which character gets appended, since case can be semantically meaningful
  // downstream (see CLAUDE.md).
  void enterText(const String &prompt, char *result, uint8_t maxLen,
                 const char *charSet, const char *initial,
                 boolean noDuplicates = false, String (*displayXform)(char) = nullptr);
}

#endif /* #ifndef MORSETEXTENTRY_H_ */

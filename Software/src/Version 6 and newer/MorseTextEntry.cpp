/******************************************************************************************************************************
 *  MorseTextEntry — see MorseTextEntry.h. Cross-variant encoder-driven text entry.
 *****************************************************************************************************************************/

#include "MorseTextEntry.h"
#include "morsedefs.h"          // Buttons::modeButton / volButton
#include "MorseOutput.h"
#include "MorsePreferences.h"   // checkEncoder(), checkShutDown(), serialEvent(), sidetoneVolume

namespace MorseTextEntry
{
  const char *const CHARSET_CALLSIGN = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
  const char *const CHARSET_NAME     = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
}

static boolean containsChar(const char *s, char c)
{
  for (; *s; ++s) if (*s == c) return true;
  return false;
}

// Renders one glyph through the optional display transform - never touches
// what gets stored or matched, only what gets drawn. A source char may
// expand to several drawn chars (e.g. a prosign tag).
static String displayGlyph(char c, String (*xform)(char))
{
  return xform ? xform(c) : String(c);
}

static String displayText(const char *s, String (*xform)(char))
{
  if (!xform) return String(s);
  String out; out.reserve(strlen(s));
  for (; *s; ++s) out += xform(*s);
  return out;
}

void MorseTextEntry::enterText(const String &prompt, char *result, uint8_t maxLen,
                               const char *charSet, const char *initial,
                               boolean noDuplicates, String (*displayXform)(char))
{
  const int charCount = strlen(charSet);
  uint8_t len = 0;
  if (initial) {
    while (initial[len] && len < maxLen) { result[len] = initial[len]; ++len; }
  }
  result[len] = '\0';
  int charIdx = 0;
  if (noDuplicates) {
    int guard = charCount;
    while (guard-- > 0 && containsChar(result, charSet[charIdx]))
      charIdx = (charIdx + 1) % charCount;
  }
  boolean needsRedraw = true;

  while (true) {
    if (needsRedraw) {
      needsRedraw = false;
      // Prompt on the status line; entered text with the live candidate in
      // brackets on the first scroll line; two short hint lines below. The
      // text fits the 14-char OLED for short fields (player identity = 8).
      MorseOutput::clearStatusLine();
      MorseOutput::printOnStatusLine(true, 0, prompt);
      MorseOutput::clearScrollLines();
      MorseOutput::printOnScroll(0, BOLD,    0, displayText(result, displayXform) + "[" + displayGlyph(charSet[charIdx], displayXform) + "]");
      MorseOutput::printOnScroll(1, REGULAR, 0, "click = add");
      MorseOutput::printOnScroll(2, REGULAR, 0, "FN=del hold=ok");
      MorseOutput::refreshDisplay();
    }

    int t = checkEncoder();
    if (t) {                                        // turn: pick a character
      MorseOutput::resetTOT();
      MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
      charIdx = (charIdx + t + charCount) % charCount;
      if (noDuplicates) {
        int guard = charCount;
        while (guard-- > 0 && containsChar(result, charSet[charIdx]))
          charIdx = (charIdx + t + charCount) % charCount;
      }
      needsRedraw = true;
    }

    Buttons::modeButton.Update();
    if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
    if (Buttons::modeButton.clicks == 1 && len < maxLen &&           // click: append
        !(noDuplicates && containsChar(result, charSet[charIdx]))) {
      result[len++] = charSet[charIdx];
      result[len] = '\0';
      charIdx = 0;
      if (noDuplicates) {
        int guard = charCount;
        while (guard-- > 0 && containsChar(result, charSet[charIdx]))
          charIdx = (charIdx + 1) % charCount;
      }
      needsRedraw = true;
    }
    if (Buttons::modeButton.clicks == -1) { result[len] = '\0'; return; }   // long press: done

    Buttons::volButton.Update();
    if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
    if (Buttons::volButton.clicks == 1 && len > 0) {         // FN: backspace
      result[--len] = '\0';
      charIdx = 0;
      needsRedraw = true;
    }
    if (Buttons::volButton.clicks == -1) { result[len] = '\0'; return; }    // long press: done

    checkShutDown(false);
    serialEvent();
    delay(20);
  }
}

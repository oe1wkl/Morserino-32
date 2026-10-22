/******************************************************************************************************************************
 *  MorseTextEntry — see MorseTextEntry.h. Cross-variant encoder-driven text entry.
 *****************************************************************************************************************************/

#include "MorseTextEntry.h"
#include "morsedefs.h"          // Buttons::modeButton / volButton
#include "MorseOutput.h"
#include "MorseVoice.h"      // Accessibility Edition: this screen is otherwise silent
#include "MorsePreferences.h"   // checkEncoder(), checkShutDown(), serialEvent(), sidetoneVolume

namespace MorseTextEntry
{
  const char *const CHARSET_CALLSIGN = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
  const char *const CHARSET_NAME     = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
  // Lower case on purpose: in this firmware an upper case single character is a prosign code, and the
  // Accessibility Edition would speak it as one. No space either - it has no clip, and the protocol trims it.
  const char *const CHARSET_PASSPHRASE = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+";
}

// Width of the entry line. The candidate in brackets is appended to the text, so a long field (pass phrase,
// practice set) would run off the line; show its tail instead, marked with '<'. Short fields are unaffected.
#ifdef CONFIG_TFT
static const uint8_t ENTRY_LINE_WIDTH = 21;   // IntelOneMono at the large scroll font
#else
static const uint8_t ENTRY_LINE_WIDTH = 14;   // OLED
#endif

static String fitEntryLine(const String &text, const String &candidate)
{
  String line = text + "[" + candidate + "]";
  if (line.length() <= ENTRY_LINE_WIDTH) return line;
  int keep = ENTRY_LINE_WIDTH - 1 - (candidate.length() + 2);    // room for '<' and "[c]"
  if (keep < 0) keep = 0;
  return "<" + text.substring(text.length() - keep) + "[" + candidate + "]";
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

// Accessibility Edition: without this the whole widget is mute, and it is the one screen a
// blind operator has to drive character by character - Call Sign, Op Name and Practice Set
// all come through here. Everything below is composed from atoms the voice pack already has
// (the prompt, the numbers, "characters", and the per-character phonetics), so no new clips
// are owed. MorseVoice::* are no-ops on builds without CONFIG_AUDIO_A11Y, so the calls stay
// unconditional; only tick() needs the guard, since it is what drives playback and this loop
// blocks outside loop().
//
// announceMoreChar() wants the RAW character, not the displayXform()'d glyph: prosign codes
// expand to "pro sign" plus two phonetics, which is what makes them distinguishable by ear
// from the plain letter that shares their code.
//
// Call Sign and Op Name are entered in upper case, and there an upper-case character is a LETTER. The voice pack
// keys upper-case single characters as prosign codes, so voiced raw, C D F G I J L ... were silent and A B E K N S
// were announced as prosigns (tester report, 2026-10). voiceAsLetters speaks the lower-case letter instead, and
// the space (Op Name) as the word.
static void sayCandidate(const char *charSet, int charIdx, boolean voiceAsLetters) {
    char c = charSet[charIdx];
    if (voiceAsLetters) {
        if (c == ' ') { MorseVoice::announceMore("space"); return; }
        c = (char) tolower((unsigned char) c);
    }
    MorseVoice::announceMoreChar(String(c));
}

void MorseTextEntry::enterText(const String &prompt, char *result, uint8_t maxLen,
                               const char *charSet, const char *initial,
                               boolean noDuplicates, String (*displayXform)(char),
                               boolean voiceAsLetters)
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

  // Heading first, then the character under the cursor. The prompt carries a trailing colon
  // for the display; the voice pack is keyed on the bare label ("Practice Set", "Call Sign",
  // "Op Name" - the last one spoken as "Operator Name" via spoken_overrides.tsv).
  String heading = prompt;
  if (heading.endsWith(":")) heading.remove(heading.length() - 1);
  MorseVoice::announce(heading);
  sayCandidate(charSet, charIdx, voiceAsLetters);

  while (true) {
    if (needsRedraw) {
      needsRedraw = false;
      // Prompt on the status line; entered text with the live candidate in
      // brackets on the first scroll line; two short hint lines below. The
      // text fits the 14-char OLED for short fields (player identity = 8).
      MorseOutput::clearStatusLine();
      MorseOutput::printOnStatusLine(true, 0, prompt);
      MorseOutput::clearScrollLines();
      MorseOutput::printOnScroll(0, BOLD,    0, fitEntryLine(displayText(result, displayXform), displayGlyph(charSet[charIdx], displayXform)));
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
      MorseVoice::announce("");            // start a fresh utterance: one character per detent
      sayCandidate(charSet, charIdx, voiceAsLetters);
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
      MorseVoice::announce(String(len));   // "3 characters, Yankee" - the count confirms the
      MorseVoice::announceMore("characters");  // add went in, the character says where we are now
      sayCandidate(charSet, charIdx, voiceAsLetters);
    }
    if (Buttons::modeButton.clicks == -1) { result[len] = '\0'; return; }   // long press: done

    Buttons::volButton.Update();
    if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
    if (Buttons::volButton.clicks == 1 && len > 0) {         // FN: backspace
      result[--len] = '\0';
      charIdx = 0;
      needsRedraw = true;
      MorseVoice::announce(String(len));
      MorseVoice::announceMore("characters");
      sayCandidate(charSet, charIdx, voiceAsLetters);
    }
    if (Buttons::volButton.clicks == -1) { result[len] = '\0'; return; }    // long press: done

#ifdef CONFIG_AUDIO_A11Y
    MorseVoice::tick();
#endif
    checkShutDown(false);
    serialEvent();
    delay(20);
  }
}

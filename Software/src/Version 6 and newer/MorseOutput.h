#ifndef MORSEOUTPUT_H_
#define MORSEOUTPUT_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

#include "Arduino.h"

#ifndef CONFIG_TFT
  #define NoOfLines 15
  #define NoOfVisibleLines 3
#else
  #define NoOfLines 18
  #ifdef CONFIG_SCROLL_FONT_SIZE
    #define NoOfVisibleLinesNormal 4
    #define NoOfVisibleLinesSmall 5   // one more line fits at the small scroll font's genuinely tighter (ASCII-only) line pitch
  #else
    #define NoOfVisibleLines 4
  #endif
#endif

enum FONT_ATTRIB
{
    VOID, REGULAR, BOLD, INVERSE_REGULAR, INVERSE_BOLD, MORSE_REGULAR, MORSE_BOLD, OK_RESULT, ERR_RESULT
};

// CW transcription weight convention: BOLD = CW the operator must copy (echo
// prompt, received Trx, decoded audio, QSO Bot transmission); REGULAR = the
// operator's own keying and the passive CW Generator read-along.

extern uint8_t scrollTop;

#ifdef CONFIG_SCROLL_FONT_SIZE
// runtime visible-line count (NoOfVisibleLinesNormal or NoOfVisibleLinesSmall,
// see toggleScrollFont()/applyScrollFontGeometry()); everywhere else (OLED,
// and TFT without the Font Size preference) NoOfVisibleLines stays the
// #define above, it never varies.
extern uint8_t NoOfVisibleLines;
#endif

namespace MorseOutput
{
  extern int8_t maxPos;
  extern int8_t relPos;
  extern volatile uint64_t TOTcounter;
  const int notes[] =
        {233, 262, 294, 311, 349, 392, 440, 466, 523, 587, 622, 698, 784, 880, 932};

  void initDisplay();
  void clearDisplay();
  void refreshDisplay();
  void decreaseBrightness();
  void setBrightness(uint8_t brightness);
#ifdef CONFIG_SCROLL_FONT_SIZE
  void toggleScrollFont();
  void applyScrollFontGeometry();
#endif
  void sleep();
  void printOnStatusLine(boolean strong, uint8_t xpos, const String& string);
  void clearBuffer();
  void refreshScrollArea(int relPos);
  void refreshScrollLine(int bufferLine, int displayLine);
  /// returns the width of the printed string in pixels - uint16_t, not uint8_t:
  /// a full scroll line is 306 px on the TFT and callers divide the result by the
  /// character width to get a column count (see refreshScrollLine).
  /// forceNormal overrides the Font Size preference to always draw the normal
  /// (large) size - for text that needs a glyph the small font doesn't have,
  /// e.g. the boot splash's copyright "(c)" (TFT's small font is ASCII-only,
  /// see IntelOneMono12ptAscii.h). Ignored when small is also true.
  uint16_t printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, const String& mystring, boolean small = false, boolean forceNormal = false);
  void printToScroll(FONT_ATTRIB style, const String& text, boolean autoflush, boolean scroll);
  void printToScroll_internal(FONT_ATTRIB style, const String& text, boolean scroll);
  boolean wordNeedsWrap(uint16_t wordLen);
  void clearScrollLines();
  void clearLine(uint8_t line);
  void clearScrollBuffer();
  void clearScroll();
  void flushScroll(boolean scroll);
  void newLine(boolean scroll);
  void displayScrollBar(boolean visible);
  void displayBatteryStatus(int v);
  void displayEmptyBattery(void (*f)());
  void displayVolume(boolean speedsetting, uint8_t volume);
  void updateSMeter(int rssi);
  void drawInputStatus(boolean on);
  uint8_t getScrollTop();
  void clearStatusLine();
  void showVolumeBar(uint16_t mini, uint16_t maxi);
  void drawVolumeCtrl(boolean inverse, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t volume);
  void showVolumeScope(uint16_t mini, uint16_t maxi);
  void dispLoraLogo();
  void dispWifiLogo();
#ifdef CONFIG_MCP73871
  extern uint8_t  ppCurrentState;
  extern uint8_t  ppPreviousState;
  extern bool     batteryDisplayDirty;
  extern bool     batteryIconVisible;
 
  uint8_t getPowerpathState();          // read raw pin state
  void checkPowerpathState();           // fast: check ISR flag, update state
  void updateBatteryDisplay();          // slow: measure + redraw (menu/prefs only)
  void drawBatteryIcon(uint8_t pps, uint8_t bars);
  void clearBatteryIcon();
  void resetPowerpathDisplay();
#endif

#ifdef CONFIG_TFT
  void dispM32Logo();
  void setTheme (uint8_t theme);
  void testFontLayout();
#endif

  void resetTOT();

  void soundSetup();
#ifdef CONFIG_TLV320AIC3100
  void soundEventHandler();
  void soundSetVolume(uint8_t v); // v = 0 - 19
  //float calcVolume(uint8_t v);

#endif
  void soundSuspend();
#ifdef CONFIG_SOUND_I2S
  void setSidetoneEnvelope(uint8_t prefValue);   // prefValue 0..8 maps to 1..9 ms attack/release
#endif
  void pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut);
  void pwmNoTone(unsigned int volume);
  void pwmClick(unsigned int volume);
  void soundSignalOK();
  void soundSignalERR();
  bool voiceStart(const char* path);      // V9.0 a11y: start async clip (no-op without I2S)
  bool voiceService();                    //   poll from loop; true while a clip is playing
  void voiceStop();                       //   interrupt the current clip
}

#endif /* #ifndef MORSEOUTPUT_H_ */
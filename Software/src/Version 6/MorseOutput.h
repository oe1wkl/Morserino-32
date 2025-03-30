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

#define NoOfLines 15

enum FONT_ATTRIB
{
    VOID, REGULAR, BOLD, INVERSE_REGULAR, INVERSE_BOLD
};

const FONT_ATTRIB FONT_INCOMING = REGULAR;
const FONT_ATTRIB FONT_OUTGOING = BOLD;

extern uint8_t scrollTop;

namespace MorseOutput
{
  extern const int8_t maxPos;
  extern int8_t relPos;
  extern volatile uint64_t TOTcounter;
  const int notes[] =
        {233, 262, 294, 311, 349, 392, 440, 466, 523, 587, 622, 698, 784, 880, 932};

  void initDisplay();
  void clearDisplay();
  void refreshDisplay();
  void decreaseBrightness();
  void setBrightness(uint8_t brightness);
  void sleep();
  void printOnStatusLine(boolean strong, uint8_t xpos, String string);
  void clearBuffer();
  void refreshScrollArea(int relPos);
  void refreshScrollLine(int bufferLine, int displayLine);
  uint8_t printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring, boolean small = false);
  void printToScroll(FONT_ATTRIB style, String text, boolean autoflush, boolean scroll);
  void printToScroll_internal(FONT_ATTRIB style, String text, boolean scroll);
  void clearThreeLines();
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

  void resetTOT();

  void soundSetup();
#ifdef CONFIG_TLV320AIC3100
  void soundEventHandler();
#endif
  void soundSuspend();
  void pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut);
  void pwmNoTone(unsigned int volume);
  void pwmClick(unsigned int volume);
  void soundSignalOK();
  void soundSignalERR();
}

#endif /* #ifndef MORSEOUTPUT_H_ */
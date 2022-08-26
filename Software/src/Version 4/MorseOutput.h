#ifndef MORSEDISPLAY_H_
#define MORSEDISPLAY_H_

#include "Arduino.h"
#include "heltec.h"

#define NoOfLines 15

enum FONT_ATTRIB
{
    REGULAR, BOLD, INVERSE_REGULAR, INVERSE_BOLD
};

const FONT_ATTRIB FONT_INCOMING = REGULAR;
const FONT_ATTRIB FONT_OUTGOING = BOLD;

namespace MorseOutput
{ 
  extern const int8_t maxPos;
  extern int8_t relPos;
  extern volatile uint64_t TOTcounter;
  const int notes[] =
        {0, 233, 262, 294, 311, 349, 392, 440, 466, 523, 587, 622, 698, 784, 880, 932};

  
  void clearDisplay();
  void decreaseBrightness();
  void sleep();
  void printOnStatusLine(boolean strong, uint8_t xpos, String string);
  void clearBuffer();
  void refreshScrollArea(int relPos);
  void refreshScrollLine(int bufferLine, int displayLine);
  uint8_t printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring);
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
  void clearStatusLine();
  void showVolumeBar(uint16_t mini, uint16_t maxi);
  void drawVolumeCtrl(boolean inverse, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t volume);
  void showVolumeScope(uint16_t mini, uint16_t maxi);
  void dispLoraLogo();
  void dispWifiLogo();

  void resetTOT();

  void soundSetup();
  void pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut);
  void pwmNoTone();
  void pwmClick(unsigned int volume);
  void soundSignalOK();
  void soundSignalERR();
}

#endif /* MORSEDISPLAY_H_ */

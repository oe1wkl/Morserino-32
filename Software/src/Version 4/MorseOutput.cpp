/******************************************************************************************************************************
    morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module **
    Copyright (C) 2018 ff.  Willi Kraml, OE1WKL                                                                             **

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program.
    If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

/// This mpodule contrains functions for output on the display, on the USB serial output, on the speaker and on line out
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////// New scrolling display

/// circular buffer: 14 chars by NoOfLines lines (bottom 3 are visible)
#define NoOfLines 15
#define NoOfCharsPerLine 14
#define SCROLL_TOP 15
#define LINE_HEIGHT 16
#define C_WIDTH 9

#include <Arduino.h>
#include "heltec.h"
#include "MorseOutput.h"
#include "morsedefs.h"
#include "wklfonts.h"
#include "MorsePreferences.h"

using namespace MorseOutput;

char textBuffer[NoOfLines][2 * NoOfCharsPerLine + 1]; /// we need extra room for style markers (FONT_ATTRIB stored as characters to toggle on/off the style within a line)
/// and 0 terminator


uint8_t linePointer = 0;    /// defines the current bottom line
uint8_t bottomLine = 0;

const int8_t MorseOutput::maxPos = NoOfLines - 3;
int8_t MorseOutput::relPos = MorseOutput::maxPos;

#define lora_width 6        /// a simple logo that shows when we operate with loRa, stored in XBM format
#define lora_height 11
static unsigned char lora_bits[] = {
  0x0f, 0x18, 0x33, 0x24, 0x29, 0x2b, 0x29, 0x24, 0x33, 0x18, 0x0f
};

#define wifi_width 6        /// a simple logo that shows when we operate with WiFi, stored in XBM format
#define wifi_height 11
static unsigned char wifi_bits[] = {
   0x00, 0x08, 0x10, 0x24, 0x29, 0x2b, 0x29, 0x24, 0x10, 0x08, 0x00 };

volatile uint64_t MorseOutput::TOTcounter;                       // holds millis for Time-Out Timer

String printToScroll_buffer = "";
FONT_ATTRIB printToScroll_lastStyle = REGULAR;

/////////////////////// parameters for LF tone generation and  HF (= vol ctrl) PWM
int toneFreq = 500 ;
int toneChannel = 2;      // this PWM channel is used for LF generation, duty cycle is 0 (silent) or 50% (tone)
int lineOutChannel = 3;   // this PWM channel is used for line-out LF generation, duty cycle is 0 (silent) or 50% (tone)
int volChannel = 8;       // this PWM channel is used for HF generation, duty cycle between 1% (almost silent) and 100% (loud)
int pwmResolution = 10;
unsigned int volFreq = 32000; // this is the HF frequency we are using

const int  dutyCycleFiftyPercent =  511;                                                                             ;
const int  dutyCycleTwentyPercent = 250;
const int  dutyCycleZero = 0;


////// Display functions


void MorseOutput::clearDisplay()
{
  Heltec.display -> clear();
  Heltec.display -> display();
}

void MorseOutput::sleep()
{
  Heltec.display -> sleep();                //OLED sleep
}

void MorseOutput::decreaseBrightness() {
    switch (MorsePreferences::oledBrightness) {
      case 255: 
                MorsePreferences::oledBrightness = 127;
                break;
      case 127:
                MorsePreferences::oledBrightness = 63;
                break;
      case 63:
                MorsePreferences::oledBrightness = 28;
                break;
      case 28:
                MorsePreferences::oledBrightness = 9;
                break;
      default:
                MorsePreferences::oledBrightness = 255;
                break;
    }
    Heltec.display -> setBrightness(MorsePreferences::oledBrightness);
}


void MorseOutput::printToScroll(FONT_ATTRIB style, String text, boolean autoflush, boolean scroll) {
  boolean styleChanged = (style != printToScroll_lastStyle);
  boolean lengthExceeded = printToScroll_buffer.length() + text.length() > 10;
  //DEBUG("printToScroll String: " + text);
  if (styleChanged || lengthExceeded) {
    MorseOutput::flushScroll(scroll);
  }

  printToScroll_buffer += text;
  printToScroll_lastStyle = style;
  //DEBUG("printToScroll_buffer: " + printToScroll_buffer);
  if (autoflush || text.endsWith("\n")) {
    MorseOutput::flushScroll(scroll);
  }
}


void MorseOutput::clearBuffer() {
  MorseOutput::printToScroll(REGULAR, "", false, false);                     // clear the buffer first
}


void MorseOutput::clearScrollBuffer() {
  printToScroll_buffer = "";
  printToScroll_lastStyle = REGULAR;
}


void MorseOutput::flushScroll(boolean scroll) {
  uint8_t len = printToScroll_buffer.length();
  if (len != 0) {
    MorseOutput::printToScroll_internal(printToScroll_lastStyle, printToScroll_buffer, scroll);
    //DEBUG("printToScroll_buffer: " + printToScroll_buffer);
    clearScrollBuffer();
  }
}


/// store text in textBuffer, if it fits the screen line; otherwise scroll up, clear bottom buffer, store in new buffer, print on new line
void MorseOutput::printToScroll_internal(FONT_ATTRIB style, String text, boolean scroll) {

  static uint8_t pos = 0;
  static uint8_t screenPos = 0;
  static FONT_ATTRIB lastStyle = REGULAR;
  uint8_t l = text.length();
  if (l == 0) {                               // an empty string signals we should clear the buffer
    for (int i = 0; i < NoOfLines; ++i) {
      textBuffer[i][0] = (char) 0;                    /// empty this line
    }
    refreshScrollArea((NoOfLines + bottomLine - 2) % NoOfLines);
    pos = screenPos = 0;                                // reset the position pointers
    return;
  }

  int linebreak = text.endsWith("\n");
  if (linebreak) {
    text.replace("\n", "");
    l = text.length();
  }
  int textTooLong = (screenPos + l > NoOfCharsPerLine);

  if (textTooLong) {                 // we need to scroll up and start a new line
    MorseOutput::newLine(scroll);
    pos = 0;  screenPos = 0; lastStyle = REGULAR;
  }

  /// store text in buffer
  if (style == REGULAR) {
    memcpy(&textBuffer[bottomLine][pos], text.c_str(), l);  // copy the string of characters
    pos += l;
    textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
  } else {
    if (style == lastStyle)  {                                // not regular, but we have no change in style!
      pos -= 1;                                               // go one pos back to overwrite style marker
      memcpy(&textBuffer[bottomLine][pos], text.c_str(), l);  // copy the string of characters
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker
      textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
    } else {
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the beginning
      memcpy(&textBuffer[bottomLine][pos], text.c_str(), l);  // copy the string of characters
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the end
      textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
      lastStyle = style;                                      // remember new style flag
    }
  }

  if (relPos == maxPos) {                                     // we show the bottom lines on the screen, therefore we add the new stuff  immediately
    /// and send string to screen, avoiding refresh of complete line
    //DEBUG("relPos: " + String(relPos));
    MorseOutput::printOnScroll(2, style, screenPos, text);               // these characters are 9 pixels wide,
  }
  Heltec.display -> setFont(DialogInput_plain_15);;
  screenPos += (Heltec.display -> getStringWidth(text) / C_WIDTH);
  if (linebreak) {
    MorseOutput::newLine(scroll);
    pos = 0;  screenPos = 0; lastStyle = REGULAR;
  }
}


void MorseOutput::newLine(boolean scroll) {
  linePointer = (linePointer + 1) % NoOfLines;
  if (relPos && relPos != maxPos)
    --relPos;
  bottomLine = linePointer;
  textBuffer[bottomLine][0] = (char) 0;               /// and empty the bottom line

  if (relPos == 0 || relPos == maxPos)
    refreshScrollArea(relPos);
  if (scroll)
    MorseOutput::displayScrollBar(true);

}

/// refresh all three lines from buffer in scroll area;
void MorseOutput::refreshScrollArea(int relPos) {
  //int pos = ((bottomLine + relPos +1) % NoOfLines);
  refreshScrollLine(((bottomLine + relPos + 1) % NoOfLines), 0);          /// refresh all three lines
  refreshScrollLine(((bottomLine + relPos + 2) % NoOfLines), 1);
  refreshScrollLine(((bottomLine + relPos + 3) % NoOfLines), 2);
  Heltec.display -> display();
}

/// print a line to the screen
void MorseOutput::refreshScrollLine(int bufferLine, int displayLine) {
  String temp;
  temp.reserve(16);
  temp = "";
  char c;
  boolean irFlag = false;
  FONT_ATTRIB style = REGULAR;
  int pos = 0;
  uint8_t charsPrinted;

  Heltec.display -> setColor(BLACK);
  Heltec.display -> fillRect(0, SCROLL_TOP + displayLine * LINE_HEIGHT , 127, LINE_HEIGHT + 1); // black out the line on screen
  for (int i = 0; (c = textBuffer[bufferLine][i]) ; ++i) {
    if (c < 4)   {         /// a flag
      if (irFlag)         /// at the end of an emphasized string
      {
        charsPrinted = MorseOutput::printOnScroll(displayLine, style, pos, temp) / C_WIDTH;
        style = REGULAR; 
        pos += charsPrinted; 
        temp = ""; 
        irFlag = false;
      }
      else                /// at the beginning of an emphasized string
      {
        if (temp.length()) {
          charsPrinted = MorseOutput::printOnScroll(displayLine, style, pos, temp) / C_WIDTH;
          style = REGULAR; 
          pos += charsPrinted; 
          temp = "";
        }
        style = (FONT_ATTRIB) c; 
        irFlag = true;
      }
    }
    else {                /// normal character - add it to temp
      temp += c;
    }
  }
  if (temp.length())
    MorseOutput::printOnScroll(displayLine, style, pos, temp);
}


/// place a string onto the scroll area; line = 0, 1 or 2
uint8_t MorseOutput::printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring) {
  uint8_t w;

  if (how > BOLD)
    Heltec.display -> setColor(WHITE);
  else
    Heltec.display -> setColor(BLACK);

  if (how & BOLD)
    Heltec.display -> setFont(DialogInput_bold_15);
  else
    Heltec.display -> setFont(DialogInput_plain_15);

  Heltec.display -> setTextAlignment(TEXT_ALIGN_LEFT);

  // convert the array characters into a String object
  w = Heltec.display -> getStringWidth(mystring);
  Heltec.display -> fillRect(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT , w, LINE_HEIGHT + 1);

  if (how > BOLD)
    Heltec.display -> setColor(BLACK);
  else
    Heltec.display -> setColor(WHITE);

  Heltec.display -> drawString(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT, mystring);
  Heltec.display -> display();
  resetTOT();
  return w;         // we return the actual width of the output, in case of converted UTF8 characters
}

/// clear the three lines of the display area
void MorseOutput::clearThreeLines() {
  for (int i = 0; i < 3; ++i) {
    Heltec.display -> setColor(BLACK);
    Heltec.display -> fillRect(0, SCROLL_TOP + i * LINE_HEIGHT , 127, LINE_HEIGHT + 1);
    Heltec.display -> setColor(WHITE);
  }
}


void MorseOutput::clearScroll() {
  MorseOutput::printToScroll_internal(REGULAR, "", false);
  clearScrollBuffer();
  clearThreeLines();
}



void MorseOutput::drawVolumeCtrl (boolean inverse, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t volume) { // volume = 0-19
  int i = (width - 4) * volume / 19;
//DEBUG(String(i));
  if (inverse)
    Heltec.display -> setColor(BLACK);
  else
    Heltec.display -> setColor(WHITE);

  Heltec.display -> fillRect(x, y, width, height);

  if (!inverse)
    Heltec.display -> setColor(BLACK);
  else
    Heltec.display -> setColor(WHITE);

  Heltec.display -> fillRect(x + 2, y + 4, (width - 4) * volume / 19, height - 8);
  Heltec.display -> drawHorizontalLine(x + 2, y + height / 2, width - 4);
  Heltec.display -> display();
  resetTOT();
}


void MorseOutput::displayScrollBar(boolean visible) {          /// display a scroll bar on the right edge of the display
  const int l_bar = 3 * 49 / NoOfLines;

  if (visible) {
    Heltec.display -> setColor(WHITE);
    Heltec.display -> drawVerticalLine(127, 15, 49);
    Heltec.display -> setColor(BLACK);
    Heltec.display -> drawVerticalLine(127, 15 + (relPos * (49 - l_bar) / maxPos), l_bar);
  } else {
    Heltec.display -> setColor(BLACK);
    Heltec.display -> drawVerticalLine(127, 15, 49);
  }
  Heltec.display -> display();
  resetTOT();
}


///// display battery status as text and icon, parameter v: Voltage in mV
void MorseOutput::displayBatteryStatus(int v) {    /// v in millivolts!

  int a, b, c; String s; double d;
  d = v / 50;
  c = round(d) * 50;
  // DEBUG("v: " + String (v) + " c: " + String(c));
  a = c / 1000;
  b = (c - 1000 * a) / 100;
  if (v > 1000)
    s = "U: " + String(a) + "." + String(b) + " V";
  else
    s = "Unknown   ?";
  printOnScroll(2, REGULAR, 0, s);
  int w = constrain(v, 3100, 4100);
  w = map(w, 3100, 4100, 0, 31);
  Heltec.display -> drawRect(75, SCROLL_TOP + 2 * LINE_HEIGHT + 3, 35, LINE_HEIGHT - 4);
  Heltec.display -> drawRect(110, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 4, LINE_HEIGHT - 8);
  if (v > 1000)
    Heltec.display -> fillRect(77, SCROLL_TOP + 2 * LINE_HEIGHT + 5 , w, LINE_HEIGHT - 8);
  Heltec.display -> display();
}

void MorseOutput::displayEmptyBattery(void (*f)()) {                                /// display a warning and go to (return to) deep sleep
  Heltec.display -> clear();
  Heltec.display -> drawRect(10, 11, 95, 50);
  Heltec.display -> drawRect(105, 26, 15, 20);
  printOnScroll(1, INVERSE_BOLD, 4,  "EMPTY");
  delay(4000);
  (*f)();
}


/// display volume as a progress bar: vol = 1-100
void MorseOutput::displayVolume (boolean speedsetting, uint8_t volume) {
  drawVolumeCtrl(speedsetting ? false : true, 93, 0, 28, 15, volume);
  Heltec.display -> display();
}


////// S Meter for Trx modus

void MorseOutput::updateSMeter(int rssi) {

  static boolean wasZero = false;

  if (rssi == 0)
    if (wasZero)
      return;
    else {
      drawVolumeCtrl( false, 93, 0, 28, 15, 0);
      wasZero = true;
    }
  else {
    drawVolumeCtrl( false, 93, 0, 28, 15, constrain(map(rssi, -150, -20, 0, 100), 0, 100));
    wasZero = false;
  }
  Heltec.display -> display();
}

/// for morse decoder: show a suqare on status line when we detected a signal

void MorseOutput::drawInputStatus( boolean on) {
  if (on)
    Heltec.display -> setColor(BLACK);
  else
    Heltec.display -> setColor(WHITE);
  Heltec.display -> fillRect(1, 1, 13, 13);
  Heltec.display -> display();
}


void MorseOutput::dispLoraLogo() {                     /// display a small logo in the top right corner to indicate we operate with LoRa
  Heltec.display -> setColor(BLACK);
  Heltec.display -> drawXbm(121, 2, lora_width, lora_height, lora_bits);
  Heltec.display -> setColor(WHITE);
  Heltec.display -> display();
}

void MorseOutput::dispWifiLogo() {     // display a small logo in the top right corner to indicate we operate with WiFi
  Heltec.display -> setColor(BLACK);
  Heltec.display -> drawXbm(121, 2, wifi_width, wifi_height, wifi_bits);
  Heltec.display -> setColor(WHITE);
  Heltec.display -> display();
}

//////// Display the status line in CW Keyer Mode
//////// Layout of top line:
//////// Tch ul 15 WpM
//////// 0    5    0


void MorseOutput::printOnStatusLine(boolean strong, uint8_t xpos, String string) {    // place a string onto the status line; chars are 7px wide = 18 chars per line
  if (strong)
    Heltec.display -> setFont(DialogInput_bold_12);
  else
    Heltec.display -> setFont(DialogInput_plain_12);
  Heltec.display -> setTextAlignment(TEXT_ALIGN_LEFT);
  uint8_t w = Heltec.display -> getStringWidth(string);
  Heltec.display -> setColor(WHITE);
  Heltec.display -> fillRect(xpos * 7, 0 , w, 15);
  Heltec.display -> setColor(BLACK);
  Heltec.display -> drawString(xpos * 7, 0, string);
  Heltec.display -> setColor(WHITE);
  Heltec.display -> display();
  resetTOT();
}

void MorseOutput::clearStatusLine() {              // the status line is at the top, and inverted!
  Heltec.display -> setColor(WHITE);
  Heltec.display -> fillRect(0, 0, 128, 15);
  Heltec.display -> setColor(BLACK);

  Heltec.display -> display();
}

void MorseOutput::clearLine(uint8_t line) {                                              /// clear a line - display is done somewhere else!
  Heltec.display -> setColor(BLACK);
  Heltec.display -> fillRect(0, SCROLL_TOP + line * LINE_HEIGHT , 127, LINE_HEIGHT+1);
  Heltec.display -> setColor(WHITE);
}


void MorseOutput::showVolumeScope(uint16_t mini, uint16_t maxi)
{
  uint16_t a, b, c;
  a = map(mini, 0, 4096, 0, 125);
  b = map(maxi, 0, 4000, 0, 125);
  c = b - a;
  MorseOutput::clearLine(2);
  Heltec.display->drawRect(5, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 102, LINE_HEIGHT - 8);
  Heltec.display->drawRect(30, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 52, LINE_HEIGHT - 8);
  Heltec.display->fillRect(a, SCROLL_TOP + 2 * LINE_HEIGHT + 7, c, LINE_HEIGHT - 11);
  Heltec.display->display();
}


void MorseOutput::resetTOT() {       //// reset the Time Out Timer - we do this whenever there is a screen update
    MorseOutput::TOTcounter = millis();
}


/////// functions for audio output


void MorseOutput::soundSetup()
{
    // set up PWMs for tone generation
    ledcSetup(toneChannel, toneFreq, pwmResolution);
    ledcAttachPin(LF_Pin, toneChannel);

    ledcSetup(lineOutChannel, toneFreq, pwmResolution);
    ledcAttachPin(lineOutPin, lineOutChannel);                                    ////// change this for real version - no line out currntly

    ledcSetup(volChannel, volFreq, pwmResolution);
    ledcAttachPin(HF_Pin, volChannel);

    ledcWrite(toneChannel, 0);
    ledcWrite(lineOutChannel, 0);
}


void MorseOutput::pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut) { // frequency in Hertz, volume in range 0 - 19; we use 10 bit resolution
  //const uint16_t vol[] = {0,  1, 2, 3, 4, 6, 9, 14, 21, 31, 45, 70, 100, 145, 220, 320, 470, 720, 1023}; // 19 values
  const uint16_t vol[] =   {0,  1, 2, 4, 6, 9, 14, 21, 31, 45, 70, 100, 140, 200, 280, 390, 512, 680, 840, 1023}; // 20 values
  int i = constrain(volume, 0, 19);
  //DEBUG(String(vol[i]));
  //DEBUG(String(frequency));
  if (lineOut) {
      ledcWriteTone(lineOutChannel, (double) frequency);
      ledcWrite(lineOutChannel, dutyCycleFiftyPercent);
  }

  ledcWrite(volChannel, volFreq);
  ledcWrite(volChannel, vol[i]);
  ledcWriteTone(toneChannel, frequency);
 

  if (i == 0 ) 
      ledcWrite(toneChannel, dutyCycleZero);
  else 
  //else if (i > 2)
      ledcWrite(toneChannel, dutyCycleFiftyPercent);
  //else
      //ledcWrite(toneChannel, i*i*i + 4 + 2*i);          /// an ugly hack to allow for lower volumes on headphones
      //ledcWrite(toneChannel,400);
}


void MorseOutput::pwmNoTone() {      // stop playing a tone by changing duty cycle of the tone to 0
  ledcWrite(toneChannel, dutyCycleTwentyPercent);
  ledcWrite(lineOutChannel, dutyCycleTwentyPercent);
  delayMicroseconds(125);
  ledcWrite(toneChannel, dutyCycleZero);
  ledcWrite(lineOutChannel, dutyCycleZero);
}


void MorseOutput::pwmClick(unsigned int volume) {                        /// generate a click on the speaker
    if (!MorsePreferences::encoderClicks)
      return;
    pwmTone(572,volume > 4 ? volume-4 : volume, false);
    delay(3);
    //pwmTone(286,volume, false);
    pwmTone(1144,volume > 3 ? volume-3 : volume, false);

    delay(6);
    //pwmTone(143,volume-4, false);
    //delay(7);
    pwmNoTone();
}

void MorseOutput::soundSignalOK() {
    pwmTone(440, MorsePreferences::sidetoneVolume, false);
    delay(97);
    pwmNoTone();
    pwmTone(587, MorsePreferences::sidetoneVolume, false);
    delay(193);
    pwmNoTone();
}


void MorseOutput::soundSignalERR() {
    pwmTone(311, MorsePreferences::sidetoneVolume, false);
    delay(193);
    pwmNoTone();
}

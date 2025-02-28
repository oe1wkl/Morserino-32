/******************************************************************************************************************************
    morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module **
    Copyright (C) 2018-2025 ff.  Willi Kraml, OE1WKL                                                                        **

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program.
    If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

/// This module contains functions for output on the display, on the USB serial output, on the speaker and on line out
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////// New scrolling display

#ifndef CONFIG_DISPLAYWRAPPER
/// circular buffer: 14 chars by NoOfLines lines (bottom 3 are visible)
#define NoOfLines 15
#define NoOfCharsPerLine 14
#define SCROLL_TOP 15
#define LINE_HEIGHT 16
#define C_WIDTH 9
#else
#define NoOfCharsPerLine 512
#define LINE_HEIGHT display.getStringHeight("A")
#define C_WIDTH display.getStringWidth("A")
#define SCROLL_TOP display.getHeight()==64 ? 15 : 31
#endif

#include <Arduino.h>
#include "MorseOutput.h"
#include "morsedefs.h"
#include "MorsePreferences.h"

#ifndef CONFIG_DISPLAYWRAPPER
#include "wklfonts.h"
#include  "SSD1306Wire.h"
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL, GEOMETRY_128_64, I2C_TWO, 700000);
#else
#include "DisplayWrapper.h"
DisplayWrapper display;
#endif

#ifdef CONFIG_SOUND_I2S
#include "I2S_Sidetone.hpp"
I2S_Sidetone sidetone;
#endif

#ifdef CONFIG_WM8960
#include <SparkFun_WM8960_Arduino_Library.h>
WM8960 codec;
#endif

#ifdef CONFIG_TLV320AIC3100
#include "tlv320aic31xx_codec.h"
TLV320AIC31xx codec(&Wire);
#endif

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

void MorseOutput::initDisplay()
{
#ifdef OLED_RST
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
#endif
  display.init();
  if (! MorsePreferences::leftHanded)
    display.flipScreenVertically();
  display.clear();
}

void MorseOutput::clearDisplay()
{
  display.clear();
  display.display();
}

void MorseOutput::refreshDisplay()
{
  display.display();
}

void MorseOutput::sleep()
{
  display.displayOff(); // OLED sleep
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
    display.setBrightness(MorsePreferences::oledBrightness);
}

void MorseOutput::setBrightness(uint8_t brightness) {
  display.setBrightness(brightness);
}

void MorseOutput::printToScroll(FONT_ATTRIB style, String text, boolean autoflush, boolean scroll) {
  boolean styleChanged = (style != printToScroll_lastStyle);
  boolean lengthExceeded = printToScroll_buffer.length() + text.length() > 10;
  // DEBUG("printToScroll String: >" + text + "<");
  if (styleChanged || lengthExceeded) {
    // DEBUG("FLUSH!");
    MorseOutput::flushScroll(scroll);
  }

  printToScroll_buffer += text;
  printToScroll_lastStyle = style;
  // DEBUG("printToScroll_buffer: >" + printToScroll_buffer + "<");
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

  // for DEBUG
  //char c;
  //unsigned char ch;
  //
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
#ifdef CONFIG_DISPLAYWRAPPER
  int textTooLong = (screenPos + l > display.getWidth()/display.getStringWidth("A"));
#else
  int textTooLong = (screenPos + l > NoOfCharsPerLine);
#endif

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
      ///DEBUG("lastStyle :" + text);
      //pos -= 1;                                               // go one pos back to overwrite style marker
      memcpy(&textBuffer[bottomLine][pos], text.c_str(), l);  // copy the string of characters
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker
      textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
    } else {
      //DEBUG("NOTlastStyle :" + text);
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the beginning
      memcpy(&textBuffer[bottomLine][pos], text.c_str(), l);  // copy the string of characters
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the end
      textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
      lastStyle = style;                                      // remember new style flag
    }
  }
///// for debugging: show contents of text buffer
///DEBUG("Buffer:");
///for (int i  = 0; (c = textBuffer[bottomLine][i]); ++i) {
///  DEBUG(String( ch = c ) + " <");
/// }
/////

  if (relPos == maxPos) {                                     // we show the bottom lines on the screen, therefore we add the new stuff  immediately
    /// and send string to screen, avoiding refresh of complete line
    //DEBUG("relPos: " + String(relPos));
    MorseOutput::printOnScroll(2, style, screenPos, text);               // these characters are 9 pixels wide,
  }
  display.setFont(DialogInput_plain_15);;
  screenPos += (display.getStringWidth(text) / C_WIDTH);
  if (linebreak) {
    MorseOutput::newLine(scroll);
    pos = 0;  screenPos = 0; lastStyle = REGULAR;
  }
}


void MorseOutput::newLine(boolean scroll) {
  //DEBUG("Newline!");
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
  display.display();
}

/// print a line to the screen

void MorseOutput::refreshScrollLine(int bufferLine, int displayLine) {
  String temp;
  temp.reserve(16);
  temp = "";
  char c;
  unsigned char ch;
  boolean irFlag = false;
  FONT_ATTRIB style = REGULAR;
  int pos = 0;
  uint8_t charsPrinted;

  display.setColor(BLACK);
  display.fillRect(0, SCROLL_TOP + displayLine * LINE_HEIGHT , display.getWidth()-1, LINE_HEIGHT + 1); // black out the line on screen
  for (int i = 0; (c = textBuffer[bufferLine][i]) ; ++i) {
    // if (c == ' ') DEBUG("Blank!");
    if (c < 5)   {         /// a flag
      if (irFlag)         /// at the end of an emphasized string
      {
            //DEBUG("irFl>>" + temp + "<<");
        charsPrinted = MorseOutput::printOnScroll(displayLine, style, pos, temp) / C_WIDTH;
        style = REGULAR;
        pos += charsPrinted;
        temp = "";
        irFlag = false;
      }
      else                /// at the beginning of an emphasized string
      {
        if (temp.length()) {
              //DEBUG("noFl>>" + temp + "<<");

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
      // DEBUG("temp >>" + temp + "<<");
    }
  }

  if (temp.length())
    MorseOutput::printOnScroll(displayLine, style, pos, temp);
}


/// place a string onto the scroll area; line = 0, 1 or 2

uint8_t MorseOutput::printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring, boolean small) {
  uint8_t w;
// DEBUG("pos: " + String(xpos) + " >" + mystring + "<");
  if (how > BOLD)
    display.setColor(WHITE);
  else
    display.setColor(BLACK);
if (small) {
if (how & BOLD)
      display.setFont(DialogInput_bold_12);
    else
      display.setFont(DialogInput_plain_12);
  } else {
    if (how & BOLD)
      display.setFont(DialogInput_bold_15);
    else
      display.setFont(DialogInput_plain_15);
  }

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // convert the array characters into a String object
  w = display.getStringWidth(mystring);
  display.fillRect(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT , w, LINE_HEIGHT + 1);

  if (how > BOLD)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.drawString(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT, mystring);
  display.display();
  resetTOT();
  return w;         // we return the actual width of the output, in case of converted UTF8 characters
}

/*
uint8_t MorseOutput::printOnScrollSmall(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring) {
  uint8_t w;
// DEBUG("pos: " + String(xpos) + " >" + mystring + "<");
  if (how > BOLD)
    display.setColor(WHITE);
  else
    display.setColor(BLACK);

  if (how & BOLD)
    display.setFont(DialogInput_bold_12);
  else
    display.setFont(DialogInput_plain_12);

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // convert the array characters into a String object
  w = display.getStringWidth(mystring);
  display.fillRect(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT , w, LINE_HEIGHT + 1);

  if (how > BOLD)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.drawString(xpos * C_WIDTH, SCROLL_TOP + line * LINE_HEIGHT, mystring);
  display.display();
  resetTOT();
  return w;         // we return the actual width of the output, in case of converted UTF8 characters
}
*/


/// clear the three lines of the display area

void MorseOutput::clearThreeLines() {
  for (int i = 0; i < 3; ++i) {
    display.setColor(BLACK);
    display.fillRect(0, SCROLL_TOP + i * LINE_HEIGHT , display.getWidth()-1, LINE_HEIGHT + 1);
    display.setColor(WHITE);
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
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.fillRect(x, y, width, height);

  if (!inverse)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.fillRect(x + 2, y + 4, (width - 4) * volume / 19, height - 8);
  display.drawHorizontalLine(x + 2, y + height / 2, width - 4);
  display.display();
  resetTOT();
}


void MorseOutput::displayScrollBar(boolean visible) {          /// display a scroll bar on the right edge of the display
  const int l_bar = 3 * 49 / NoOfLines;

  if (visible) {
    display.setColor(WHITE);
    display.drawVerticalLine(display.getWidth()-1, 15, 49);
    display.setColor(BLACK);
    display.drawVerticalLine(display.getWidth()-1, 15 + (relPos * (49 - l_bar) / maxPos), l_bar);
  } else {
    display.setColor(BLACK);
    display.drawVerticalLine(display.getWidth()-1, 15, 49);
  }
  display.display();
  resetTOT();
}


///// display battery status as text and icon, parameter v: Voltage in mV

void MorseOutput::displayBatteryStatus(int v) {    /// v in millivolts!
  int a, b, c; String s; double d;
  s.reserve(20);
  d = v / 50;
  c = round(d) * 50;
  // DEBUG("v: " + String (v) + " c: " + String(c));
  a = c / 1000;
  b = (c - 1000 * a) / 100;
  if (v > 1000)
    s = "U: " + String(a) + "." + String(b) + " V";
  else
    s = "Unknown   ?";
#ifdef CONFIG_MCP73871
  uint8_t powerpath_state = (digitalRead(CONFIG_MCP_STAT1_PIN)<<2) + ( digitalRead(CONFIG_MCP_STAT2_PIN) << 1) + digitalRead(CONFIG_MCP_PG_PIN);
  switch (powerpath_state) {
      case 0:
        s = "Charger Fault!";
        break;
      case 2:
        s = "Charging";
        break;
      case 3:
        s = "Low Battery!";
        break;
      case 4:
        s = "Charge complete";
        break;
      case 6:
        s = "No Battery!";
        break;
      case 7:
        // Serial.println("Shutdown No Input Power Present");
        break;
      default:
        //Serial.print("Unknown State: ");
        break;
    }
#endif
  printOnScroll(2, REGULAR, 0, s);
#ifndef CONFIG_MCP73871
  int w = constrain(v, 3100, 4100);
  w = map(w, 3100, 4100, 0, 31);
  display.drawRect(75, SCROLL_TOP + 2 * LINE_HEIGHT + 3, 35, LINE_HEIGHT - 4);
  display.drawRect(110, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 4, LINE_HEIGHT - 8);
  if (v > 1000)
    display.fillRect(77, SCROLL_TOP + 2 * LINE_HEIGHT + 5 , w, LINE_HEIGHT - 8);
#endif
  display.display();
}

void MorseOutput::displayEmptyBattery(void (*f)()) {                                /// display a warning and go to (return to) deep sleep
  display.clear();
  display.drawRect(10, 11, 95, 50);
  display.drawRect(105, 26, 15, 20);
  printOnScroll(1, INVERSE_BOLD, 4,  "EMPTY");
  delay(4000);
  (*f)();
}


/// display volume as a progress bar: vol = 1-100
void MorseOutput::displayVolume (boolean speedsetting, uint8_t volume) {
  drawVolumeCtrl(speedsetting ? false : true, display.getWidth()-35, 0, 28, SCROLL_TOP, volume);
  display.display();
}


////// S Meter for Trx modus

void MorseOutput::updateSMeter(int rssi) {

  static boolean wasZero = false;

  if (rssi == 0)
    if (wasZero)
      return;
    else {
      drawVolumeCtrl( false, display.getWidth()-35, 0, 28, 15, 0);
      wasZero = true;
    }
  else {
    drawVolumeCtrl( false, display.getWidth()-35, 0, 28, 15, constrain(map(rssi, -150, -20, 0, 100), 0, 100));
    wasZero = false;
  }
  display.display();
}

/// for morse decoder: show a suqare on status line when we detected a signal

void MorseOutput::drawInputStatus( boolean on) {
  if (on)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);
  display.fillRect(1, 1, 13, 13);
  display.display();
}


void MorseOutput::dispLoraLogo() {                     /// display a small logo in the top right corner to indicate we operate with LoRa
  display.setColor(BLACK);
  display.drawXbm(display.getWidth()-7, 2, lora_width, lora_height, lora_bits);
  display.setColor(WHITE);
  display.display();
}

void MorseOutput::dispWifiLogo() {     // display a small logo in the top right corner to indicate we operate with WiFi
  display.setColor(BLACK);
  display.drawXbm(display.getWidth()-7, 2, wifi_width, wifi_height, wifi_bits);
  display.setColor(WHITE);
  display.display();
}

//////// Display the status line in CW Keyer Mode
//////// Layout of top line:
//////// Tch ul 15 WpM
//////// 0    5    0


void MorseOutput::printOnStatusLine(boolean strong, uint8_t xpos, String string) {    // place a string onto the status line; chars are 7px wide = 18 chars per line
  if (strong)
    display.setFont(DialogInput_bold_12);
  else
    display.setFont(DialogInput_plain_12);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  uint8_t w = display.getStringWidth(string);
  display.setColor(WHITE);
  display.fillRect(xpos * display.getStringWidth("A"), 0 , w, LINE_HEIGHT-1);
  display.setColor(BLACK);
  display.drawString(xpos * display.getStringWidth("A"), 0, string);
  display.setColor(WHITE);
  display.display();
  resetTOT();
}

void MorseOutput::clearStatusLine() {              // the status line is at the top, and inverted!
  display.setColor(WHITE);
  display.fillRect(0, 0, display.getWidth(), LINE_HEIGHT-1);
  display.setColor(BLACK);

  display.display();
}

void MorseOutput::clearLine(uint8_t line) {                                              /// clear a line - display is done somewhere else!
  display.setColor(BLACK);
  display.fillRect(0, SCROLL_TOP + line * LINE_HEIGHT , display.getWidth()-1, LINE_HEIGHT+1);
  display.setColor(WHITE);
}


void MorseOutput::showVolumeScope(uint16_t mini, uint16_t maxi)
{
  uint16_t a, b, c;
  a = map(mini, 0, 4096, 0, 125);
  b = map(maxi, 0, 4000, 0, 125);
  c = b - a;
  MorseOutput::clearLine(2);
  display.drawRect(5, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 102, LINE_HEIGHT - 8);
  display.drawRect(30, SCROLL_TOP + 2 * LINE_HEIGHT + 5, 52, LINE_HEIGHT - 8);
  display.fillRect(a, SCROLL_TOP + 2 * LINE_HEIGHT + 7, c, LINE_HEIGHT - 11);
  display.display();
}


void MorseOutput::resetTOT() {       //// reset the Time Out Timer - we do this whenever there is a screen update
    MorseOutput::TOTcounter = millis();
}


/////// functions for audio output


void MorseOutput::soundSetup()
{
#ifndef CONFIG_SOUND_I2S
  // set up PWMs for tone generation
  ledcSetup(toneChannel, toneFreq, pwmResolution);
  ledcAttachPin(LF_Pin, toneChannel);

  ledcSetup(lineOutChannel, toneFreq, pwmResolution);
  ledcAttachPin(lineOutPin, lineOutChannel);                                    ////// change this for real version - no line out currntly

  ledcSetup(volChannel, volFreq, pwmResolution);
  ledcAttachPin(HF_Pin, volChannel);

  ledcWrite(toneChannel, 0);
  ledcWrite(lineOutChannel, 0);
#else
#ifdef CONFIG_WM8960
#pragma message ("WM8960 CODEC")
	Wire.begin(CONFIG_WM8960_SDA, CONFIG_WM8960_SCL);
	if (codec.begin() == false) //Begin communication over I2C
	{
		DEBUG("The WM8960 did not respond. Please check wiring.");
	} else {
    DEBUG("WM8960 is connected properly.");
    // General setup needed
    codec.enableVREF();
    codec.enableVMID();

#ifdef CONFIG_DECODER_I2S
    // configure mic input for decoder

    // link ADCLRC with DACLRC
    codec.setALRCGPIO();

    // Setup signal flow to the ADC

    codec.enableLMIC();
    codec.enableRMIC();

    // Connect from INPUT1 to "n" (aka inverting) inputs of PGAs.
    codec.connectLMN1();
    codec.connectRMN1();

    // Disable mutes on PGA inputs (aka INTPUT1)
    codec.disableLINMUTE();
    codec.disableRINMUTE();

    // Set pga volumes
    codec.setLINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)
    codec.setRINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)

    // Set input boosts to get inputs 1 to the boost mixers
    codec.setLMICBOOST(WM8960_MIC_BOOST_GAIN_0DB);
    codec.setRMICBOOST(WM8960_MIC_BOOST_GAIN_0DB);

    // Connect from MIC inputs (aka pga output) to boost mixers
    codec.connectLMIC2B();
    codec.connectRMIC2B();

    // Enable boost mixers
    codec.enableAINL();
    codec.enableAINR();

    // Connect LB2LO (booster to output mixer (analog bypass)
    codec.enableLB2LO();
    codec.enableRB2RO();

    // config mic input end
#endif

    // Connect from DAC outputs to output mixer
    codec.enableLD2LO();
    codec.enableRD2RO();

    // Enable output mixers
    codec.enableLOMIX();
    codec.enableROMIX();

    // CLOCK STUFF, These settings will get you 44.1KHz sample rate, and class-d
    // freq at 705.6kHz
    codec.enablePLL(); // Needed for class-d amp clock
    codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
    codec.setSMD(WM8960_PLL_MODE_FRACTIONAL);
    codec.setCLKSEL(WM8960_CLKSEL_PLL);
    codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
    codec.setBCLKDIV(4);
    codec.setDCLKDIV(WM8960_DCLKDIV_16);
    codec.setPLLN(7);
    codec.setPLLK(0x86, 0xC2, 0x26); // PLLK=86C226h
    codec.setWL(WM8960_WL_16BIT);

    codec.enablePeripheralMode();

    codec.enableAdcLeft();
    codec.enableAdcRight();
    codec.enableDacLeft();
    codec.enableDacRight();

    codec.disableDacMute();   // Default is "soft mute" on, so we must disable mute to make channels active
    //codec.enableLoopBack(); // Loopback sends ADC data directly into DAC
    codec.disableLoopBack();

    codec.enableSpeakers();

    codec.setDacLeftDigitalVolume(255);
    codec.setDacRightDigitalVolume(255);

    codec.setSpeakerVolumeDB(0.00);
    codec.setSpeakerDcGain(5);
    codec.setSpeakerAcGain(5);

    codec.enableHeadphones();
    codec.setHeadphoneVolumeDB(0.00);

    // headphone jack detection
    codec.enableHeadphoneJackDetect();
    codec.setHeadphoneJackDetectInput(WM8960_JACKDETECT_LINPUT3);
	}

#endif
#ifdef CONFIG_TLV320AIC3100
#pragma message ("TLV320AIC3100 CODEC")
    Wire.begin(CONFIG_TLV320AIC3100_SDA, CONFIG_TLV320AIC3100_SCL);
    if (!codec.begin()) {
      Serial.println("ERROR: TLV320AIC3100 Codec didn't respond via I2C!");
    } else {
      codec.setWordLength(AIC31XX_WORD_LEN_16BITS);

      // clock configuration for 44,1kHz 16bit stereo, the master clock will be derived via PLL from the i2s BCLK
      codec.setCLKMUX(AIC31XX_PLL_CLKIN_BCLK, AIC31XX_CODEC_CLKIN_PLL);
      codec.setPLL(1, 2, 32, 0); // uint8_t pll_p, uint8_t pll_r, uint8_t pll_j, uint16_t pll_d
      codec.setNDACVal(8);
      codec.setNDACPower(true);
      codec.setMDACVal(2);
      codec.setMDACPower(true);
      codec.setNADCVal(8);
      codec.setNADCPower(true);
      codec.setMADCVal(2);
      codec.setMADCPower(true);
      codec.setPLLPower(1);
      delay(20); // let PLL settle

      codec.setMicPGAEnable(true);
      codec.writeRegister(AIC31XX_MICPGAPI, 0x10); // MIC1RP 10k

      // enable internal clock for timer
      // this is required as we derive master clock from BCLK and hence have no MCLK input
      codec.modifyRegister(AIC31XX_TIMERDIVIDER, AIC31XX_TIMER_SELECT_MASK, 0);

      // enable headset detection and trigger interrupt 1 for headset events
      codec.enableHeadsetDetect();
      codec.setHSDetectInt1(true);

      // codec.writeRegister(AIC31XX_MICBIAS,11);
      codec.enableDAC();
      codec.setDACMute(false);
      codec.setDACVolume(20.0f,20.0f);
      codec.enableADC();
      codec.setADCGain(-12.0f);
      // run soundEventHandler once to mute/unmute HP/Spk depending on HS plug state
      soundEventHandler();

      // codec.dumpRegisters(); // nifty when debugging codec issues
    }
#endif
  sidetone.begin(44100,16,2,128); //  defaults to 44100, 16, 2, 32
  sidetone.setFrequency(600.0);
#endif
}

#ifdef CONFIG_TLV320AIC3100
void MorseOutput::soundEventHandler() {
  // interrupt reason needs to be read, otherwise the codec won't send any further interrupts
  if (codec.readRegister(AIC31XX_INTRDACFLAG) & AIC31XX_HSPLUG) { // bit 4 is set on headset related interrupts
    Serial.println("AIC31XX: Headset plug interrupt triggered");
  }
  if (codec.isHeadsetDetected()) {
    Serial.println("AIC31XX: Headset detected");
    codec.enableHeadphoneAmp();
    codec.setHeadphoneVolume(-10.0f,-10.0f); // unmute
    codec.setHeadphoneGain(0.0f,0.0f);
    codec.setHeadphoneMute(false); // unmute hp
    codec.setSpeakerMute(true); // unmute class d speaker amp
  } else {
    Serial.println("AIC31XX: Headset not plugged");
    codec.enableSpeakerAmp();
    codec.setSpeakerGain(6.0f); // valid db: 6, 12, 18, 24
    codec.setSpeakerVolume(-10.0f); // unmute
    codec.setSpeakerMute(false); // unmute class d speaker amp
    codec.setHeadphoneMute(true); // mute hp
  }
}
#endif

void MorseOutput::soundSuspend()
{
#ifdef CONFIG_WM8960
  codec.reset();
  delay(10);
  codec.disableVREF();
#endif
#ifdef CONFIG_TLV320AIC3100
  codec.reset();
#endif
}

void MorseOutput::pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut) { // frequency in Hertz, volume in range 0 - 19; we use 10 bit resolution
#ifndef CONFIG_SOUND_I2S
  const uint16_t vol[] =   {0,  1, 2, 4, 6, 9, 14, 21, 31, 45, 70, 100, 140, 200, 280, 390, 512, 680, 840, 1023}; // 20 values
  unsigned int i = constrain(volume, 0, 19);
  unsigned int j = vol[i] >> 8;     // experimental: soften the inital click
  unsigned int jj = vol[i] >> 3;
  //DEBUG(String(vol[i]));
  //DEBUG(String(frequency));
  if (lineOut) {
      ledcWriteTone(lineOutChannel, (double) frequency);
      ledcWrite(lineOutChannel, dutyCycleFiftyPercent);
  }

  ledcWrite(volChannel, 0);

  if (i == 0 ) {
      ledcWrite(toneChannel, dutyCycleZero);
  }
  else  {
  ledcWrite(toneChannel, dutyCycleFiftyPercent);
  ledcWriteTone(toneChannel, frequency);
  }

  //ledcWrite(volChannel, volFreq);
  ledcWrite(volChannel, j);       // experimental: soften the inital click
  delay(3);                       // experimental: soften the inital click
  ledcWrite(volChannel, jj);       // experimental: soften the inital click
  delay(3);
  ledcWrite(volChannel, vol[i]);

#else
  sidetone.setFrequency(frequency);
  sidetone.setVolume(float(volume) / 19.0);
  sidetone.on();
#endif
}


void MorseOutput::pwmNoTone(unsigned int volume) {      // stop playing a tone by changing duty cycle of the tone to 0
#ifndef CONFIG_SOUND_I2S
  const uint16_t vol[] =   {0,  1, 2, 4, 6, 9, 14, 21, 31, 45, 70, 100, 140, 200, 280, 390, 512, 680, 840, 1023}; // 20 values
  unsigned int i = constrain(volume, 0, 19);
  unsigned int j = vol[i] >> 8;     // experimental: soften the inital click
  unsigned int jj = vol[i] >> 3;

  //ledcWrite(toneChannel, 450);
  //ledcWrite(lineOutChannel, 450);
  ledcWrite(volChannel, jj);         // experimental: soften the click
  delay(3);
  ledcWrite(volChannel, j);         // experimental: soften the click
  delay(3);

  ledcWrite(volChannel, 0);

  ledcWrite(toneChannel, dutyCycleZero);
  ledcWrite(lineOutChannel, dutyCycleZero);

#else
  sidetone.off();
#endif
}


void MorseOutput::pwmClick(unsigned int volume) {                        /// generate a click on the speaker
    if (!MorsePreferences::pliste[posClicks].value)
      return;
    pwmTone(572,volume > 4 ? volume-4 : volume, false);
    delay(3);
    //pwmTone(286,volume, false);
    pwmTone(1144,volume > 3 ? volume-3 : volume, false);

    delay(6);
    //pwmTone(143,volume-4, false);
    //delay(7);
    pwmNoTone(volume);
}

void MorseOutput::soundSignalOK() {
    pwmTone(440, MorsePreferences::sidetoneVolume, false);
    delay(97);
    pwmNoTone(MorsePreferences::sidetoneVolume);
#ifdef CONFIG_SOUND_I2S
    delay(20);
#endif
    pwmTone(587, MorsePreferences::sidetoneVolume, false);
    delay(193);
    pwmNoTone(MorsePreferences::sidetoneVolume);
}


void MorseOutput::soundSignalERR() {
    pwmTone(311, MorsePreferences::sidetoneVolume, false);
    delay(193);
    pwmNoTone(MorsePreferences::sidetoneVolume);
}

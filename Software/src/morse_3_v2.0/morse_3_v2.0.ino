//////// Program Version
#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define BETA false

///// its is crucial to have the right board version - Boards 2 and 2a (prototypes only) set it to 2, Boards 3 set it to 3
///// the Board Version 2 is for HEltec Modules V1 only, Board Version 3 for Heltec V2 only
///// Board version 1 not supported anymore!

#define BOARDVERSION  3

///////////////////////
/////// protocol version for Lora - for the time being this is B01
/////// the first version of the CW over LoRA protocol; future versions will be B02, B03, B00 (reserved for future use)

#define CWLORAVERSION B01


/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018  Willi Kraml, OE1WKL                                                                                 ***
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

 /*****************************************************************************************************************************
 *  code by others used in this sketch, apart from the ESP32 libraries distributed by Heltec
 *  (see: https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series)
 *
 *  ClickButton library -> https://code.google.com/p/clickbutton/ by Ragnar Aronsen
 * 
 * 
 *  For volume control of NF output: I used a similar principle as  Connor Nishijima, see 
 *                                   https://hackaday.io/project/11957-10-bit-component-less-volume-control-for-arduino
 *                                   but actually using two PWM outputs, connected with an AND gate
 *  Routines for morse decoder - to a certain degree based on code by Hjalmar Skovholm Hansen OZ1JHM
 *                                   - see http://skovholm.com/cwdecoder
 ****************************************************************************************************************************/

///// include of the various libraries and include files being used

#include <Wire.h>          // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h"       // alias for `#include "SSD1306Wire.h"
#include "ClickButton.h"   // button control library
#include <Preferences.h>   // ESP 32 library for storing things in non-volatile storage
#include <SPI.h>           // library for SPI interface
#include <LoRa.h>          // library for LoRa transceiver
#include <WiFi.h>          // basic WiFi functionality
#include <WebServer.h>     // simple web sever
#include <ESPmDNS.h>       // DNS functionality
#include <WiFiClient.h>    //WiFi clinet library
#include <Update.h>        // update "over the air" (OTA) functionality
#include "FS.h"
#include "SPIFFS.h"

#include "wklfonts.h"      // monospaced fonts in size 12 (regular and bold) for smaller text and 15 for larger text (regular and bold), called :
                           // DialogInput_plain_12, DialogInput_bold_12 & DialogInput_plain_15, DialogInput_bold_15
                           // these fonts were created with this tool: http://oleddisplay.squix.ch/#/home
#include "abbrev.h"        // common CW abbreviations
#include "english_words.h" // common English words



/// we need this for some strange reason: the min definition breaks with WiFi
#define _min(a,b) ((a)<(b)?(a):(b))
#define _max(a,b) ((a)>(b)?(a):(b))


///////////// Some GLOBAL defines

// SENS_FACTOR is used for auto-calibrating sensitivity of touch paddles (somewhere between 2.0 and 2.5)
#define SENS_FACTOR 2.22


///////////////////////////////      H A R D W A R E      ///////////////////////////////////////////
//// Here are the definitions for the various hardware-related I/O pins of the ESP32
/////////////////////////////////////////////////////////////////////////////////////////////////////
///// Board versions:
///// 2(a): for Heltec ESP32 LORA Version 1 (Morserino-32 prototypes)
///// 3: for heltec ESP32 LORA Version 2
///// Warning: Board version 1 not supported anymore!!!! (was an early prototype)
/////
///// the following Pins are dependent on the board version
///// 21:  V2: an encoder port, using interrupts      V3: Vext - used internally to switch Vext on and off
///// 39:  V2: ADC input to measure battery voltage   V3: encoder port (interrupt driven) instead of 21
///// 13:  V2: leftPin (external paddle, ext. pullup) V3: used internally to read battery voltage
///// 38:  V2: not in use                             V3: encoder port (interrupt driven) instead of 35
///// 32:  V2: used for LoRa internally               V3: leftPin
///// 33:  V2: used for LoRa internally               V3: rightPin
///// 34: rightPin                                    V3: cannot be used any longer, used for LoRa internally
///// 35: PinDT                                       V3: cannot be used any longer, used for LoRa internally

#ifndef BOARDVERSION
  #error "You need to define a board version  at the beginning of the source file!"
#endif

/////// here are the board dependent pins definitions

#if BOARDVERSION == 2

/// where are the external paddles?
const int leftPin = 13;   // external pullup resistor is necessary for closing contacts!
const int rightPin = 34;  // external pullup resistor is necessary for closing contacts!

/// where is the encoder?
const int PinCLK=21;                   // Used for generating interrupts using CLK signal - needs external pullup resisitor! 
const int PinDT=35;                    // Used for reading DT signal  - needs external pullup resisitor! 

// input for battery voltage control
const int batteryPin = 39;

#elif BOARDVERSION == 3

/// where are the external paddles?
const int leftPin = 33;   // external pullup resistor is necessary for closing contacts!
const int rightPin = 32;  // external pullup resistor is necessary for closing contacts!

/// where is the encoder?
const int PinCLK=38;                   // Used for generating interrupts using CLK signal - needs external pullup resisitor! 
const int PinDT=39;                    // Used for reading DT signal  - needs external pullup resisitor! 

// input for battery voltage control
const int batteryPin = 13;

// pin to switch ON Vext
const int Vext = 21;


#else
  #error "Invalid Board Version! This program version only supports board versions 2 and 3."
#endif


//////// HARDWARE definitions that have not been changed between board version2 and 3 (ESP32 WIFI LORA V.1 and V.2)

//OLED pins to ESP32 GPIOs:

const int OLED_SDA = 4;
const int OLED_SCL = 15;
const int OLED_RST =  16;
// define OLED display and its address - the Heltec ESP32 LoRA uses its display on 0x3c
SSD1306  display(0x3c, OLED_SDA, OLED_SCL, OLED_RST);

// Pin definition of WIFI LoRa 32
// HelTec AutoMation 2017 support@heltec.cn 
#define SCK     5    // GPIO5  -- SX127x's SCK
#define MISO    19   // GPIO19 -- SX127x's MISO
#define MOSI    27   // GPIO27 -- SX127x's MOSI
#define SS      18   // GPIO18 -- SX127x's CS
#define RST     14   // GPIO14 -- SX127x's RESET
#define DI0     26   // GPIO26 -- SX127x's IRQ(Interrupt Request)

// #define BAND    433E6  //you can set band here directly,e.g. 868E6,915E6
#define PABOOST true


//// with the following we define which pins are used as output for the two pwm channels
//// HF output (with varying duticycle and fixed frequency) and LF output (with varying frequency and fixed dutycycle of 50%)
/// are being added with a 2-transistor AND gate to create a tone frequency with variable frequency and volume

const int LF_Pin = 23;    // for the lower (= NF) frequency generation
const int HF_Pin = 22;    // for the HF PWM generation


/// where are the touch paddles?
const int LEFT = T2;        // = Pin 2
const int RIGHT = T5;       // = Pin 12


/// 2nd switch button - toggles between Speed control and Volume control
const int volButtonPin = 0;


// Tx keyer 
const int keyerPin = 25;        // this keys the transmitter / through a MOSFET Optocoupler - at the same time lights up the LED


// audio in
const int audioInPin = 36;      // audio in for Morse decoder // 


// NF Line-out (for iCW etc.)
const int lineOutPin = 17; // for NF line out


/// Switch button (on rotary encoder)
const int modeButtonPin = 37;    // input pin for mode button - needs external pullup!


// define the buttons for the clickbutton library
ClickButton modeButton(modeButtonPin);  // initialize mode button
ClickButton volButton(volButtonPin);    // external pullup for this one

///////////////////////////////////////// END OF HARDWARE DEFS ////////////////////////////////////////////////////////////////////




////////////////////////////// New scrolling display

/// circular buffer: 14 chars by NoOfLines lines (bottom 3 are visible)
#define NoOfLines 15
#define NoOfCharsPerLine 14
#define SCROLL_TOP 15
#define LINE_HEIGHT 16

char textBuffer[NoOfLines][2*NoOfCharsPerLine+1];   /// we need extra room for style markers (FONT_ATTRIB stored as characters to toggle on/off the style within a line) 
                                                    /// and 0 terminator

enum FONT_ATTRIB {REGULAR, BOLD, INVERSE_REGULAR, INVERSE_BOLD};

uint8_t linePointer = 0;    /// defines the current bottom line
uint8_t bottomLine = 0;

const int8_t maxPos = NoOfLines -3;
int8_t relPos = maxPos;

#define lora_width 6        /// a simple logo that shows when we operate with loRa, stored in XBM format
#define lora_height 11
static unsigned char lora_bits[] = {
   0x0f, 0x18, 0x33, 0x24, 0x29, 0x2b, 0x29, 0x24, 0x33, 0x18, 0x0f };


/////////////////////// parameters for LF tone generation and  HF (= vol ctrl) PWM
int toneFreq = 500 ;
int toneChannel = 2;      // this PWM channel is used for LF generation, duty cycle is 0 (silent) or 50% (tone)
int lineOutChannel = 3;   // this PWM channel is used for line-out LF generation, duty cycle is 0 (silent) or 50% (tone)
int volChannel = 8;       // this PWM channel is used for HF generation, duty cycle between 1% (almost silent) and 100% (loud)
int pwmResolution = 10;
unsigned int volFreq = 32000; // this is the HF frequency we are using

const int  dutyCycleFiftyPercent =  512;                                                                             ;
const int  dutyCycleTwentyPercent = 25;
const int  dutyCycleZero = 0;

const int notes[] = {0, 233, 262, 294, 311, 349, 392, 440, 466, 523, 587, 622, 698, 784, 880, 932};



// things for reading the encoder
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;


volatile int8_t _oldState;

#define LATCHSTATE 3

volatile int8_t encoderPos = 0;
volatile uint64_t IRTime = 0;   // interrupt time
const int encoderWaitTime = 100 ;         // how long to wait for next reading from encoder in microseconds
volatile uint8_t stateRegister = 0;

// positions: [3] 1 0 2 [3] 1 0 2 [3]
// [3] is the positions where my rotary switch detends
// ==> right, count up
// <== left,  count down



/// the states the morserino can be in - selected intop level menu
enum morserinoMode {morseKeyer, loraTrx, morseTrx, morseGenerator, echoTrainer, morseDecoder, shutDown, measureNF, invalid };
morserinoMode morseState = morseKeyer;

//////// variables and constants for the modus menu


const uint8_t menuN = 40;     // no of menu items +1

const String menuText [menuN] = {
  "",
  "CW Keyer", // 1
  
  "CW Generator", // 2
    "Random",
    "CW Abbrevs",
    "English Words",
    "Call Signs",
    "Mixed",
    "File Player",

  "Echo Trainer", // 9
    "Random",
    "CW Abbrevs",
    "English Words",
    "Call Signs",
    "Mixed",
    "File Player",
  
  "Koch Trainer", // 16
    "Select Lesson",
    "Learn New Chr",
    "CW Generator",   // 19
      "Random",   // 20
      "CW Abbrevs",
      "English Words",
      "Mixed",
    "Echo Trainer",   // 24
      "Random",
      "CW Abbrevs",
      "English Words",
      "Mixed",
  
  "Transceiver",    // 29
    "LoRa Trx",
    "iCW/Ext Trx",
  
  "CW Decoder",     // 32

  "WiFi Functions", // 33
    "Disp MAC Addr",
    "Config WiFi",
    "Check WiFi",
    "Upload File",
    "Update Firmw", //38
  
  
  "Go To Sleep" } ; // 39

enum navi {naviLevel, naviLeft, naviRight, naviUp, naviDown };
enum menuNo { _dummy, _keyer, _gen, _genRand, _genAbb, _genWords, _genCalls, _genMixed, _genPlayer,
              _echo, _echoRand, _echoAbb, _echoWords, _echoCalls, _echoMixed, _echoPlayer,
              _koch, _kochSel, _kochLearn, _kochGen, _kochGenRand, _kochGenAbb, _kochGenWords,
              _kochGenMixed, _kochEcho, _kochEchoRand, _kochEchoAbb, _kochEchoWords, _kochEchoMixed,
              _trx, _trxLora, _trxIcw, _decode, _wifi, _wifi_mac, _wifi_config, _wifi_check, _wifi_upload, _wifi_update, _goToSleep };
              

const uint8_t menuNav [menuN] [5] = {                   // { level, left, right, up, down}
  { 0,0,0,0,0},                                         // 0 = dummy                
  {0,_goToSleep,_gen,_dummy,0},                         // 1 keyer
  {0,_keyer,_echo,_dummy,_genRand},                     // 2 generator
  {1,_genPlayer,_genAbb,_gen,0},                        // 3 gen random
  {1,_genRand,_genWords,_gen,0},                        // 4 gen Abb
  {1,_genAbb,_genCalls,_gen,0},                         // 5 gen words
  {1,_genWords,_genMixed,_gen,0},                       // 6 gen calls
  {1,_genCalls,_genPlayer,_gen,0},                      // 7 gen mixed
  {1,_genMixed,_genRand,_gen,0},                        // 8 gen player
  {0,_gen,_koch,_dummy,_echoRand},                      // 9 echo tr
  {1,_echoPlayer,_echoAbb,_echo,0},                     // 10 echo random
  {1,_echoRand,_echoWords,_echo,0},                     // 11 echo abb
  {1,_echoAbb,_echoCalls,_echo,0},                      // 12 echo words
  {1,_echoWords,_echoMixed,_echo,0},                    // 13 echo calls
  {1,_echoCalls,_echoPlayer,_echo,0},                   // 14 echo mixed
  {1,_echoMixed,_echoRand,_echo,0},                     // 15 echo player
  {0,_echo,_trx,_dummy,_kochSel},                          // 16 koch
  {1,_kochEcho,_kochLearn,_koch,0},                     // 17 koch select
  {1,_kochSel,_kochGen,_koch,0},                        // 18 koch learn new
  {1,_kochLearn,_kochEcho,_koch,_kochGenRand},          // 19 koch gen
  {2,_kochGenMixed,_kochGenAbb,_kochGen,0},             // 20 koch gen random
  {2,_kochGenRand,_kochGenWords,_kochGen,0},            // 21 koch gen abb
  {2,_kochGenAbb,_kochGenMixed,_kochGen,0},             // 22 koch gen words
  {2,_kochGenWords,_kochGenRand,_kochGen,0},            // 23 koch gen mixed
  {1,_kochGen,_kochSel,_koch,_kochEchoRand},            // 24 koch echo
  {2,_kochEchoMixed,_kochEchoAbb,_kochEcho,0},          // 25 koch echo random
  {2,_kochEchoRand,_kochEchoWords,_kochEcho,0},         // 26 koch echo abb
  {2,_kochEchoAbb,_kochEchoMixed,_kochEcho,0},          // 27 koch echo words
  {2,_kochEchoWords,_kochEchoRand,_kochEcho,0},         // 28 koch echo mixed
  {0,_koch,_decode,_dummy,_trxLora},                    // 29 transceiver
  {1,_trxIcw,_trxIcw,_trx,0},                           // 30 lora
  {1,_trxLora,_trxLora,_trx,0},                         // 31 icw
  {0,_trx,_wifi,_dummy,0},                              // 32 decoder
  {0,_decode,_goToSleep,_dummy,_wifi_mac},              // 33 WiFi
  {1,_wifi_update,_wifi_config,_wifi,0},                // 34 Disp Mac
  {1,_wifi_mac,_wifi_check,_wifi,0},                    // 35 Config Wifi
  {1,_wifi_config,_wifi_upload,_wifi,0},                // 36 Check WiFi
  {1,_wifi_check,_wifi_update,_wifi,0},                 // 37 Upload File
  {1,_wifi_upload,_wifi_mac,_wifi,0},                   // 38 Update Firmware 
  {0,_wifi,_keyer,_dummy,0}                             // 39 goto sleep
};

boolean quickStart;                                     // should we execute menu item immediately?

// defines for keyer modi
//

#define    IAMBICA      1          // Curtis Mode A
#define    IAMBICB      2          // Curtis Mode B (with enhanced Curtis timing, set as parameter
#define    ULTIMATIC    3          // Ultimatic mode
#define    NONSQUEEZE   4          // Non-squeeze mode of dual-lever paddles - simulate a single-lever paddle

// define modes for state machine of the various modi the encoder can be in
 
enum encoderMode {speedSettingMode, volumeSettingMode, scrollMode }; 

encoderMode encoderState = speedSettingMode;    // we start with adjusting the speed

//// for adjusting preferences


enum prefPos  { posClicks, posPitch, posExtPaddles, posPolarity, 
                posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS, 
                posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption, 
                posRandomLength, posCallLength, posAbbrevLength, posWordLength, 
                posTrainerDisplay, posWordDoubler, posEchoDisplay, posEchoRepeats,  posEchoConf, 
                posKeyTrainerMode, posLoraTrainerMode, posGoertzelBandwidth, posSpeedAdapt,
                posKochSeq, posKochFilter, posLatency, posRandomFile, posTimeOut, posQuickStart, posAutoStop, posLoraSyncW,
                posLoraBand, posLoraQRG, posSnapRecall, posSnapStore, posMaxSequence};

const String prefOption[] = { "Encoder Click", "Tone Pitch Hz", "External Pol.", "Paddle Polar.", 
                              "Keyer Mode   ", "CurtisB DahT%", "CurtisB DitT%", "AutoChar Spce", 
                              "Tone Shift   ", "InterWord Spc", "InterChar Spc", "Random Groups", 
                              "Length Rnd Gr", "Length Calls ", "Length Abbrev", "Length Words ", 
                              "CW Gen Displ ", "Each Word 2x ", "Echo Prompt  ", "Echo Repeats ", "Confrm. Tone ", 
                              "Key ext TX   ", "Send via LoRa", "Bandwidth    ", "Adaptv. Speed",  
                              "Koch Sequence", "Koch         ", "Latency      ", "Randomize File",
                              "Time Out     ", "Quick Start  ", "Auto Stop    ", "LoRa Channel  ",
                              "LoRa Band    ", "LoRa Frequ   ", "RECALLSnapshot", "STORE Snapshot",
                              "Max # of Words"};                   
 prefPos keyerOptions[] =      {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS, posKeyTrainerMode, posTimeOut, posQuickStart };
 prefPos generatorOptions[] =  {posClicks, posPitch, posExtPaddles, posInterWordSpace, posInterCharSpace, posRandomOption, 
                                    posRandomLength, posCallLength, posAbbrevLength, posWordLength, posMaxSequence,
                                    posTrainerDisplay, posWordDoubler, posKeyTrainerMode, posLoraTrainerMode, posLoraSyncW, posTimeOut, posQuickStart, posAutoStop };
 prefPos playerOptions[] =     {posClicks, posPitch, posExtPaddles, posInterWordSpace, posInterCharSpace, posMaxSequence, posTrainerDisplay, 
                                     posRandomFile, posWordDoubler, posKeyTrainerMode, posLoraTrainerMode, posLoraSyncW, posTimeOut, posQuickStart, posAutoStop };
 prefPos echoPlayerOptions[] = {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                                    posEchoToneShift, posInterWordSpace, posInterCharSpace, posMaxSequence, posRandomFile, posEchoRepeats,  posEchoDisplay, posEchoConf, posTimeOut, posQuickStart};
 prefPos echoTrainerOptions[]= {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                                    posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption, 
                                    posRandomLength, posCallLength, posAbbrevLength, posWordLength, posMaxSequence, posEchoRepeats,  posEchoDisplay, posEchoConf, posSpeedAdapt, posTimeOut, posQuickStart };
 prefPos kochGenOptions[] =    {posClicks, posPitch, posExtPaddles, posInterWordSpace, posInterCharSpace, 
                                    posRandomLength,  posAbbrevLength, posWordLength, posMaxSequence,
                                    posTrainerDisplay, posWordDoubler, posKeyTrainerMode, posLoraTrainerMode, posLoraSyncW, posKochSeq, posTimeOut, posQuickStart, posAutoStop };
 prefPos kochEchoOptions[] =   {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                                    posEchoToneShift, posInterWordSpace, posInterCharSpace, 
                                    posRandomLength, posAbbrevLength, posWordLength, posMaxSequence, posEchoRepeats, posEchoDisplay, posEchoConf, posSpeedAdapt, posKochSeq, posTimeOut, posQuickStart };
 prefPos loraTrxOptions[] =    {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                                    posEchoToneShift, posTimeOut, posQuickStart, posLoraSyncW };
 prefPos extTrxOptions[] =     {posClicks, posPitch, posExtPaddles, posPolarity, posLatency, posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                                    posEchoToneShift, posGoertzelBandwidth, posTimeOut, posQuickStart };
 prefPos decoderOptions[] =    {posClicks, posPitch, posGoertzelBandwidth, posTimeOut, posQuickStart };

 prefPos allOptions[] =        { posClicks, posPitch, posExtPaddles, posPolarity, posLatency,
                                    posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS, 
                                    posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption, 
                                    posRandomLength, posCallLength, posAbbrevLength, posWordLength, posMaxSequence,
                                    posTrainerDisplay, posRandomFile, posWordDoubler, posEchoRepeats, posEchoDisplay, posEchoConf, 
                                    posKeyTrainerMode, posLoraTrainerMode, posLoraSyncW, posGoertzelBandwidth, posSpeedAdapt, posKochSeq, posTimeOut, posQuickStart, posAutoStop};

prefPos *currentOptions = allOptions;

#define SizeOfArray(x)       (sizeof(x) / sizeof(x[0]))

int currentOptionSize;


///////////////////////////////////
//// Other Global VARIABLES ////////////
/////////////////////////////////

unsigned int interCharacterSpace, interWordSpace;   // need to be properly initialised!
unsigned int effWpm;                                // calculated effective speed in WpM
unsigned int lUntouched = 0;                        // sensor values (in untouched state) will be stored here
unsigned int rUntouched = 0;

volatile uint64_t TOTcounter;                       // holds millis for Time-Out Timer

char numBuffer[16];                // for number to string conversion with sprintf()


enum GEN_TYPE { RANDOMS, ABBREVS, WORDS, CALLS, MIXED, PLAYER, KOCH_MIXED, KOCH_LEARN };              // the things we can generate in generator mode
enum DISPLAY_TYPE { NO_DISPLAY, DISPLAY_BY_CHAR, DISPLAY_BY_WORD };   // how we display in trainer mode
enum random_OPTIONS {OPT_ALL, OPT_ALPHA, OPT_NUM, OPT_PUNCT, OPT_PRO, OPT_ALNUM, OPT_NUMPUNCT, OPT_PUNCTPRO, OPT_ALNUMPUNCT, OPT_NUMPUNCTPRO, OPT_KOCH };
enum PROMPT_TYPE { NO_PROMPT, CODE_ONLY , DISP_ONLY ,CODE_AND_DISP };

//// not any longer defined in preferences:
  GEN_TYPE generatorMode = RANDOMS;          // trainer: what symbol (groups) are we going to send?            0 -  5

  
Preferences pref;               // use the Preferences library for storing and retrieving objects
// the preferences variable and their defaults

  uint8_t p_version_major = VERSION_MAJOR;
  uint8_t p_version_minor = VERSION_MINOR;
  uint8_t p_sidetoneFreq = 11;               // side tone frequency                               1 - 15
  uint8_t p_sidetoneVolume = 60;              // side tone volume, as a value between 0 and 100   0 -100
  boolean p_didah = false;                    // paddle polarity                                  bool
  uint8_t p_keyermode = 2;                    // Iambic keyer mode: see the #defines above        1 -  3
  uint8_t p_interCharSpace = 3;               // trainer: in dit lengths                          3 - 24
  boolean p_useExtPaddle = false;             // has now a different meaning: true when we need to reverse the polarity of the ext paddle
  uint8_t p_ACSlength = 0;                    // in ACS: we extend the pause between charcaters to the equal length of how many dots 
                                              // (2, 3 or 4 are meaningful, 0 means off) 0, 2-4
  boolean p_encoderClicks = true;             // all: should rotating the encoder generate a click?
  uint8_t p_randomLength = 3;                 // trainer: how many random chars in one group -    1 -  5
  uint8_t p_randomOption = 0;                 // trainer: from which pool are we generating random characters?  0 - 9
  uint8_t p_callLength = 0;                   // trainer: max length of call signs generated (0 = unlimited)    0, 3 - 6
  uint8_t p_abbrevLength = 0;                 // trainer: max length of abbreviations generated (0 = unlimited) 0, 2 - 6
  uint8_t p_wordLength = 0;                   // trainer: max length of english words generated (0 = unlimited) 0, 2 - 6
  uint8_t p_trainerDisplay = DISPLAY_BY_CHAR; // trainer: how we display what the trainer generates: nothing, by character, or by word  0 - 2
  uint8_t p_curtisBTiming = 45;               // keyer: timing for enhanced Curtis mode: dah                    0 - 100
  uint8_t p_curtisBDotTiming = 75 ;           // keyer: timing for enhanced Curtis mode: dit                    0 - 100
  uint8_t p_interWordSpace = 7;               // trainer: normal interword spacing in lengths of dit,           6 - 45 ; default = norm = 7

  uint8_t p_echoRepeats = 3;                  // how often will echo trainer repeat an erroniously entered word? 0 - 7, 7=forever, default = 3
  uint8_t p_echoDisplay = 1;                  //  1 = CODE_ONLY 2 = DISP_ONLY 3 = CODE_AND_DISP
  uint8_t p_kochFilter = 5;                   // constrain output to characters learned according to Koch's method 2 - 45
  boolean p_wordDoubler = false;              // in CW trainer mode only, repeat each word
  uint8_t p_echoToneShift = 1;                // 0 = no shift, 1 = up, 2 = down (a half tone)                   0 - 2
  boolean p_echoConf = true;                  // true if echo trainer confirms audibly too, not just visually
  uint8_t p_keyTrainerMode = 1;               // key a transmitter in generator and player mode?
                                              //  0: "Never";  1: "CW Keyer only";  2: "Keyer&Generator";
  uint8_t p_loraTrainerMode = 0;              // transmit via LoRa in generator and player mode?
                                              //  0: "No";  1: "yes"
  uint8_t p_goertzelBandwidth = 0;            //  0: "Wide" 1: "Narrow" 
  boolean p_speedAdapt = false;               //  true: in echo modes, increase speed when OK, reduce when not ok     
  uint8_t p_latency = 5;                      //  time span after currently sent element during which paddles are not checked; in 1/8th of dit length; stored as 1 -  8  
  uint8_t p_randomFile = 0;                   // if 0, play file word by word; if 255, skip random number of words (0 - 255) between reads   
  boolean p_lcwoKochSeq = false;              // if true, replace native sequence with LCWO sequence    
  uint8_t p_timeOut = 1;                      // time-out value: 4 = no timeout, 1 = 5 min, 2 = 10 min, 3 = 15 min
  boolean p_quickStart = false;               // should we start the last executed command immediately?
  boolean p_autoStop = false;                 // If to stop after each word in generator modes
  uint8_t p_loraSyncW = 0x27;                 // allows to set different LoRa sync words, and so creating virtual "channels"

///// stored in preferences, but not adjustable through preferences menu:
  uint8_t p_responsePause = 5;                // in echoTrainer mode, how long do we wait for response? in interWordSpaces; 2-12, default 5
  uint8_t p_wpm = 15;                         // keyer speed in words per minute                  5 - 60
  uint8_t p_menuPtr = 1;                      // current position of menu
  String  p_wlanSSID = "";                    // SSID for connecting to the Internet
  String  p_wlanPassword = "";                // password for connecting to WiFi router
  uint32_t p_fileWordPointer = 0;             // remember how far we have read the file in player mode / reset when loading new file         
  uint8_t p_promptPause = 2;                  // in echoTrainer mode, length of pause before we send next word; multiplied by interWordSpace
  uint8_t p_tLeft = 20;                       // threshold for left paddle
  uint8_t p_tRight = 20;                      // threshold for right paddle
  
  uint8_t p_loraBand = 0;                     // 0 = 433, 1 = 868, 2 = 920
#define QRG433 434.15E6
#define QRG866 869.15E6
#define QRG920 920.55E6
  uint32_t p_loraQRG = QRG433;                // for 70 cm band

  uint8_t p_snapShots = 0;                    // keep track which snapshots are being used ( 0 .. 7, called 1 to 8)
  uint8_t p_maxSequence = 0;                  // max # of words generated beofre the Morserino pauses

////// end of variables stored in preferences

/// variables for managing snapshots
uint8_t memories[8];
uint8_t memCounter;
uint8_t memPtr = 0;



boolean kochActive = false;                 // set to true when in Koch trainer mode

//  keyerControl bit definitions

#define     DIT_L      0x01     // Dit latch
#define     DAH_L      0x02     // Dah latch
#define     DIT_LAST   0x04     // Dit was last processed element

//  Global Keyer Variables
//
unsigned char keyerControl = 0; // this holds the latches for the paddles and the DIT_LAST latch, see above


boolean DIT_FIRST = false; // first latched was dit?
unsigned int ditLength ;        // dit length in milliseconds - 100ms = 60bpm = 12 wpm
unsigned int dahLength ;        // dahs are 3 dits long
unsigned char keyerState;
unsigned long charCounter = 25; // we use this to count characters after changing speed - after n characters we decide to write the config into NVS
uint8_t sensor;                 // what we read from checking the touch sensors
boolean leftKey, rightKey;


///////////////////////////////////////////////////////////////////////////////
//
//  Iambic Keyer State Machine Defines
 
enum KSTYPE {IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT };

// morse code decoder

struct linklist {
     const char* symb;
     const uint8_t dit;
     const uint8_t dah;
};


const struct linklist CWtree[67]  = {
  {"",1,2},            // 0
  {"e", 3,4},         // 1
  {"t",5,6},          // 2
//
  {"i", 7, 8},        // 3
  {"a", 9,10},        // 4
  {"n", 11,12},       // 5
  {"m", 13,14},       // 6
//
  {"s", 15,16},       // 7
  {"u", 17,18},       // 8
  {"r", 19,20},       // 9
  {"w", 21,22},       //10
  {"d", 23,24},       //11
  {"k", 25, 26},      //12
  {"g", 27, 28},      //13
  {"o", 29,30},       //14
//---------------------------------------------
  {"h", 31,32},       // 15
  {"v", 33, 34},      // 16
  {"f", 63, 63},      // 17
  {"ü", 35, 36},      // 18 german ue
  {"l", 37, 38},      // 19
  {"ä", 39, 63},      // 20 german ae
  {"p", 63, 40},      // 21
  {"j", 63, 41},      // 22
  {"b", 42, 43},      // 23
  {"x", 44, 63},      // 24
  {"c", 63, 45},      // 25
  {"y", 46, 63},      // 26
  {"z", 47, 48},      // 27
  {"q", 63, 63},      // 28
  {"ö", 49, 63},      // 29 german oe
  {"<ch>", 50, 51},        // 30 !!! german "ch" 
//---------------------------------------------
  {"5", 64, 63},      // 31
  {"4", 63, 63},      // 32
  {"<ve>", 63, 52},      // 33  or <sn>, sometimes "*"
  {"3", 63,63},       // 34
  {"*", 53,63,},      // 35 ¬ used for all unidentifiable characters ¬
  {"2", 63, 63},      // 36 
  {"<as>", 63,63},         // 37 !! <as>
  {"*", 54, 63},      // 38
  {"+", 63, 55},      // 39
  {"*", 56, 63},      // 40
  {"1", 57, 63},      // 41
  {"6", 63, 58},      // 42
  {"=", 63, 63},      // 43
  {"/", 63, 63},      // 44
  {"<ka>", 59, 60},        // 45 !! <ka>
  {"<kn>", 63, 63},        // 46 !! <kn>
  {"7", 63, 63},      // 47
  {"*", 63, 61},      // 48
  {"8", 62, 63},      // 49
  {"9", 63, 63},      // 50
  {"0", 63, 63},      // 51
//
  {"<sk>", 63, 63},        // 52 !! <sk>
  {"?", 63, 63},      // 53
  {"\"", 63, 63},      // 54
  {".", 63, 63},      // 55
  {"@", 63, 63},      // 56
  {"\'",63, 63},      // 57
  {"-", 63, 63},      // 58
  {";", 63, 63},      // 59
  {"!", 63, 63},      // 60
  {",", 63, 63},      // 61
  {":", 63, 63},      // 62
//
  {"*", 63, 63},       // 63 Default for all unidentified characters
  {"*", 65, 63},       // 64
  {"*", 66, 63},       // 65
  {"<err>", 66, 63}      // 66 !! Error - backspace
};


byte treeptr = 0;                          // pointer used to navigate within the linked list representing the dichotomic tree

unsigned long interWordTimer = 0;      // timer to detect interword spaces
unsigned long acsTimer = 0;            // timer to use for automatic character spacing (ACS)


const String CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKVäöüH";
//                      0....5....1....5....2....5....3....5....4....5....5...    
// we use substrings as char pool for trainer mode
// SANK will be replaced by <as>, <ka>, <kn> and <sk>, H = ch
// a = CWchars.substring(0,26); 9 = CWchars.substring(26,36); ? = CWchars.substring(36,45); <> = CWchars.substring(44,49);
// a9 = CWchars.substring(0,36); 9? = CWchars.substring(26,45); ?<> = CWchars.substring(36,50);
// a9? = CWchars.substring(0,45); 9?<> = CWchars.substring(26,50);
// a9?<> = CWchars;


///// variables for generating CW

   String CWword = "";
   String clearText = "";

   int repeats = 0;

   int rxDitLength = 0;                    // set new value for length of dits and dahs and other timings
   int rxDahLength = 0;
   int rxInterCharacterSpace = 0;
   int rxInterWordSpace = 0;

  //CWword.reserve(144);
  //clearText.reserve(50);
boolean active = false;                           // flag for trainer mode
boolean startFirst = true;                        // to indicate that we are starting a new sequence in the trainer modi
boolean firstTime = true;                         /// for word doubler mode

uint8_t wordCounter = 0;                          // for maxSequence
boolean stopFlag = false;                         // for maxSequence
boolean echoStop = false;                         // for maxSequence

unsigned long genTimer;                         // timer used for generating morse code in trainer mode

enum MORSE_TYPE {KEY_DOWN, KEY_UP };                    //   State Machine Defines
unsigned char generatorState;


//byte NoE = 0;             // Number of Elements
// byte nextElement[8];      // the list of elements; 0 = dit, 1 = dah

// for each character:
// byte length// byte morse encoding as binary value, beginning with most significant bit

byte poolPair[2];           // storage in RAM for one morse code character

const byte pool[][2]  = {
// letters
               {B01000000, 2},  // a    0     
               {B10000000, 4},  // b
               {B10100000, 4},  // c
               {B10000000, 3},  // d
               {B00000000, 1},  // e
               {B00100000, 4},  // f
               {B11000000, 3},  // g
               {B00000000, 4},  // h
               {B00000000, 2},  // i
               {B01110000, 4},  // j 
               {B10100000, 3},  // k
               {B01000000, 4},  // l
               {B11000000, 2},  // m  
               {B10000000, 2},  // n
               {B11100000, 3},  // o
               {B01100000, 4},  // p  
               {B11010000, 4},  // q
               {B01000000, 3},  // r
               {B00000000, 3},  // s
               {B10000000, 1},  // t
               {B00100000, 3},  // u
               {B00010000, 4},  // v
               {B01100000, 3},  // w
               {B10010000, 4},  // x
               {B10110000, 4},  // y
               {B11000000, 4},  // z  25
// numbers
               {B11111000, 5},  // 0  26    
               {B01111000, 5},  // 1
               {B00111000, 5},  // 2
               {B00011000, 5},  // 3
               {B00001000, 5},  // 4
               {B00000000, 5},  // 5
               {B10000000, 5},  // 6
               {B11000000, 5},  // 7
               {B11100000, 5},  // 8
               {B11110000, 5},  // 9  35
// interpunct   . , : - / = ? @ +    010101 110011 111000 100001 10010 10001 001100 011010 01010
               {B01010100, 6},  // .  36    
               {B11001100, 6},  // ,  37    
               {B11100000, 6},  // :  38    
               {B10000100, 6},  // -  39    
               {B10010000, 5},  // /  40    
               {B10001000, 5},  // =  41    
               {B00110000, 6},  // ?  42    
               {B01101000, 6},  // @  43    
               {B01010000, 5},  // +  44    (at the same time <ar> !) 
// Pro signs  <>  <as> <ka> <kn> <sk>
               {B01000000, 5},  // <as> 45 S
               {B10101000, 5},  // <ka> 46 A
               {B10110000, 5},  // <kn> 47 N
               {B00010100, 6},   // <sk> 48    K
               {B00010000, 5},  // <ve> 49 E
// German characters
               {B01010000, 4},  // ä    50   
               {B11100000, 4},  // ö    51
               {B00110000, 4},  // ü    52
               {B11110000, 4}   // ch   53  H
            };

////////////////////////////////////////////////////////////////////
///// Variables for Echo Trainer Mode
/////

String echoResponse = "";
enum echoStates { START_ECHO, SEND_WORD, REPEAT_WORD, GET_ANSWER, COMPLETE_ANSWER, EVAL_ANSWER };
echoStates echoTrainerState = START_ECHO;
String echoTrainerPrompt, echoTrainerWord;


/////////////////// Variables for Koch modes

String  kochWords[WORDS_NUMBER_OF_ELEMENTS];
int numberOfWords;
String kochAbbr[ABBREV_NUMBER_OF_ELEMENTS];
int numberOfAbbr;

String kochChars;
const String morserinoKochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAV@:";
const String lcwoKochChars =      "kmuresnaptlwi.jz=foy,vg5/q92h38b?47c1d60x-K+ASNV@:";

////// variables for CW decoder

boolean keyTx = false;             // when state is set by manual key or touch paddle, then true!
                                   // we use this to decide if Tx should be keyed or not

/////////////////// Variables for LoRa: Buffer management etc

char loraTxBuffer[32];

uint8_t loRaRxBuffer[256];
uint16_t byteBuFree = 256;
uint8_t nextBuWrite = 0;
uint8_t nextBuRead = 0;

uint8_t loRaSerial;                                     /// a 6 bit serial number, start with some random value, will be incremented witch each sent LoRa packet
                                                        /// the first two bits in teh byte will be the protocol id (CWLORAVERSION)


////////////////// Variables for file handling and WiFi functions

File file;

WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

File fsUploadFile;              // a File object to temporarily store the received file

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS
const char* host = "m32";               // hostname of the webserver


/// WiFi constants
const char* ssid = "morserino";
const char* password = "";


                          // HTML for the AP server - ued to get SSID and Password for local WiFi network - needed for file upload and OTA SW updates
const char* myForm = "<html><head><meta charset='utf-8'><title>Get AP Info</title><style> form {width: 420px;}div { margin-bottom: 20px;}"
                "label {display: inline-block; width: 240px; text-align: right; padding-right: 10px;} button, input {float: right;}</style>"
                "</head><body>"
                "<form action='/set' method='get'><div>"
                "<label for='ssid'>SSID of WiFi network?</label>"
                "<input name='ssid' id='ssid' ></div> <div>"
                "<label for='pw'>WiFi Password?</label> <input name='pw' id='pw'>"
                "</div><div><button>Submit</button></div></form></body></html>";



/*
 * HTML for Upload Login page
 */

const char* uploadLoginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 File Upload - Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='upload')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";


const char* updateLoginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 Firmware Update Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='update')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Begin'>"
    "</form>"
 "<div id='prg'>Progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";


////// variables for Morse Decoder - the more global ones. rest is further down...
////////////////////////////
/// variables for morse decoder
///////////////////////////////
uint32_t magnitudelimit;                                   // magnitudelimit_low = ( p_goertzelBandwidth? 80000 : 30000);
uint32_t magnitudelimit_low;                               // magnitudelimit = magnitudelimit_low;


boolean speedChanged = true;
boolean filteredState = false;
boolean filteredStateBefore = false;

/// state machine for decoding CW
enum DECODER_STATES  {LOW_, HIGH_, INTERELEMENT_, INTERCHAR_};
DECODER_STATES decoderState = LOW_;


///////////////////////////////////////////////////////////
// The sampling frq will be 106.000 on ESp32             //
// because we need the tone in the center of the bins    //
// I set the tone to 698 Hz                              //
// then n the number of samples which give the bandwidth //
// can be (106000 / tone) * 1 or 2 or 3 or 4 etc         //
// init is 106000/698 = 152 *4 = 608 samples             //
// 152 will give you a bandwidth around 700 hz           //
// 304 will give you a bandwidth around 350 hz           //
// 608 will give you a bandwidth around 175 hz           //
///////////////////////////////////////////////////////////

float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;
const float sampling_freq = 106000.0;
const float target_freq = 698.0; /// adjust for your needs see above
int goertzel_n = 152;   //// you can use:         152, 304, 456 or 608 - thats the max buffer reserved in checktone()
                       ///// resulting bandwidth: 700, 350, 233 or 175 Hz, respectively
float bw;

///////////////////////////////////////
// Noise Blanker time which          //
// will be computed based on speed?? //
///////////////////////////////////////
int nbtime = 7;  /// ms noise blanker

unsigned long startTimeHigh;
unsigned long highDuration;
//long lasthighduration;
//long hightimesavg;
//long lowtimesavg;
long startTimeLow;
long lowDuration;
boolean stop = false;

unsigned long ditAvg, dahAvg;     /// average values of dit and dah lengths to decode as dit or dah and to adapt to speed change

volatile uint8_t  dit_rot = 0;
volatile unsigned long  dit_collector = 0;


////////////////////////////////////////////////////////////////////
// encoder subroutines
/// interrupt service routine - needs to be positioned BEFORE all other functions, including setup() and loop()
/// interrupt service routine

void IRAM_ATTR isr ()  {                    // Interrupt service routine is executed when a HIGH to LOW transition is detected on CLK
//if (micros()  > (IRTime + 1000) ) {
portENTER_CRITICAL_ISR(&mux);

    int sig2 = digitalRead(PinDT); //MSB = most significant bit
    int sig1 = digitalRead(PinCLK); //LSB = least significant bit
    delayMicroseconds(125);                 // this seems to improve the responsiveness of the encoder and avoid any bouncing

    int8_t thisState = sig1 | (sig2 << 1);
    if (_oldState != thisState) {
      stateRegister = (stateRegister << 2) | thisState;
      if (thisState == LATCHSTATE) {
        
          if (stateRegister == 135 )
            encoderPos = 1;
          else if (stateRegister == 75)
            encoderPos = -1;
          else
            encoderPos = 0;
        }
    _oldState = thisState;
    } 
portEXIT_CRITICAL_ISR(&mux);
}



int IRAM_ATTR checkEncoder() {
  int t;
  
  portENTER_CRITICAL(&mux);

  t = encoderPos;
  if (encoderPos) {
    encoderPos = 0;
    portEXIT_CRITICAL(&mux);
    return t;
  } else {
    portEXIT_CRITICAL(&mux);
    return 0;
  }
}


////////////////////////   S E T U P /////////////////////////////

void setup()
{
 
  Serial.begin(115200);
  delay(200); // give me time to bring up serial monitor

  // enable Vext
  #if BOARDVERSION == 3
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext,LOW);
  #endif

  
  // set up the encoder - we need external pull-ups as the pins used do not have built-in pull-ups!
  pinMode(PinCLK,INPUT_PULLUP);
  pinMode(PinDT,INPUT_PULLUP);  
  pinMode(keyerPin, OUTPUT);        // we can use the built-in LED to show when the transmitter is being keyed
  pinMode(leftPin, INPUT);          // external keyer left paddle
  pinMode(rightPin, INPUT);         // external keyer right paddle

  /// enable deep sleep
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, (esp_sleep_ext1_wakeup_mode_t) 0); //1 = High, 0 = Low
  analogSetAttenuation(ADC_0db);


// we MUST reset the OLED RST pin for 50 ms! for the old board only, but as it does not hurt we do it anyway
//#if BOARDVERSION == 2
  pinMode(OLED_RST,OUTPUT);
  digitalWrite(OLED_RST, LOW);     // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLED_RST, HIGH);    // while OLED is running, must set GPIO16 in high
//# endif

 // init display
  
  display.init();
  display.flipScreenVertically();  // rotate 180°  when USB is to the left of the module
  display.clear();
  display.display();
  adcAttachPin(batteryPin);
  analogSetPinAttenuation(batteryPin,ADC_11db);  

// set up PWMs for tone generation
  ledcSetup(toneChannel, toneFreq, pwmResolution);
  ledcAttachPin(LF_Pin, toneChannel);
  
  ledcSetup(lineOutChannel, toneFreq, pwmResolution);
  ledcAttachPin(lineOutPin, lineOutChannel);                                      ////// change this for real version - no line out currntly
  
  ledcSetup(volChannel, volFreq, pwmResolution);
  ledcAttachPin(HF_Pin, volChannel);
  
  ledcWrite(toneChannel,0);
  ledcWrite(lineOutChannel,0);

  //call ISR when any high/low changed seen
  //on any of the enoder pins
  attachInterrupt (digitalPinToInterrupt(PinDT), isr, CHANGE);   
  attachInterrupt (digitalPinToInterrupt(PinCLK), isr, CHANGE);
 
  encoderPos = 0;           /// this is the encoder position

/// set up for encoder button
  pinMode(modeButtonPin, INPUT);
  pinMode(volButtonPin, INPUT_PULLUP);               // external pullup for all GPIOS > 32 with ESP32-LORA
                                                     // wake up also works without external pullup! Interesting!
  
  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  modeButton.debounceTime   = 11;   // Debounce timer in ms
  modeButton.multiclickTime = 220;  // Time limit for multi clicks
  modeButton.longClickTime  = 350; // time until "held-down clicks" register

  volButton.debounceTime   = 11;   // Debounce timer in ms
  volButton.multiclickTime = 220;  // Time limit for multi clicks
  volButton.longClickTime  = 350; // time until "held-down clicks" register



  // to calibrate sensors, we record the values in untouched state
  initSensors();
  
  // read preferences from non-volatile storage
  // if version cannot be read, we have a new ESP32 and need to write the preferences first
  readPreferences("morserino");

  if (p_lcwoKochSeq) kochChars = lcwoKochChars;
  else kochChars = morserinoKochChars;

   
  //// populate the array for abbreviations and words according to length and Koch filter
  createKochWords(p_wordLength, p_kochFilter) ;  // 
  createKochAbbr(p_abbrevLength, p_kochFilter);


/// check if BLACK knob has been pressed on startup - if yes, we have to perform LoRa Setup
  delay(50);
  if (SPIFFS.begin() && digitalRead(modeButtonPin) == LOW)   {        // BLACK was pressed at start-up - checking for SPIFF so that programming 1st time w/o pull-up shows menu
     display.clear();
     display.display();
     printOnStatusLine(true, 0,  "Release BLACK");
      while (digitalRead(modeButtonPin) == LOW)      // wait until released
      ;
    loraSystemSetup();
  }

  /// set up quickstart - this should only be done once at startup - after successful quickstart we disable it to allow normal menu operation
  quickStart = p_quickStart;


////////////  Setup for LoRa
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(p_loraQRG,PABOOST)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setFrequency(p_loraQRG);                       /// default = 434.150 MHz - Region 1 ISM Band, can be changed by system setup
  LoRa.setSpreadingFactor(7);                         /// default
  LoRa.setSignalBandwidth(250E3);                     /// 250 kHz
  LoRa.noCrc();                                       /// we use error correction
  LoRa.setSyncWord(p_loraSyncW);                      /// the default would be 0x34
  
  // register the receive callback
  LoRa.onReceive(onReceive);
  /// initialise the serial number
  loRaSerial = random(64);

  
  ///////////////////////// mount (or create) SPIFFS file system
    #define FORMAT_SPIFFS_IF_FAILED true

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){     ///// if SPIFFS cannot be mounted, it does not exist. So create  (format) it, and mount it
        //Serial.println("SPIFFS Mount Failed");
        return;
    }
  //////////////////////// create file player.txt if it does not exist|
  const char * defaultFile = "This is just an initial dummy file for the player. Dies ist nur die anfänglich enthaltene Standarddatei für den Player.\n"
                             "Did you not upload your own file? Hast du keine eigene Datei hochgeladen?";
                             
    if (!SPIFFS.exists("/player.txt")) {                                    // file does not exist, therefor we create it from the text above
        file = SPIFFS.open("/player.txt", FILE_WRITE);
        if(!file){
            Serial.println("- failed to open file for writing");
            return;
        }
        if(file.print(defaultFile)){
            ;
        } else {
            Serial.println("- write failed");
        }
        file.close();
    }    
    displayStartUp();

    ///delay(2500);  //// just to be able to see the startup screen for a while - is contained in displayStartUp()

  ////

  menu_();
} /////////// END setup()


// display startup screen and check battery status
void displayStartUp() {
  String stat = "Morserino-32 ";
  display.clear();
  display.display();
  stat += String(p_loraQRG / 10000);
 /*
  switch (p_loraBand) {
    case 0 : stat += "433M ";
      break;
    case 1 : stat += "868M ";
      break;
    case 2 : stat += "920M ";
      break;
  } */
  printOnStatusLine(true, 0, stat);
  
  printOnScroll(0, REGULAR, 0, "Ver. " );
  
  sprintf(numBuffer,"%2i", VERSION_MAJOR);
  printOnScroll(0, REGULAR, 5, numBuffer);
  
  printOnScroll(0, REGULAR, 7,  "." );
  sprintf(numBuffer,"%1i", VERSION_MINOR);
  printOnScroll(0, REGULAR, 8, numBuffer);

  if (BETA) printOnScroll(0, REGULAR, 10, "beta");

  printOnScroll(1, REGULAR, 0, "© 2018 f.");

  uint16_t volt = batteryVoltage();
  //Serial.println(volt);
  
  if (volt < 3150) {
    display.clear(); display.display();
    displayEmptyBattery();                            // warn about empty battery and go to deep sleep (again)
  }
  else {
      displayBatteryStatus(volt);
  }
  delay(3000);
}

//////// System Setup / LoRa Setup ///// Called when BALCK knob is pressed @ startup

void loraSystemSetup() {
    displayKeyerPreferencesMenu(posLoraBand);
    adjustKeyerPreference(posLoraBand);
    displayKeyerPreferencesMenu(posLoraQRG);
    adjustKeyerPreference(posLoraQRG);
    /// now store chosen values in Preferences
    pref.begin("morserino", false);             // open the namespace as read/write
    pref.putUChar("loraBand", p_loraBand);
    pref.putUInt("loraQRG", p_loraQRG);
    pref.end();
}



enum AutoStopModes {off, stop1, stop2}  autoStop = off;

///////////////////////// THE MAIN LOOP - do this OFTEN! /////////////////////////////////

void loop() {
// static uint64_t loopC = 0;
   int t;

   boolean activeOld = active;
   checkPaddles();
   switch (morseState) {
      case morseKeyer:    if (doPaddleIambic(leftKey, rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          break;
      case loraTrx:      if (doPaddleIambic(leftKey, rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          generateCW();
                          break;
      case morseTrx:      if (doPaddleIambic(leftKey, rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }  
                          doDecode();
                          if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
                          break;    
      case morseGenerator:  if ((autoStop == stop1) || leftKey  || rightKey)   {                                    // touching a paddle starts and stops the generation of code
                          // for debouncing:
                          while (checkPaddles() )
                              ;                                                           // wait until paddles are released

                          if (p_autoStop) {
                            active = (autoStop == off);
                            switch (autoStop) {
                              case off : {
                                  break;
                                  //
                                }
                              case stop1: {
                                  autoStop = stop2;
                                  break;
                                }
                              case stop2: {
                                  printToScroll(REGULAR, "\n");
                                  autoStop = off;
                                  break;
                                }
                            }
                          }
                          else {
                            active = !active;
                            autoStop = off;
                          }

                          //delay(100);
                          } /// end squeeze
                          
                          ///// check stopFlag triggered by maxSequence
                          if (stopFlag) {
                            active = stopFlag = false;
                          }
                          if (activeOld != active) {
                            if (!active) {
                               keyOut(false, true, 0, 0);
                               printOnStatusLine(true, 0, "Continue w/ Paddle");
                            }
                          else {
                               cleanStartSettings();        
                            }
                          }
                          if (active)
                            generateCW();
                          break;
      case echoTrainer:                             ///// check stopFlag triggered by maxSequence
                          if (stopFlag) {
                            active = stopFlag = false;
                            keyOut(false, true, 0, 0);
                            printOnStatusLine(true, 0, "Continue w/ Paddle");
                          }
                          if (!active && (leftKey  || rightKey))   {                       // touching a paddle starts  the generation of code
                              // for debouncing:
                              while (checkPaddles() )
                                  ;                                                           // wait until paddles are released
                              active = !active;
             
                              cleanStartSettings();
                          } /// end touch to start
                          if (active)
                          switch (echoTrainerState) {
                            case  START_ECHO:   
                            case  SEND_WORD:
                            case  REPEAT_WORD:  echoResponse = ""; generateCW();
                                                break;
                            case  EVAL_ANSWER:  echoTrainerEval();
                                                break;
                            case  COMPLETE_ANSWER:                    
                            case  GET_ANSWER:   if (doPaddleIambic(leftKey, rightKey)) 
                                                    return;                             // we are busy keying and so need a very tight loop !
                                                break;
                            }                              
                            break;
      case morseDecoder: doDecode();
                         if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
      default:            break;
            
                        
  } // end switch and code depending on state of metaMorserino

/// if we have time check for button presses

    modeButton.Update();
    volButton.Update();
    
    switch (volButton.clicks) {
      case 1:   if (encoderState == scrollMode) {
                    if (morseState != morseDecoder)
                        encoderState = speedSettingMode;
                    else
                        encoderState = volumeSettingMode;
                    relPos = maxPos;
                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    displayScrollBar(false);
                } else if (encoderState == volumeSettingMode && morseState != morseDecoder) {          //  single click toggles encoder between speed and volume
                  encoderState = speedSettingMode;
                  pref.begin("morserino", false);                     // open the namespace as read/write
                  if (pref.getUChar("sidetoneVolume") != p_sidetoneVolume)
                      pref.putUChar("sidetoneVolume", p_sidetoneVolume);  // store the last volume, if it has changed
                  pref.end();
                  displayCWspeed();
                  displayVolume();
                }
                else {
                  encoderState = volumeSettingMode;
                  displayCWspeed();
                  displayVolume();
                }
                break;
      case -1:  if (encoderState == scrollMode) {
                    encoderState = (morseState == morseDecoder ? volumeSettingMode : speedSettingMode);
                    relPos = maxPos;
                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    displayScrollBar(false);
                }       
                else {
                    encoderState = scrollMode;
                    displayScrollBar(true);
                }
                break;
    }
   
    switch (modeButton.clicks) {                                // actions based on enocder button
       case -1:   menu_();                                       // long click exits current mode and goes to top menu
                  return;
       case 1:    if (morseState == morseGenerator || morseState == echoTrainer) {//  start/stop in trainer modi, in others does nothing currently
                  active = !active;
                  if (!active) {
                        //digitalWrite(keyerPin, LOW);           // turn the LED off, unkey transmitter, or whatever
                        //pwmNoTone(); 
                        keyOut(false, true, 0, 0);
                        printOnStatusLine(true, 0, "Continue w/ Paddle");
                  }
                  else {
                    cleanStartSettings();
                  }
                        
              }
              break;
       case 2:  setupPreferences(p_menuPtr);                               // double click shows the preferences menu (true would select a specific option only)
                display.clear();                                  // restore display
                displayTopLine();
                if (morseState == morseGenerator || morseState == echoTrainer) 
                    stopFlag = true;                                  // we stop what we had been doing
                else
                    stopFlag = false;
                //startFirst = true;
                //firstTime = true;
     default: break;
    }
    
/// and we have time to check the encoder
     if ((t = checkEncoder())) {
        //Serial.println("t: " + String(t));
        pwmClick(p_sidetoneVolume);         /// click
        switch (encoderState) {
          case speedSettingMode:  
                                  changeSpeed(t);
                                  break;
          case volumeSettingMode: 
                                  p_sidetoneVolume += (t*10)+11;
                                  p_sidetoneVolume = constrain(p_sidetoneVolume, 11, 111) -11;
                                  //Serial.println(p_sidetoneVolume);
                                  displayVolume();
                                  break;
          case scrollMode:
                                  if (t == 1 && relPos < maxPos ) {        // up = scroll towards bottom
                                    ++relPos;
                                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                  }
                                  else if (t == -1 && relPos > 0) {
                                    --relPos;
                                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                  }
                                  //encoderPos = 0;
                                  //portEXIT_CRITICAL(&mux);
                                  displayScrollBar(true);
                                  break;
          }
    } // encoder 
    checkShutDown(false);         // check for time out
    
}     /////////////////////// end of loop() /////////


void cleanStartSettings() {
    clearText = "";
    CWword = "";
    echoTrainerState = START_ECHO;
    generatorState = KEY_UP; 
    keyerState = IDLE_STATE;
    interWordTimer = 4294967000;                 // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
    genTimer = millis()-1;                       // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc... 
    wordCounter = 0;                             // reset word counter for maxSequence
    startFirst = true;
    displayTopLine();
}


////// The MENU


void menu_() {
   uint8_t newMenuPtr = p_menuPtr;
   uint8_t disp = 0;
   int t, command;
   
     //// initialize a few things now
     //Serial.println("THE MENU");
    ///updateTimings(); // now done after reading preferences
    LoRa.idle();
    //keyerState = IDLE_STATE;
    active = false;
    //startFirst = true;
    cleanStartSettings();
    /*
    clearText = "";
    CWword = "";
    echoTrainerState = START_ECHO;
    generatorState = KEY_UP; 
    keyerState = IDLE_STATE;
    interWordTimer = 4294967000;                 // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
    genTimer = millis()-1;                       // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc... 
    */
    clearScroll();                  // clear the buffer
    clearScrollBuffer();

    keyOut(false, true, 0, 0);
    keyOut(false, false, 0, 0);
    encoderState = speedSettingMode;             // always start with this encoderstate (decoder will change it anyway)
    currentOptions = allOptions;                 // this is the array of options when we double click the BLACK button: while in menu, you can change all of them
    currentOptionSize = SizeOfArray(allOptions);
    pref.begin("morserino", false);              // open the namespace as read/write
    if ((p_fileWordPointer != pref.getUInt("fileWordPtr")))   // update word pointer if necessary (if we ran player before)
       pref.putUInt("fileWordPtr", p_fileWordPointer);
    pref.end(); 
    file.close();                               // just in case it is still open....
    display.clear();
    
    while (true) {                          // we wait for a click (= selection)
        if (disp != newMenuPtr) {
          disp = newMenuPtr;
          menuDisplay(disp);
        }
        if (quickStart) {
            quickStart = false;
            command = 1;
            delay(500);
            printOnScroll(2, REGULAR, 1, "QUICK START");
            display.display();
            delay(500);
            display.clear();
        }
        else {
            modeButton.Update();
            command = modeButton.clicks;
        }

        switch (command) {                                          // actions based on enocder button
          case 2: if (setupPreferences(newMenuPtr))                       // all available options when called from top menu
                    newMenuPtr = p_menuPtr;
                  menuDisplay(newMenuPtr);
                  break;
          case 1: // check if we have a submenu or if we execute the selection
                  //Serial.println("newMP: " + String(newMenuPtr) + " navi: " + String(menuNav[newMenuPtr][naviDown]));
                  if (menuNav[newMenuPtr][naviDown] == 0) {
                      p_menuPtr = newMenuPtr;
                      disp = 0;
                      if (p_menuPtr < _wifi) {                        // remember last executed, unless it is a wifi function or shutdown
                          pref.begin("morserino", false);             // open the namespace as read/write
                          pref.putUChar("lastExecuted", p_menuPtr);   // store last executed command
                          pref.end();                                 // close namespace
                      }
                      if (menuExec())
                        return;
                  } else {
                      newMenuPtr = menuNav[newMenuPtr][naviDown];
                  }
                  break;
          case -1:  // we need to go one level up, if possible
                  if (menuNav[newMenuPtr][naviUp] != 0) 
                      newMenuPtr = menuNav[newMenuPtr][naviUp];
          default: break;
        }

       if ((t=checkEncoder())) {
          //pwmClick(p_sidetoneVolume);         /// click 
          newMenuPtr =  menuNav [newMenuPtr][(t == -1) ? naviLeft : naviRight];
       }

       volButton.Update();
    
       switch (volButton.clicks) {
          case -1:  audioLevelAdjust();                         /// for adjusting line-in audio level (at the same time keying tx and sending oudio on line-out
                    display.clear();
                    menuDisplay(disp);
                    break;
          /* case  3:  wifiFunction();                                  /// configure wifi, upload file or firmware update
                    break;
          */
       }
       checkShutDown(false);                  // check for time out   
  } // end while - we leave as soon as the button has been pressed
} // end menu_() 


void menuDisplay(uint8_t ptr) {
  //Serial.println("Level: " + (String) menuNav [ptr][naviLevel] + " " + menuText[ptr]);
  uint8_t oneUp = menuNav[ptr][naviUp];
  uint8_t twoUp = menuNav[oneUp][naviUp];
  uint8_t oneDown = menuNav[ptr][naviDown];
    
  printOnStatusLine(true, 0,  "Select Modus:     ");

  clearLine(0); clearLine(1); clearLine(2);                       // delete previous content
  
  /// level 0: top line, possibly ".." on line 1
  /// level 1: higher level on 0, item on 1, possibly ".." on 2
  /// level 2: higher level on 1, highest level on 0, item on 2
  switch (menuNav [ptr][naviLevel]) {
    case 2: printOnScroll(2, BOLD, 0, menuText[ptr]);
            printOnScroll(1, REGULAR, 0, menuText[oneUp]);
            printOnScroll(0, REGULAR, 0, menuText[twoUp]);
            break;
    case 1: if (oneDown)
                printOnScroll(2, REGULAR, 0, String(".."));
            printOnScroll(1, BOLD, 0, menuText[ptr]);
            printOnScroll(0, REGULAR, 0, menuText[oneUp]);
            break;
    case 0: 
            if (oneDown)
                printOnScroll(1, REGULAR, 0, String(".."));
            printOnScroll(0, BOLD, 0, menuText[ptr]);
            break;
  }
}

///////////// GEN_TYPE { RANDOMS, ABBREVS, WORDS, CALLS, MIXED, KOCH_MIXED, KOCH_LEARN };           // the things we can generate in generator mode


boolean menuExec() {                                          // return true if we should  leave menu after execution, true if we should stay in menu
  //Serial.println("Executing menu item " + String(p_menuPtr));

  uint32_t wcount = 0;
  
  kochActive = false;
  switch (p_menuPtr) {
    case  _keyer:  /// keyer
                currentOptions = keyerOptions;                // list of available options in keyer mode
                currentOptionSize = SizeOfArray(keyerOptions);
                morseState = morseKeyer;
                display.clear();
                printOnScroll(1, REGULAR, 0, "Start CW Keyer" );
                delay(500);
                display.clear();
                displayTopLine();
                printToScroll(REGULAR,"");      // clear the buffer
                clearPaddleLatches();
                keyTx = true;
                return true;
                break;
     case _genRand:
     case _genAbb:
     case _genWords:
     case _genCalls:
     case _genMixed:      /// generator
                generatorMode = (GEN_TYPE) (p_menuPtr - 3);                   /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                currentOptions = generatorOptions;                            // list of available options in generator mode
                currentOptionSize = SizeOfArray(generatorOptions);
                goto startTrainer;
     case _genPlayer:  
                generatorMode = (GEN_TYPE) (p_menuPtr - 3);                   /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                currentOptions = playerOptions;                               // list of available options in player mode
                currentOptionSize = SizeOfArray(playerOptions);
                file = SPIFFS.open("/player.txt");                            // open file
                //skip p_fileWordPointer words, as they have been played before
                wcount = p_fileWordPointer;
                p_fileWordPointer = 0;
                skipWords(wcount);
                
     startTrainer:
                startFirst = true;
                firstTime = true;
                morseState = morseGenerator;
                display.clear();
                printOnScroll(0, REGULAR, 0, "Generator     ");
                printOnScroll(1, REGULAR, 0, "Start/Stop:   ");
                printOnScroll(2, REGULAR, 0, "Paddle | BLACK");
                delay(1250);
                display.clear();
                displayTopLine();
                clearScroll();      // clear the buffer
                keyTx = true;
                return true;
                break;
      case  _echoRand:
      case  _echoAbb:
      case  _echoWords:
      case  _echoCalls:
      case  _echoMixed:
                currentOptions = echoTrainerOptions;                        // list of available options in echo trainer mode
                currentOptionSize = SizeOfArray(echoTrainerOptions);
                generatorMode = (GEN_TYPE) (p_menuPtr - 10);                /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                goto startEcho;
      case  _echoPlayer:    /// echo trainer
                generatorMode = (GEN_TYPE) (p_menuPtr - 10);                /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                currentOptions = echoPlayerOptions;                         // list of available options in echo player mode
                currentOptionSize = SizeOfArray(echoPlayerOptions);
                file = SPIFFS.open("/player.txt");                            // open file
                //skip p_fileWordPointer words, as they have been played before
                wcount = p_fileWordPointer;
                p_fileWordPointer = 0;
                skipWords(wcount);
       startEcho:
                startFirst = true;
                morseState = echoTrainer;
                echoStop = false;
                display.clear();
                printOnScroll(0, REGULAR, 0, generatorMode == KOCH_LEARN ? "New Character:" : "Echo Trainer:");
                printOnScroll(1, REGULAR, 0, "Start:       ");
                printOnScroll(2, REGULAR, 0, "Press paddle ");
                delay(1250);
                display.clear();
                displayTopLine();
                printToScroll(REGULAR,"");      // clear the buffer
                keyTx = false;
                return true;
                break;
      case  _kochSel: // Koch Select 
                displayKeyerPreferencesMenu(posKochFilter);
                adjustKeyerPreference(posKochFilter);
                writePreferences("morserino");
                //createKochWords(p_wordLength, p_kochFilter) ;  // update the arrays!
                //createKochAbbr(p_abbrevLength, p_kochFilter);
                return false;
                break;
      case  _kochLearn:   // Koch Learn New .  /// just a new generatormode....
                generatorMode = KOCH_LEARN;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochGenRand: // RANDOMS 
                generatorMode = RANDOMS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochEchoRand: // Koch Echo Random
                generatorMode = RANDOMS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _trxLora: // LoRa Transceiver
                currentOptions = loraTrxOptions;                            // list of available options in lora trx mode
                currentOptionSize = SizeOfArray(loraTrxOptions);
                morseState = loraTrx;
                display.clear();
                printOnScroll(1, REGULAR, 0, "Start LoRa Trx" );
                delay(600);
                display.clear();
                displayTopLine();
                printToScroll(REGULAR,"");      // clear the buffer
                clearPaddleLatches();
                keyTx = false;
                clearText = "";
                LoRa.receive();
                return true;
                break;
      case  _trxIcw: /// icw/ext TRX
                currentOptions = extTrxOptions;                            // list of available options in ext trx mode
                currentOptionSize = SizeOfArray(extTrxOptions);
                morseState = morseTrx;
                display.clear();
                printOnScroll(1, REGULAR, 0, "Start CW Trx" );
                clearPaddleLatches();
                keyTx = true;
                goto setupDecoder;

      case  _decode: /// decoder
                currentOptions = decoderOptions;                            // list of available options in lora trx mode
                currentOptionSize = SizeOfArray(decoderOptions);
                morseState = morseDecoder;
                  /// here we will do the init for decoder mode
                //trainerMode = false;
                encoderState = volumeSettingMode;
                keyTx = false;
                display.clear();
                printOnScroll(1, REGULAR, 0, "Start Decoder" );
      setupDecoder:
                speedChanged = true;
                delay(650);
                display.clear();
                displayTopLine();
                drawInputStatus(false);
                printToScroll(REGULAR,"");      // clear the buffer
                
                displayCWspeed();
                displayVolume();
                  
                /// set up variables for Goertzel Morse Decoder
                setupGoertzel();
                filteredState = filteredStateBefore = false;
                decoderState = LOW_;
                ditAvg = 60;
                dahAvg = 180;
                return true;
                break;
      case  _wifi_mac:
                WiFi.mode(WIFI_MODE_STA);               // init WiFi as client
                display.clear();
                display.display();
                printOnStatusLine(true, 0,  WiFi.macAddress());
                delay(2000);
                printOnScroll(0, REGULAR, 0, "RED: restart" );
                delay(1000);  
                while (true) {
                  checkShutDown(false);  // possibly time-out: go to sleep
                  if (digitalRead(volButtonPin) == LOW)
                    ESP.restart();
                }
                break;
      case  _wifi_config:
                startAP();          // run as AP to get WiFi credentials from user
                break;
      case _wifi_check:
                display.clear();
                display.display();
                printOnStatusLine(true, 0,  "Connecting... ");
                if (! wifiConnect())
                    ; //return false;  
                else {
                    printOnStatusLine(true, 0,  "Connected!    ");
                    printOnScroll(0, REGULAR, 0, p_wlanSSID);
                    printOnScroll(1, REGULAR, 0, WiFi.localIP().toString());
                }
                WiFi.mode( WIFI_MODE_NULL ); // switch off WiFi                      
                delay(1000);
                printOnScroll(2, REGULAR, 0, "RED: return" );
                while (true) {
                      checkShutDown(false);  // possibly time-out: go to sleep
                      if (digitalRead(volButtonPin) == LOW)
                        return false;
                }
                break;
      case _wifi_upload:
                uploadFile();       // upload a text file
                break;
      case _wifi_update:
                updateFirmware();   // run OTA update
                break;
      case  _goToSleep: /// deep sleep
                checkShutDown(true);
      default:  break;
  }
  return false;
}   /// end menuExec()



///////////////////
// we use the paddle for iambic keying
/////

boolean doPaddleIambic (boolean dit, boolean dah) {
  boolean paddleSwap;                      // temp storage if we need to swap left and right
  static long ktimer;                      // timer for current element (dit or dah)
  static long curtistimer;                 // timer for early paddle latch in Curtis mode B+
  static long latencytimer;                // timer for "muting" paddles for some time in state INTER_ELEMENT
  unsigned int pitch;

  if (!p_didah)   {              // swap left and right values if necessary!
      paddleSwap = dit; dit = dah; dah = paddleSwap; 
  }
      

  switch (keyerState) {                                         // this is the keyer state machine
     case IDLE_STATE:
         // display the interword space, if necessary
         if (millis() > interWordTimer) {
             if (morseState == loraTrx)    {                    // when in Trx mode
                 cwForLora(3);
                 sendWithLora();                        // finalise the string and send it to LoRA
             }
             printToScroll(REGULAR, " ");                       // output a blank
             interWordTimer = 4294967000;                       // almost the biggest possible unsigned long number :-) - do not output extra spaces!
             if (echoTrainerState == COMPLETE_ANSWER)   {       // change the state of the trainer at end of word
                echoTrainerState = EVAL_ANSWER;
                return false;
             }
         }
        
       // Was there a paddle press?
        if (dit || dah ) {
            updatePaddleLatch(dit, dah);  // trigger the paddle latches
            if (morseState == echoTrainer)   {      // change the state of the trainer at end of word
                echoTrainerState = COMPLETE_ANSWER;
             }
            treeptr = 0;
            if (dit) {
                setDITstate();          // set next state
                DIT_FIRST = true;          // first paddle pressed after IDLE was a DIT
            }
            else {
                setDAHstate();  
                DIT_FIRST = false;         // first paddle was a DAH
            }
        }
        else {
           if (echoTrainerState == GET_ANSWER && millis() > genTimer) {
            echoTrainerState = EVAL_ANSWER;
         } 
         return false;                // we return false if there was no paddle press in IDLE STATE - Arduino can do other tasks for a bit
        }
        break;

    case DIT:
    /// first we check that we have waited as defined by ACS settings
            if ( p_ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for (3 or whatever) dits...
              break;
            clearPaddleLatches();                           // always clear the paddle latches at beginning of new element
            keyerControl |= DIT_LAST;                        // remember that we process a DIT

            ktimer = ditLength;                              // prime timer for dit
            switch ( p_keyermode ) {
              case IAMBICB:  curtistimer = 2 + (ditLength * p_curtisBDotTiming / 100);   
                             break;                         // enhanced Curtis mode B starts checking after some time
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = ditLength;        // no early paddle checking in Curtis mode A Ultimatic mode oder Non-squeeze
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
            
    case DAH:
            if ( p_ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for 3 dits...
              break;
            clearPaddleLatches();                          // clear the paddle latches
            keyerControl &= ~(DIT_LAST);                    // clear dit latch  - we are not processing a DIT
            
            ktimer = dahLength;
            switch (p_keyermode) {
              case IAMBICB:  curtistimer = 2 + (dahLength * p_curtisBTiming / 100);    // enhanced Curtis mode B starts checking after some time
                             break;
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = dahLength;        // no early paddle checking in Curtis mode A or Ultimatic mode
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
     

      
    case KEY_START:
          // Assert key down, start timing, state shared for dit or dah
          pitch = notes[p_sidetoneFreq];
          if ((morseState == echoTrainer || morseState == loraTrx) && p_echoToneShift != 0) {
             pitch = (p_echoToneShift == 1 ? pitch * 18 / 17 : pitch * 17 / 18);        /// one half tone higher or lower, as set in parameters in echo trainer mode
          }
           //pwmTone(pitch, p_sidetoneVolume, true);
           //keyTransmitter();
           keyOut(true, true, pitch, p_sidetoneVolume);
           ktimer += millis();                     // set ktimer to interval end time          
           curtistimer += millis();                // set curtistimer to curtis end time
           keyerState = KEYED;                     // next state
           break;
 
    case KEYED:
                                                   // Wait for timers to expire
           if (millis() > ktimer) {                // are we at end of key down ?
               //digitalWrite(keyerPin, LOW);        // turn the LED off, unkey transmitter, or whatever
               //pwmNoTone();                      // stop side tone
               keyOut(false, true, 0, 0);
               ktimer = millis() + ditLength;    // inter-element time
               latencytimer = millis() + ((p_latency-1) * ditLength / 8);
               keyerState = INTER_ELEMENT;       // next state
            }
            else if (millis() > curtistimer ) {     // in Curtis mode we check paddle as soon as Curtis time is off
                 if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
                 else
                    updatePaddleLatch(dit, false);  
                 // updatePaddleLatch(dit, dah);       // but we remain in the same state until element time is off! 
            }
            break;
 
    case INTER_ELEMENT:
            //if ((p_keyermode != NONSQUEEZE) && (millis() < latencytimer)) {     // or should it be p_keyermode > 2 ? Latency for Ultimatic mode?
            if (millis() < latencytimer) {
              if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
              else
                    updatePaddleLatch(dit, false);
              // updatePaddleLatch(dit, dah); 
            }
            else {
                updatePaddleLatch(dit, dah);          // latch paddle state while between elements       
                if (millis() > ktimer) {               // at end of INTER-ELEMENT
                    switch(keyerControl) {
                          case 3:                                         // both paddles are latched
                          case 7: 
                                  switch (p_keyermode) {
                                      case NONSQUEEZE:  if (DIT_FIRST)                      // when first element was a DIT
                                                               setDITstate();            // next element is a DIT again
                                                        else                                // but when first element was a DAH
                                                               setDAHstate();            // the next element is a DAH again! 
                                                        break;
                                      case ULTIMATIC:   if (DIT_FIRST)                      // when first element was a DIT
                                                               setDAHstate();            // next element is a DAH
                                                        else                                // but when first element was a DAH
                                                               setDITstate();            // the next element is a DIT! 
                                                        break;
                                      default:          if (keyerControl & DIT_LAST)     // Iambic: last element was a dit - this is case 7, really
                                                            setDAHstate();               // next element will be a DAH
                                                        else                                // and this is case 3 - last element was a DAH
                                                            setDITstate();               // the next element is a DIT                         
                                   }
                                   break;
                                                                          // dit only is latched, regardless what was last element  
                          case 1:
                          case 5:  
                                   setDITstate();
                                   break;
                                                                          // dah only latched, regardless what was last element
                          case 2:
                          case 6:  
                                   setDAHstate();
                                   break;
                                                                          // none latched, regardless what was last element
                          case 0:
                          case 4:  
                                   keyerState = IDLE_STATE;               // we are at the end of the character and go back into IDLE STATE
                                   displayMorse();                        // display the decoded morse character(s)
                                   if (morseState == loraTrx)
                                      cwForLora(0);
                                   
                                   ++charCounter;                         // count this character
                                   // if we have seen 12 chars since changing speed, we write the config to preferences (speed and left & right thresholds)
                                   if (charCounter == 12) {
                                      pref.begin("morserino", false);             // open the namespace as read/write
                                      pref.putUChar("wpm", p_wpm);
                                      pref.putUChar("tLeft", p_tLeft);
                                      pref.putUChar("tRight", p_tRight);
                                      pref.end();
                                   }
                                   if (p_ACSlength > 0)
                                        acsTimer = millis() + p_ACSlength * ditLength; // prime the ACS timer
                                   if (morseState == morseKeyer || morseState == loraTrx || morseState == morseTrx)
                                      interWordTimer = millis() + 5*ditLength;
                                   else
                                       interWordTimer = millis() + interWordSpace;  // prime the timer to detect a space between characters
                                                                              // nominally 7 dit-lengths, but we are not quite so strict here in keyer or TrX mode,
                                                                              // use the extended time in echo trainer mode to allow longer space between characters, 
                                                                              // like in listening
                                   keyerControl = 0;                          // clear all latches completely before we go to IDLE
                          break;
                    } // switch keyerControl : evaluation of flags
                }
            } // end of INTER_ELEMENT
  } // end switch keyerState - end of state machine

  if (keyerControl & 3)                                               // any paddle latch?                            
    return true;                                                      // we return true - we processed a paddle press
  else
    return false;                                                     // when paddle latches are cleared, we return false
} /////////////////// end function doPaddleIambic()



//// this function checks the paddles (touch or external), returns true when a paddle has been activated, 
///// and sets the global variable leftKey and rightKey accordingly


boolean checkPaddles() {
  static boolean oldL = false, newL, oldR = false, newR;
  int left, right;
  static long lTimer = 0, rTimer = 0;
  const int debDelay = 750;       // debounce time = 0,75  ms
  
  /* intral and external paddle are now working in parallel - the parameter p_extPaddle is used to indicate reverse polarity of external paddle
  */
  left = p_useExtPaddle ? rightPin : leftPin;
  right = p_useExtPaddle ? leftPin : rightPin;
  sensor = readSensors(LEFT, RIGHT);
  newL = (sensor >> 1) | (!digitalRead(left)) ;
  newR = (sensor & 0x01) | (!digitalRead(right)) ;

  if ((p_keyermode == NONSQUEEZE) && newL && newR) 
    return (leftKey || rightKey);

  if (newL != oldL)
      lTimer = micros();
  if (newR != oldR)
      rTimer = micros();
  if (micros() - lTimer > debDelay)
      if (newL != leftKey) 
          leftKey = newL;
  if (micros() - rTimer > debDelay)
      if (newR != rightKey) 
          rightKey = newR;       

  oldL = newL;
  oldR = newR;
  
  return (leftKey || rightKey);
}

///
/// Keyer subroutines
///

// update the paddle latches in keyerControl
void updatePaddleLatch(boolean dit, boolean dah)
{ 
    if (dit)
      keyerControl |= DIT_L;
    if (dah)
      keyerControl |= DAH_L;
}

// clear the paddle latches in keyer control
void clearPaddleLatches ()
{
    keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
}

// functions to set DIT and DAH keyer states
void setDITstate() {
  keyerState = DIT;
  treeptr = CWtree[treeptr].dit;
  if (morseState == loraTrx)
      cwForLora(1);                         // build compressed string for LoRA
}

void setDAHstate() {
  keyerState = DAH;
  treeptr = CWtree[treeptr].dah;
  if (morseState == loraTrx)
      cwForLora(2);
}


// toggle polarity of paddles
void togglePolarity () {
      p_didah = !p_didah; 
     //displayPolarity();
}
  

/// display decoded morse code (and store it in echoTrainer
void displayMorse() {
  String symbol;
  symbol.reserve(6);
  if (treeptr == 0)
    return;
  symbol = CWtree[treeptr].symb;
  //Serial.println("Symbol: " + symbol + " treeptr: " + String(treeptr));
  printToScroll( REGULAR, symbol);
  if (morseState == echoTrainer) {                /// store the character in the response string
      symbol.replace("<as>", "S");
      symbol.replace("<ka>", "A");
      symbol.replace("<kn>", "N");
      symbol.replace("<sk>", "K");
      symbol.replace("<ve>", "V");
      symbol.replace("<ch>", "H");
      echoResponse.concat(symbol);
  }
  treeptr = 0;                                    // reset tree pointer
}   /// end of displayMorse()



//// functions for generating a tone....

void pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut) { // frequency in Hertz, volume in range 0 - 100; we use 10 bit resolution
  const uint16_t vol[] = {0, 1,  2, 3, 16, 150, 380, 580, 700, 880, 1023};
  int i = constrain(volume/10, 0, 10);
  //Serial.println(vol[i]);
  //Serial.println(frequency);
  if (lineOut) {
      ledcWriteTone(lineOutChannel, (double) frequency);
      ledcWrite(lineOutChannel, dutyCycleFiftyPercent);
  }

  ledcWrite(volChannel, volFreq);
  ledcWrite(volChannel, vol[i]);
  ledcWriteTone(toneChannel, frequency);
 

  if (i == 0 ) 
      ledcWrite(toneChannel, dutyCycleZero);
  else if (i > 3)
      ledcWrite(toneChannel, dutyCycleFiftyPercent);
  else
      ledcWrite(toneChannel, i*i*i + 4 + 2*i);          /// an ugly hack to allow for lower volumes on headphones
    
   
  
}


void pwmNoTone() {      // stop playing a tone by changing duty cycle of the tone to 0
  ledcWrite(toneChannel, dutyCycleTwentyPercent);
  ledcWrite(lineOutChannel, dutyCycleTwentyPercent);
  delayMicroseconds(125);
  ledcWrite(toneChannel, dutyCycleZero);
  ledcWrite(lineOutChannel, dutyCycleZero);
  
}


void pwmClick(unsigned int volume) {                        /// generate a click on the speaker
    if (!p_encoderClicks)
      return;
    pwmTone(250,volume, false);
    delay(6);
    pwmTone(280,volume, false);
    delay(5);
    pwmNoTone();
}


//////// Display the status line in CW Keyer Mode
//////// Layout of top line:
//////// Tch ul 15 WpM
//////// 0    5    0

void displayTopLine() {
  clearStatusLine();

  // printOnStatusLine(true, 0, (p_useExtPaddle ? "X " : "T "));          // we do not show which paddle is in use anymore
  if (morseState == morseGenerator) 
    printOnStatusLine(true, 1,  p_wordDoubler ? "x2" : "  ");
  else {
    switch (p_keyermode) {
      case IAMBICA:   printOnStatusLine(false, 2,  "A "); break;          // Iambic A (no paddle eval during dah)
      case IAMBICB:   printOnStatusLine(false, 2,  "B "); break;          // orig Curtis B mode: paddle eval during element
      case ULTIMATIC: printOnStatusLine(false, 2,  "U "); break;          // Ultimatic Mode
      case NONSQUEEZE: printOnStatusLine(false, 2,  "N "); break;         // Non-squeeze mode
    }
  }

  displayCWspeed();                                     // update display of CW speed
  if ((morseState == loraTrx ) || (morseState == morseGenerator  && p_loraTrainerMode == true))
      dispLoraLogo();

  displayVolume();                                     // sidetone volume
  display.display();
}

void dispLoraLogo() {     // display a small logo in the top right corner to indicate we operate with LoRa
  display.setColor(BLACK);
  display.drawXbm(121, 2, lora_width, lora_height, lora_bits);
  display.setColor(WHITE);
  display.display();
}

//////// Display the current CW speed
/////// pos 7-8, "Wpm" on 10-12

void displayCWspeed () {
  if (( morseState == morseGenerator || morseState ==  echoTrainer )) 
      sprintf(numBuffer, "(%2i)", effWpm);   
  else sprintf(numBuffer, "    ");
  
  printOnStatusLine(false, 3,  numBuffer);                                         // effective wpm
  
  sprintf(numBuffer, "%2i", p_wpm);
  printOnStatusLine(encoderState == speedSettingMode ? true : false, 7,  numBuffer);
  printOnStatusLine(false, 10,  "WpM");
  display.display();
}


/// function to read sensors:
/// read both left and right twice, repeat reading if it returns 0
/// return a binary value, depending on a (adaptable?) threshold:
/// 0 = nothing touched,  1= right touched, 2 = left touched, 3 = both touched
/// binary:   00          01                10                11

uint8_t readSensors(int left, int right) {
  //static boolean first = true;
  uint8_t v, lValue, rValue;
  
  while ( !(v=touchRead(left)) )
    ;                                       // ignore readings with value 0
  lValue = v;
   while ( !(v=touchRead(right)) )
    ;                                       // ignore readings with value 0
  rValue = v;
  while ( !(v=touchRead(left)) )
    ;                                       // ignore readings with value 0
  lValue = (lValue+v) /2;
   while ( !(v=touchRead(right)) )
    ;                                       // ignore readings with value 0
  rValue = (rValue+v) /2;

  if (lValue < (p_tLeft+10))     {           //adaptive calibration
      //if (first) Serial.println("p-tLeft " + String(p_tLeft));
      //if (first) Serial.print("lValue: "); if (first) Serial.println(lValue);
      //printOnScroll(0, INVERSE_BOLD, 0,  String(lValue) + " " + String(p_tLeft));
      p_tLeft = ( 7*p_tLeft +  ((lValue+lUntouched) / SENS_FACTOR) ) / 8;
      //Serial.print("p_tLeft: "); Serial.println(p_tLeft);
  }
  if (rValue < (p_tRight+10))     {           //adaptive calibration
      //if (first) Serial.println("p-tRight " + String(p_tRight));
      //if (first) Serial.print("rValue: "); if (first) Serial.println(rValue);
      //printOnScroll(1, INVERSE_BOLD, 0,  String(rValue) + " " + String(p_tRight));
      p_tRight = ( 7*p_tRight +  ((rValue+rUntouched) / SENS_FACTOR) ) / 8;
      //Serial.print("p_tRight: "); Serial.println(p_tRight);
  }
  //first = false;
  return ( lValue < p_tLeft ? 2 : 0 ) + (rValue < p_tRight ? 1 : 0 );
}


void initSensors() {
  int v;
  lUntouched = rUntouched = 60;       /// new: we seek minimum
  for (int i=0; i<8; ++i) {
      while ( !(v=touchRead(LEFT)) )
        ;                                       // ignore readings with value 0
        lUntouched += v;
        //lUntouched = _min(lUntouched, v);
       while ( !(v=touchRead(RIGHT)) )
        ;                                       // ignore readings with value 0
        rUntouched += v;
        //rUntouched = _min(rUntouched, v);
  }
  lUntouched /= 8;
  rUntouched /= 8;
  p_tLeft = lUntouched - 9;
  p_tRight = rUntouched - 9;
}


String getRandomWord( int maxLength) {        //// give me a random English word, max maxLength chars long (1-5) - 0 returns any length
  if (maxLength > 5)
    maxLength = 0;
    if (kochActive)
        return kochWords[random(numberOfWords)]; 
    else 
        return words[random(WORDS_POINTER[maxLength], WORDS_NUMBER_OF_ELEMENTS)];
}

String getRandomAbbrev( int maxLength) {        //// give me a random CW abbreviation , max maxLength chars long (1-5) - 0 returns any length
  if (maxLength > 5)
    maxLength = 0;
    if (kochActive)
        return kochAbbr[random(numberOfAbbr)];
    else
        return abbreviations[random(ABBREV_POINTER[maxLength], ABBREV_NUMBER_OF_ELEMENTS)];  
}

// we use substrings as char pool for trainer mode
  // SANK will be replaced by <as>, <ka>, <kn> and <sk>
  // Options:
  //    0: a9?<> = CWchars; all of them; same as Koch 45+
  //    1: a = CWchars.substring(0,26);
  //    2: 9 = CWchars.substring(26,36);
  //    3: ? = CWchars.substring(36,45);
  //    4: <> = CWchars.substring(44,50);
  //    5: a9 = CWchars.substring(0,36);
  //    6: 9? = CWchars.substring(26,45);
  //    7: ?<> = CWchars.substring(36,50);
  //    8: a9? = CWchars.substring(0,45); 
  //    9: 9?<> = CWchars.substring(26,50);

  //  {OPT_ALL, OPT_ALPHA, OPT_NUM, OPT_PUNCT, OPT_PRO, OPT_ALNUM, OPT_NUMPUNCT, OPT_PUNCTPRO, OPT_ALNUMPUNCT, OPT_NUMPUNCTPRO}

String getRandomChars( int maxLength, int option) {             /// random char string, eg. group of 5, 9 differing character pools; maxLength = 1-6
  String result = ""; String pool;
  int s = 0, e = 50;
  int i;
    if (maxLength > 6) {                                        // we use a random length!
      maxLength = random(2, maxLength - 3);                     // maxLength is max 10, so random upper limit is 7, means max 6 chars...
    }
    if (kochActive) {                                           // kochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=KA+SNE@:"
        int endk =  p_kochFilter;                               //              1   5    1    5    2    5    3    5    4    5    5  
        for (i = 0; i < maxLength; ++i) {
        if (random(2))                                          // in Koch mode, we generate the last third of the chars learned  a bit more often
            result += kochChars.charAt(random(2*endk/3, endk));
        else
            result += kochChars.charAt(random(endk));
        }
    } else {
         switch (option) {
          case OPT_NUM: 
          case OPT_NUMPUNCT: 
          case OPT_NUMPUNCTPRO: 
                                s = 26; break;
          case OPT_PUNCT: 
          case OPT_PUNCTPRO: 
                                s = 36; break;
          case OPT_PRO: 
                                s = 44; break;
          default:              s = 0;  break;
        }
        switch (option) {
          case OPT_ALPHA: 
                                e = 26;  break;
          case OPT_ALNUM: 
          case OPT_NUM: 
                                e = 36; break;
          case OPT_ALNUMPUNCT: 
          case OPT_NUMPUNCT:
          case OPT_PUNCT: 
                                e = 45; break;
          default:              e = 50; break;
        }

        for (i = 0; i < maxLength; ++i) 
          result += CWchars.charAt(random(s,e));
  }
  return result;
}


String getRandomCall( int maxLength) {            // random call-sign like pattern, maxLength = 3 - 6, 0 returns any length
  const byte prefixType[] = {1,0,1,2,3,1};         // 0 = a, 1 = aa, 2 = a9, 3 = 9a
  byte prefix;
  String call = "";
  unsigned int l = 0;
  //int s, e;

  if (maxLength == 1 || maxLength == 2)
      maxLength = 3;
  if (maxLength > 6)
      maxLength = 6;

  if (maxLength == 3)
      prefix = 0;
  else  
      prefix = prefixType[random(0,6)];           // what type of prefix?
  switch (prefix) {
      case 1: call += CWchars.charAt(random(0,26));
              ++l;
      case 0: call += CWchars.charAt(random(0,26));
              ++l;
              break;
      case 2: call += CWchars.charAt(random(0,26));
              call += CWchars.charAt(random(26,36));
              l = 2;
              break;
      case 3: call += CWchars.charAt(random(26,36));
              call += CWchars.charAt(random(0,26));
              l = 2;
              break;
    } // we have a prefix by now; l is its length
      // now generate a number
    call += CWchars.charAt(random(26,36));
    ++l;
    // generate a suffix, 1 2 or 3 chars long - we re-use prefix for the suffix length
    if (maxLength == 3)
        prefix = 1;
    else if (maxLength == 0) {
        prefix = random(1,4);
        prefix = (prefix == 2 ? prefix :  random(1,4)); // increase the likelihood for suffixes of length 2
    }
    else {
        //prefix = random(1,_min(maxLength-l+1, 4));     // suffix not longer than 3 chars!
        prefix = _min(maxLength - l, 3);                 // we try to always give the desired length, but never more than 3 suffix chars
    }
    while (prefix--) {
      call += CWchars.charAt(random(0,26));
      ++l;
    } // now we have the suffix as well
    // are we /p or /m? - we do this only in rare cases - 1 out of 9, and only when maxLength = 0, or maxLength-l >= 2
    if (maxLength == 0 ) //|| maxLength - l >= 2)
      if (! random(0,8)) {
      call += "/";
      call += ( random(0,2) ? "m" : "p" );
    }
    // we have a complete call sign!
    return call;
}


/////// generate CW representations from its input string 
/////// CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKVäöüH";

String generateCWword(String symbols) {
  int pointer;
  byte bitMask, NoE;
  //byte nextElement[8];      // the list of elements; 0 = dit, 1 = dah
  String result = "";
  
  int l = symbols.length();
  
  for (int i = 0; i<l; ++i) {
    char c = symbols.charAt(i);                                 // next char in string
    pointer = CWchars.indexOf(c);                               // at which position is the character in CWchars?
    NoE = pool[pointer][1];                                     // how many elements in this morse code symbol?
    bitMask = pool[pointer][0];                                 // bitMask indicates which of the elements are dots and which are dashes
    for (int j=0; j<NoE; ++j) {
        result += (bitMask & B10000000 ? "2" : "1" );         // get MSB and store it in string - 2 is dah, 1 is dit, 0 = inter-element space
        bitMask = bitMask << 1;                               // shift bitmask 1 bit to the left 
        //Serial.print("Bitmask: ");
        //Serial.println(bitmask, BIN);
      } /// now we are at the end of one character, therefore we add enough space for inter-character
      result += "0";
  }     /// now we are at the end of the word, therefore we remove the final 0!
  result.remove(result.length()-1);
  return result;
}


void generateCW () {          // this is called from loop() (frequently!)  and generates CW
  
  static int l;
  static char c;
  boolean silentEcho;
  
  switch (generatorState) {                                             // CW generator state machine - key is up or down
    case KEY_UP:
            if (millis() < genTimer)                                    // not yet at end of the pause: just wait
                return;                                                 // therefore we return to loop()
             // here we continue if the pause has been long enough
            if (startFirst == true)
                CWword = "";
            l = CWword.length();
            
            if (l==0)  {                                               // fetch a new word if we have an empty word
                if (clearText.length() > 0) {                          // this should not be reached at all.... except when display word by word
                  //Serial.println("Text left: " + clearText);
                  if (morseState == loraTrx || (morseState == morseGenerator && p_trainerDisplay == DISPLAY_BY_WORD) ||
                        ( morseState == echoTrainer && p_echoDisplay != CODE_ONLY)) {
                      printToScroll(BOLD,cleanUpProSigns(clearText));
                      clearText = "";
                  }
                }
                fetchNewWord();
                //Serial.println("New Word: " + CWword);
                if (CWword.length() == 0)                             // we really should have something here - unless in trx mode; in this case return
                  return;
                if ((morseState == echoTrainer)) {
                  printToScroll(REGULAR, "\n");
                }
            }
            c = CWword[0];                                            // retrieve next element from CWword; if 0, we were at end of character
            CWword.remove(0,1); 
            if (c == '0' || !CWword.length())  {                      // a character just had been finished //// is there an error here?
                   if (c == '0') {
                      c = CWword[0];                                  // retrieve next element from CWword;
                      CWword.remove(0,1); 
                      if (morseState == morseGenerator && p_loraTrainerMode == 1)
                          cwForLora(0);                             // send end of character to lora
                      }
            }   /// at end of character

            //// insert code here for outputting only on display, and not as morse characters - for echo trainer
            //// genTimer vy short (1ms?)
            //// no keyOut()
            if (morseState == echoTrainer && p_echoDisplay == DISP_ONLY)
                genTimer = millis() + 2;      // very short timing
            else if (morseState != loraTrx)
                genTimer = millis() + (c == '1' ? ditLength : dahLength);           // start a dit or a dah, acording to the next element
            else 
                genTimer = millis() + (c == '1' ? rxDitLength : rxDahLength);
            if (morseState == morseGenerator && p_loraTrainerMode == 1)             // send the element to LoRa
                c == '1' ? cwForLora(1) : cwForLora(2) ; 
            /// if Koch learn character we show dit or dah
            if (generatorMode == KOCH_LEARN)
                printToScroll(REGULAR, c == '1' ? "."  : "-");

            silentEcho = (morseState == echoTrainer && p_echoDisplay == DISP_ONLY); // echo mode with no audible prompt

            if (silentEcho || stopFlag)                                             // we finished maxSequence and so do start output (otherwise we get a short click)
              ;
            else  {
                keyOut(true, (morseState != loraTrx), notes[p_sidetoneFreq], p_sidetoneVolume);
            }
            /* // replaced by the lines above, to also take care of maxSequence
            if ( ! (morseState == echoTrainer && p_echoDisplay == DISP_ONLY)) 
                   
                        keyOut(true, (morseState != loraTrx), notes[p_sidetoneFreq], p_sidetoneVolume);
            */
            generatorState = KEY_DOWN;                              // next state = key down = dit or dah

            break;
    case KEY_DOWN:
            if (millis() < genTimer)                                // if not at end of key down we need to wait, so we just return to loop()
                return;
            //// otherwise we continue here; stop keying,  and determine the length of the following pause: inter Element, interCharacter or InterWord?

           keyOut(false, (morseState != loraTrx), 0, 0);
            if (! CWword.length())   {                                 // we just ended the the word, ...  //// intercept here in Echo Trainer mode
 //             // display last character - consider echo mode!
                if (morseState == morseGenerator) 
                    autoStop = p_autoStop ? stop1 : off;
                dispGeneratedChar();
                if (morseState == echoTrainer) {
                    switch (echoTrainerState) {
                        case START_ECHO:  echoTrainerState = SEND_WORD;
                                          genTimer = millis() + interCharacterSpace + (p_promptPause * interWordSpace);
                                          break;
                        case REPEAT_WORD:
                                          // fall through 
                        case SEND_WORD:   if (echoStop)
                                                break;
                                          else {
                                                echoTrainerState = GET_ANSWER;
                                                if (p_echoDisplay != CODE_ONLY) {
                                                    printToScroll(REGULAR, " ");
                                                    printToScroll(INVERSE_REGULAR, ">");    /// add a blank after the word on the display
                                                }
                                                ++repeats;
                                                genTimer = millis() + p_responsePause * interWordSpace;
                                          }
                        default:          break;
                    }
                }
                else { 
                      genTimer = millis() + (morseState == loraTrx ? rxInterWordSpace : interWordSpace) ;              // we need a pause for interWordSpace
                      if (morseState == morseGenerator && p_loraTrainerMode == 1) {                                   // in generator mode and we want to send with LoRa
                          cwForLora(0);
                          cwForLora(3);                           // as we have just finished a word
                          //Serial.println("cwForLora(3);");
                          sendWithLora();                         // finalise the string and send it to LoRA
                          delay(interCharacterSpace+ditLength);             // we need a slightly longer pause otherwise the receiving end might fall too far behind...
                          } 
                }
             }
             else if ((c = CWword[0]) == '0') {                                                                        // we are at end of character
//              // display last character 
//              // genTimer small if in echo mode and no code!
                dispGeneratedChar(); 
                if (morseState == echoTrainer && p_echoDisplay == DISP_ONLY)
                    genTimer = millis() +1;
                else            
                    genTimer = millis() + (morseState == loraTrx ? rxInterCharacterSpace : interCharacterSpace);          // pause = intercharacter space
             }
             else  {                                                                                                   // we are in the middle of a character
                genTimer = millis() + (morseState == loraTrx ? rxDitLength : ditLength);                              // pause = interelement space
             }
             generatorState = KEY_UP;                               // next state = key up = pause
             break;         
  }   /// end switch (generatorState)
}


/// when generating CW, we display the character (under certain circumstances)
/// add code to display in echo mode when parameter is so set
/// p_echoDisplay 1 = CODE_ONLY 2 = DISP_ONLY 3 = CODE_AND_DISP

void dispGeneratedChar() {
  static String charString;
  charString.reserve(10);
  
  if (generatorMode == KOCH_LEARN || morseState == loraTrx || (morseState == morseGenerator && p_trainerDisplay == DISPLAY_BY_CHAR) ||
                    ( morseState == echoTrainer && p_echoDisplay != CODE_ONLY ))
                    //&& echoTrainerState != SEND_WORD
                    //&& echoTrainerState != REPEAT_WORD)) 
    
      {       /// we need to output the character on the display now  
        charString = clearText.charAt(0);                   /// store first char of clearText in charString
        clearText.remove(0,1);                              /// and remove it from clearText
        if (generatorMode == KOCH_LEARN)
            printToScroll(REGULAR,"");                      // clear the buffer first
        printToScroll(morseState == loraTrx ? BOLD : REGULAR, cleanUpProSigns(charString));
        if (generatorMode == KOCH_LEARN)
            printToScroll(REGULAR," ");                      // output a space
      }   //// end display_by_char
      
      ++charCounter;                         // count this character
     
     // if we have seen 12 chars since changing speed, we write the config to Preferences
     if (charCounter == 12) {
        pref.begin("morserino", false);             // open the namespace as read/write
        pref.putUChar("wpm", p_wpm);
        pref.end();
     }
}

void fetchNewWord() {
  int rssi, rxWpm, rv;

//Serial.println("startFirst: " + String((startFirst ? "true" : "false")));
//Serial.println("firstTime: " + String((firstTime ? "true" : "false")));
    if (morseState == loraTrx) {                                // we check the rxBuffer and see if we received something
       updateSMeter(0);                                         // at end of word we set S-meter to 0 until we receive something again
       //Serial.print("end of word - S0? ");
       startFirst = false;
       ////// from here: retrieve next CWword from buffer!
        if (loRaBuReady()) {
            printToScroll(BOLD, " ");
            uint8_t header = decodePacket(&rssi, &rxWpm, &CWword);
            //Serial.println("Header: " + (String) header);
            //Serial.println("CWword: " + (String) CWword);
            //Serial.println("Speed: " + (String) rxWpm);
            
            if ((header >> 6) != 1)                             // invalid protocol version
              return;
            if ((rxWpm < 5) || (rxWpm >60))                    // invalid speed
              return;
            clearText = CWwordToClearText(CWword);
            //Serial.println("clearText: " + (String) clearText);
            //Serial.println("RX Speed: " + (String)rxWpm);
            //Serial.println("RSSI: " + (String)rssi);
            
            rxDitLength = 1200 /   rxWpm ;                      // set new value for length of dits and dahs and other timings
            rxDahLength = 3* rxDitLength ;                      // calculate the other timing values
            rxInterCharacterSpace = 3 * rxDitLength;
            rxInterWordSpace = 7 * rxDitLength;
            sprintf(numBuffer, "%2ir", rxWpm);
            printOnStatusLine(true, 4,  numBuffer); 
            printOnStatusLine(true, 9, "s");
            updateSMeter(rssi);                                 // indicate signal strength of new packet
       }
       else return;                                             // we did not receive anything
               
    } // end if loraTrx
    else {

    //if (morseState != echoTrainer)
    if ((morseState == morseGenerator) && !p_autoStop) {
        printToScroll(REGULAR, " ");    /// in any case, add a blank after the word on the display
    }
    
    if (generatorMode == KOCH_LEARN) {
        startFirst = false;
        echoTrainerState = SEND_WORD;
    }
    if (startFirst == true)  {                                 /// do the intial sequence in trainer mode, too
        clearText = "vvvA";
        startFirst = false;
    } else if (morseState == morseGenerator && p_wordDoubler == true && firstTime == false) {
        clearText = echoTrainerWord;
        firstTime = true;
    } else if (morseState == echoTrainer) {
        interWordTimer = 4294967000;                   /// interword timer should not trigger something now
        //Serial.println("echoTrainerState: " + String(echoTrainerState));
        switch (echoTrainerState) {
            case  REPEAT_WORD:  if (p_echoRepeats == 7 || repeats <= p_echoRepeats) 
                                    clearText = echoTrainerWord;
                                else {
                                    clearText = echoTrainerWord;
                                    if (generatorMode != KOCH_LEARN) {
                                        printToScroll(INVERSE_REGULAR, cleanUpProSigns(clearText));    //// clean up first!
                                        printToScroll(REGULAR, " ");
                                    }
                                    goto randomGenerate;
                                }
                                break;
            //case  START_ECHO:
            case  SEND_WORD:    goto randomGenerate;
            default:            break;
        }   /// end special cases for echo Trainer
    } else {   
   
      randomGenerate:       repeats = 0;
                            if (((morseState == morseGenerator) || (morseState == echoTrainer)) && (p_maxSequence != 0) &&
                                    (generatorMode != KOCH_LEARN))  {                                           // a case for maxSequence
                                ++ wordCounter;
                                int limit = 1 + p_maxSequence;
                                if (wordCounter == limit) {
                                  clearText = "+";
                                    echoStop = true;
                                }
                                else if (wordCounter == (limit+1)) {
                                    stopFlag = true;
                                    echoStop = false;
                                    wordCounter = 1;
                                }
                            }
                            if (clearText != "+") {
                                switch (generatorMode) {
                                      case  RANDOMS:  clearText = getRandomChars(p_randomLength, p_randomOption);
                                                      break;
                                      case  CALLS:    clearText = getRandomCall(p_callLength);
                                                      break;
                                      case  ABBREVS:  clearText = getRandomAbbrev(p_abbrevLength);
                                                      break;
                                      case  WORDS:    clearText = getRandomWord(p_wordLength);
                                                      break;
                                      case  KOCH_LEARN:clearText = (String) kochChars.charAt(p_kochFilter - 1);
                                                      break;
                                      case  MIXED:    rv = random(4);
                                                      switch (rv) {
                                                        case  0:  clearText = getRandomWord(p_wordLength);
                                                                  break;
                                                        case  1:  clearText = getRandomAbbrev(p_abbrevLength);
                                                                    break;
                                                        case  2:  clearText = getRandomCall(p_callLength);
                                                                  break;
                                                        case  3:  clearText = getRandomChars(1,OPT_PUNCTPRO);        // just a single pro-sign or interpunct
                                                                  break;
                                                      }
                                                      break;
                                      case KOCH_MIXED:rv = random(3);
                                                      switch (rv) {
                                                        case  0:  clearText = getRandomWord(p_wordLength);
                                                                  break;
                                                        case  1:  clearText = getRandomAbbrev(p_abbrevLength);
                                                                    break;
                                                        case  2:  clearText = getRandomChars(p_randomLength, OPT_KOCH);        // Koch option!
                                                                  break;
                                                      }
                                                      break;
                                      case PLAYER:    if (p_randomFile)
                                                          skipWords(random(p_randomFile+1));
                                                      clearText = getWord();
                                                      /*
                                                      if (clearText == String()) {        /// at end of file: go to beginning again
                                                        p_fileWordPointer = 0;
                                                        file.close(); file = SPIFFS.open("/player.txt");
                                                      }
                                                      ++p_fileWordPointer;
                                                      */
                                                      clearText = cleanUpText(clearText);
                                                      break;  
                                    }   // end switch (generatorMode)
                            }
                            firstTime = false;
      }       /// end if else - we either already had something in trainer mode, or we got a new word

      CWword = generateCWword(clearText);
      echoTrainerWord = clearText;
    } /// else (= not in loraTrx mode)
} // end of fetchNewWord()



////// S Meter for Trx modus

void updateSMeter(int rssi) {

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
  display.display();
}

////// setup preferences ///////

             
boolean setupPreferences(uint8_t atMenu) {
  // enum morserinoMode {morseKeyer, loraTrx, morseGenerator, echoTrainer, shutDown, morseDecoder, invalid };
  static int oldPos = 1;
  int t;

  int ptrIndex, ptrMax;
  prefPos posPtr;
 
  ptrMax = currentOptionSize;

  ///// we should check here if the old ptr (oldIndex) is contained in the current preferences collection (currentOptions)
  ptrIndex = 1;
  for (int i = 0; i < ptrMax; ++i) {
      if (currentOptions[i] == oldPos) {
          ptrIndex = i;
          break;
      }
  }
  posPtr = currentOptions[ptrIndex];  
  keyOut(false, true, 0, 0);                // turn the LED off, unkey transmitter, or whatever; just in case....
  keyOut(false,false, 0, 0);  
  displayKeyerPreferencesMenu(posPtr);
  printOnScroll(2, REGULAR, 0,  " ");

  while (true) {                            // we wait for single click = selection or long click = exit - or single or long click or RED button
        modeButton.Update();
        switch (modeButton.clicks) {            // button was clicked
          case 1:     // change the option corresponding to pos
                      if (adjustKeyerPreference(posPtr))
                         goto exitFromHere;
                      break;
          case -1:    //////// long press indicates we are done with setting preferences - check if we need to store some of the preferences
          exitFromHere: writePreferences("morserino");
                        //delay(200);
                        return false;
                        break;
          }

          volButton.Update();                 // RED button
          switch (volButton.clicks) {         // was clicked
            case 1:     // recall snapshot
                        if (recallSnapshot())
                          writePreferences("morserino");
                        //delay(100);
                        return true;
                        break;
            case -1:    //store snapshot
                        
                        if (storeSnapshot(atMenu))
                          writePreferences("morserino");
                        while(volButton.clicks)
                          volButton.Update();
                        return false;
                        break;
          }

          
          //// display the value of the preference in question

         if ((t=checkEncoder())) {
            pwmClick(p_sidetoneVolume);         /// click 
            ptrIndex = (ptrIndex +ptrMax + t) % ptrMax;
            //Serial.println("ptrIndex: " + String(ptrIndex));
            posPtr = currentOptions[ptrIndex];
            //oldIndex = ptrIndex;                                                              // remember menu position
            oldPos = posPtr;
            
            displayKeyerPreferencesMenu(posPtr);
            //printOnScroll(1, BOLD, 0, ">");
            printOnScroll(2, REGULAR, 0, " ");

            display.display();                                                        // update the display   
         }    // end if (encoderPos)
         checkShutDown(false);         // check for time out
  } // end while - we leave as soon as the button has been pressed long
}   // end function setupKeyerPreferences()


//////// Display the preferences menu - we display the following preferences

void displayKeyerPreferencesMenu(int pos) {
  display.clear();
  if (pos < posLoraBand)
    printOnStatusLine(true, 0,  "Set Preferences: ");
  else if (pos < posSnapRecall)
    printOnStatusLine(true, 0,  "Config LoRa:     ");
  else 
    printOnStatusLine(true, 0,  "Manage Snapshots:");
  printOnScroll(1, BOLD, 0, prefOption[pos]);
  
  switch (pos) {
     case  posCurtisMode:  displayCurtisMode();
                          break;
     case  posCurtisBDahTiming:  displayCurtisBTiming();
                          break;
     case  posCurtisBDotTiming:  displayCurtisBDotTiming();
                          break;
          case  posACS:  displayACS();
                          break;
    case  posPolarity:  displayPolarity();
                          break;
    case posLatency:    displayLatency();
                          break;
    case  posExtPaddles:  displayExtPaddles();
                          break;
    case  posPitch:  displayPitch();
                          break;
    case  posClicks:  displayClicks();
                          break;
    case posKeyTrainerMode:  displayKeyTrainerMode();
                          break;
    case  posInterWordSpace:  displayInterWordSpace(); 
                          break; 
    case  posInterCharSpace:  displayInterCharSpace();
                          break;
    case  posKochFilter:      displayKochFilter();
                          break;
    case  posRandomOption:  displayRandomOption();
                          break;
    case posRandomLength:  displayRandomLength();
                          break;
    case posCallLength:  displayCallLength();
                          break;
    case posAbbrevLength:  displayAbbrevLength();
                          break;
    case posWordLength:  displayWordLength();
                          break;
    case posTrainerDisplay:  displayTrainerDisplay();
                          break;
    case posEchoDisplay:  displayEchoDisplay();
                          break;
    case posEchoRepeats:  displayEchoRepeats();
                          break;
    case posEchoConf:     displayEchoConf();
                          break;
    case posWordDoubler:  displayWordDoubler();
                          break;
    case posEchoToneShift:displayEchoToneShift();
                          break;
    case posLoraTrainerMode:  displayLoraTrainerMode();
                          break;
    case posLoraSyncW:    displayLoraSyncW();
                          break;
    case posGoertzelBandwidth: displayGoertzelBandwidth();
                          break;
    case posSpeedAdapt:   displaySpeedAdapt();
                          break;
    case posRandomFile:   displayRandomFile();
                          break;
    case posKochSeq:      displayKochSeq();
                          break;
    case posTimeOut:      displayTimeOut();
                          break;
    case posQuickStart:   displayQuickStart();
                          break;
    case posAutoStop:     displayAutoStop();
                          break;
     case  posLoraBand:  displayLoraBand();
                          break;
     case  posLoraQRG:  displayLoraQRG();
                          break; 
     case posSnapRecall: displaySnapRecall();
                          break;
     case posSnapStore: displaySnapStore();
                          break;  
     case posMaxSequence: displayMaxSequence();
                          break;                     
  } /// switch (pos)
  display.display();
} // displayKeyerPreferences()


/// now follow all the menu displays

void displayCurtisMode() {
  String keyerModus[] = {"Curtis A    ", 
                         "Curtis B    ", 
                         "Ultimatic   ",
                         "Non-Squeeze "};
  printOnScroll(2, REGULAR, 1, keyerModus[p_keyermode-1]);
}   

void displayCurtisBTiming() {
  // display start timing when paddles are being checked in Curtis B mode during dah: between 0 and 100
  sprintf(numBuffer, "%3i", p_curtisBTiming);
  printOnScroll(2, REGULAR, 1, numBuffer);
}

void displayCurtisBDotTiming() {
  // display start timing when paddles are being checked in Curtis B modeduring dit : between 0 and 100
  sprintf(numBuffer, "%3i", p_curtisBDotTiming);
  printOnScroll(2, REGULAR, 1, numBuffer);
}

void displayACS() {
  String ACSmode[] = {"Off         ",
                      "Invalid     ",
                      "min. 2 dots ",
                      "min. 3 dots ",
                      "min. 4 dots "};
    printOnScroll(2, REGULAR, 1, ACSmode[p_ACSlength]);                  
}

void displayPitch() {
  sprintf(numBuffer, "%3i", notes[p_sidetoneFreq]);
  printOnScroll(2, REGULAR, 1, numBuffer);
}

void displayClicks() {
     printOnScroll(2, REGULAR, 1,  p_encoderClicks ? "On " :
                                                          "Off" ); 
}

void displayExtPaddles() {
     printOnScroll(2, REGULAR, 1,  p_useExtPaddle ? "Reversed    " :
                                                    "Normal      " ); 
}

void displayPolarity() {
      printOnScroll(2, REGULAR, 1, p_didah ? ".- di-dah  " :
                                                   "-. dah-dit " ); 
}

void displayLatency() {
      sprintf(numBuffer, "%1i/8 of dit", p_latency - 1);
      printOnScroll(2, REGULAR, 1, numBuffer);
}
void displayInterWordSpace() {
  // display interword space in ditlengths 
  sprintf(numBuffer, "%2i", p_interWordSpace);
  printOnScroll(2, REGULAR, 1, numBuffer);
}

void displayInterCharSpace() {
  // display intercharacter space in ditlengths
  sprintf(numBuffer, "%2i", p_interCharSpace);
  printOnScroll(2, REGULAR, 1, numBuffer);
}


void displayRandomOption() {
  String texts[] = {"All Chars   ",
                    "Alpha       ", 
                    "Numerals    ", 
                    "Interpunct. ", 
                    "Pro Signs   ",
                    "Alpha + Num ",
                    "Num+Interp. ",
                    "Interp+ProSn",
                    "Alph+Num+Int",
                    "Num+Int+ProS"
                     };
  printOnScroll(2, REGULAR, 1, texts[p_randomOption]);
}

void displayRandomLength() {
  // display length of random character groups - 2 - 6
  if (p_randomLength <= 6) 
    sprintf(numBuffer, "%1i     ", p_randomLength);  
  else 
    sprintf(numBuffer, "2 to %1i", p_randomLength -4);
  printOnScroll(2, REGULAR, 1, numBuffer);
}


void displayCallLength() {
  // display length of calls - 3 - 6, 0 = all
  if (p_callLength == 0)
       printOnScroll(2, REGULAR, 1, "Unlimited");
  else {
    sprintf(numBuffer, "max. %1i   ", p_callLength);
    printOnScroll(2, REGULAR, 1, numBuffer);
  }
}


void displayAbbrevLength() {
  // display length of abbrev - 2 - 6, 0 = all
  if (p_abbrevLength == 0)
       printOnScroll(2, REGULAR, 1, "Unlimited");
  else {
    sprintf(numBuffer, "max. %1i    ", p_abbrevLength);
    printOnScroll(2, REGULAR, 1, numBuffer);
  }
}


void displayWordLength() {
  // display length of english words - 2 - 6, 0 = all
  if (p_wordLength == 0)
       printOnScroll(2, REGULAR, 1, "Unlimited");
  else {
    sprintf(numBuffer, "max. %1i     ", p_wordLength);
    printOnScroll(2, REGULAR, 1, numBuffer);
  }
}


void displayMaxSequence() {
  // display max # of words; 0 = no limit, 5, 10, 15, 20... 250; 255 = no limit
  if ((p_maxSequence == 0) || (p_maxSequence == 255))
      printOnScroll(2, REGULAR, 1, "Unlimited");
  else {
      sprintf(numBuffer, "%3i      ", p_maxSequence);
      printOnScroll(2, REGULAR, 1, numBuffer);
  }
}


void displayTrainerDisplay() {
  switch (p_trainerDisplay) {
    case  NO_DISPLAY:       printOnScroll(2, REGULAR, 1, "Display off ");
                            break;
    case  DISPLAY_BY_CHAR:  printOnScroll(2, REGULAR, 1, "Char by char");
                            break;
    case  DISPLAY_BY_WORD:  printOnScroll(2, REGULAR, 1, "Word by word");
                            break;
  }
}

void displayEchoDisplay() {
  switch (p_echoDisplay) {
    case CODE_ONLY:         printOnScroll(2, REGULAR, 1, "Sound only  ");
                            break;
    case DISP_ONLY:         printOnScroll(2, REGULAR, 1, "Display only");
                            break;
    case CODE_AND_DISP:     printOnScroll(2, REGULAR, 1, "Sound & Disp");
                            break;
    
  }
}
void displayKeyTrainerMode() {
  String option;
  switch(p_keyTrainerMode) {
    case 0: option = "Never        ";
            break;
    case 1: option = "CW Keyer only";
            break;
    case 2: option = "Keyer&Genertr";
            break;
  }
  printOnScroll(2, REGULAR, 1, option);
}

void displayLoraTrainerMode() {
    String option;
  switch(p_loraTrainerMode) {
    case 0: option = "LoRa Tx OFF  ";
            break;
    case 1: option = "LoRa Tx ON   ";
            break;
  }
  printOnScroll(2, REGULAR, 1, option);
}

void displayLoraSyncW() {
    String option;
  switch(p_loraSyncW) {
    case 0x27: option = "Standard Ch  ";
            break;
    case 0x66: option = "Secondary Ch ";
            break;
  }
  printOnScroll(2, REGULAR, 1, option);
}

            
void displayEchoRepeats() {
  if (p_echoRepeats < 7) {
      sprintf(numBuffer, "%i      ", p_echoRepeats);
      printOnScroll(2, REGULAR, 1, numBuffer);
  } else
  printOnScroll(2, REGULAR, 1, "Forever");
}

void displayEchoToneShift() {
  String option;
  switch (p_echoToneShift) {
    case 0: option = "No Tone Shift";
            break;
    case 1: option = "Up 1/2 Tone  ";
            break;
    case 2: option = "Down 1/2 Tone";
  }
  printOnScroll(2, REGULAR, 1, option);
}

void displayEchoConf() {
   printOnScroll(2, REGULAR, 1,  p_echoConf ? "On " :
                                              "Off" ); 
}

void displayKochFilter() {                          // const String kochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=KA+SNE@:";
    String str;
    str.reserve(6);
    str = (String) kochChars.charAt(p_kochFilter - 1);
    cleanUpProSigns(str);
    sprintf(numBuffer, "%2i %s   ", p_kochFilter, str.c_str());
    printOnScroll(2, REGULAR, 1, numBuffer);
}


void displayWordDoubler() {
  printOnScroll(2, REGULAR, 1,  p_wordDoubler ? "On  " :
                                                "Off " ); 
}

void displayRandomFile() {
    printOnScroll(2, REGULAR, 1,  p_randomFile ? "On  " :
                                                "Off " ); 
}

void displayGoertzelBandwidth()  {
    String option;
  switch(p_goertzelBandwidth) {
    case 0: option = "Wide         ";
            break;
    case 1: option = "Narrow       ";
            break;
  }
  printOnScroll(2, REGULAR, 1, option);
}

void displaySpeedAdapt() {
      printOnScroll(2, REGULAR, 1, p_speedAdapt ? "ON         " :
                                                  "OFF        " ); 
}

void displayKochSeq() {
      printOnScroll(2, REGULAR, 1, p_lcwoKochSeq ? "LCWO      " :
                                                   "M32 / JLMC");
}

void displayTimeOut() {
    String TOValue;
    
    switch (p_timeOut) {
          case 1: TOValue     = " 5 min    ";
                  break;
          case 2: TOValue     = "10 min    ";
                  break;
          case 3: TOValue     = "15 min    ";
                  break;
          case 4: TOValue     = "No timeout";
                  break;
      }
      printOnScroll(2, REGULAR, 1, TOValue);
}

void displayQuickStart() {
      printOnScroll(2, REGULAR, 1, p_quickStart ? "ON         " :
                                                  "OFF        " ); 
}

void displayAutoStop() {
      printOnScroll(2, REGULAR, 1, p_autoStop ? "ON         " :
                                                  "OFF        " ); 
}

void displayLoraBand() {
  String bandName;
  switch (p_loraBand) {
      case 0: bandName = "433 MHz ";
              break;
      case 1: bandName = "868 MHz ";
              break;
      case 2: bandName = "920 MHz ";
              break;
  }
  printOnScroll(2, REGULAR, 1, bandName);
}

void displayLoraQRG() {
  const int a = (int) QRG433;
  const int b = (int) QRG866;
  const int c = (int) QRG920;
   sprintf(numBuffer, "%6d kHz", p_loraQRG / 1000);
   printOnScroll(2, REGULAR, 1, numBuffer);
   
   switch (p_loraQRG) {
      case a:
      case b:
      case c  : printOnScroll(2, BOLD, 11,    "DEF");
                break;
      default:  printOnScroll(2, REGULAR, 11, "   ");
                break;
   }
}


void displaySnapRecall() {
  if (memCounter) {
    if (memPtr == memCounter)
      printOnScroll(2, REGULAR, 1, "Cancel Recall");
    else {
      sprintf(numBuffer, "Snapshot %d   ", memories[memPtr] +1);
      printOnScroll(2, REGULAR, 1, numBuffer);
    }
  }
  else
    printOnScroll(2, REGULAR, 1, "NO SNAPSHOTS"); 
}

void displaySnapStore() {
  uint8_t mask = 1;
  mask = mask << memPtr;
  if (memPtr == 8)
    printOnScroll(2, REGULAR, 1, "Cancel Store");
  else {
    sprintf(numBuffer, "Snapshot %d  ", memPtr+1);
    printOnScroll(2, p_snapShots & mask ? BOLD : REGULAR, 1, numBuffer);
  }
}



//// function to addjust the selected preference

boolean adjustKeyerPreference(prefPos pos) {        /// rotating the encoder changes the value, click returns to preferences menu
     //printOnScroll(1, REGULAR, 0, " ");       /// returns true when a long button press ended it, and false when there was a short click
     printOnScroll(2, INVERSE_BOLD, 0, ">");
    
    int t;
    while (true) {                            // we wait for single click = selection or long click = exit
        modeButton.Update();
        switch (modeButton.clicks) {
          case -1 : //delay(200);
                    return true;
                    break;
          case  1 : //printOnScroll(1, BOLD, 0,  ">");
                    printOnScroll(2, REGULAR, 0,  " ");
                    return false;
        }
        if (pos == posSnapRecall) {         // here we can delete a memory....
          volButton.Update();
          if (volButton.clicks) {
            if (memCounter)
              clearMemory(memPtr);
            return true;
          }
        }
        if ((t=checkEncoder())) {
            pwmClick(p_sidetoneVolume);         /// click 
            switch (pos) {
                case  posCurtisMode : p_keyermode = (p_keyermode + t);                        // set the curtis mode
                                      p_keyermode = constrain(p_keyermode, 1, 4);
                                      displayCurtisMode();                                    // display curtis mode
                                      break;
                case  posCurtisBDahTiming : p_curtisBTiming += (t * 5);                          // Curtis B timing dah (enhanced Curtis mode)
                                      p_curtisBTiming = constrain(p_curtisBTiming, 0, 100);
                                      displayCurtisBTiming();
                                      break;
                case  posCurtisBDotTiming : p_curtisBDotTiming += (t * 5);                   // Curtis B timing dit (enhanced Curtis mode)
                                      p_curtisBDotTiming = constrain(p_curtisBDotTiming, 0, 100);
                                      displayCurtisBDotTiming();
                                      break;
                case  posACS : p_ACSlength += (t+1);                       // ACS
                                if (p_ACSlength == 2)
                                  p_ACSlength +=  t;
                                p_ACSlength = constrain(p_ACSlength-1, 0, 4);
                                displayACS();
                                break;
                case  posPitch : p_sidetoneFreq += t;                             // sidetone pitch
                                p_sidetoneFreq = constrain(p_sidetoneFreq, 1, 15)  ;
                                displayPitch();
                                break;
                case  posClicks : p_encoderClicks = !p_encoderClicks;
                                displayClicks();
                                break;
                case  posExtPaddles : p_useExtPaddle = !p_useExtPaddle;                           // ext paddle on/off
                                displayExtPaddles();
                                break;
                case  posPolarity: p_didah = !p_didah;                                            // polarity
                                displayPolarity();
                                break;
                case posLatency:  p_latency += t;
                                  p_latency = constrain(p_latency, 1, 8);
                                  displayLatency();
                                  break;
                case  posKeyTrainerMode:  p_keyTrainerMode += (t+1);                     // Key TRX: 0=never, 1= keyer only, 2 = keyer & trainer
                                p_keyTrainerMode = constrain(p_keyTrainerMode-1, 0, 2);
                                displayKeyTrainerMode();
                                break; 
                case  posInterWordSpace : p_interWordSpace += t;                         // interword space in lengths of dit
                                p_interWordSpace = constrain(p_interWordSpace, 6, 45);            // has to be between 6 and 45 dits
                                displayInterWordSpace();
                                updateTimings();
                                break;
                case  posInterCharSpace : p_interCharSpace = constrain(p_interCharSpace + t, 3, 24);  // set Interchar space - 3 - 24 dits
                                displayInterCharSpace();                                           
                                updateTimings();
                                break;                                                               
                case  posKochFilter: p_kochFilter = constrain(p_kochFilter + t, 1, kochChars.length());
                                displayKochFilter();
                                break;
                //case  posGenerate : p_generatorMode = (p_generatorMode + t + 6) % 6;     // what trainer generates (0 - 5)
                 //               displayGenerate();
                 //               break;
                case  posRandomOption : p_randomOption = (p_randomOption + t + 10) % 10;     // which char set for random chars?
                                displayRandomOption();
                                break;
                case  posRandomLength: p_randomLength += t;                                 // length of random char group: 2-6
                                p_randomLength = constrain(p_randomLength, 1, 10);                   // 7-10 for rnd length 2 to 3-6
                                displayRandomLength();
                                break;
                case  posCallLength: if (p_callLength)                                             // length of calls: 0, or 3-6
                                        p_callLength -= 2;                                        // temorarily make it 0-4
                                p_callLength = constrain(p_callLength + t, 0, 4);
                                if (p_callLength)                                             // length of calls: 0, or 3-6
                                    p_callLength += 2;                                        // expand again if not 0
                                                           
                                displayCallLength();
                                break;
                case  posAbbrevLength: p_abbrevLength += (t+1);                                 // length of abbreviations: 0, or 2-6
                                if (p_abbrevLength == 2)                                      // get rid of 1
                                  p_abbrevLength +=  t;
                                p_abbrevLength = constrain(p_abbrevLength-1, 0, 6);
                                displayAbbrevLength();
                                break;
                case  posWordLength: p_wordLength += (t+1);                                   // length of English words: 0, or 2-6
                                if (p_wordLength == 2)                                        // get rid of 1
                                  p_wordLength +=  t;
                                p_wordLength = constrain(p_wordLength-1, 0, 6);
                                displayWordLength();
                                break;
                case  posMaxSequence: 
                                switch(p_maxSequence) {
                                  case 0:
                                      if (t == -1) 
                                        p_maxSequence = 250;
                                      else
                                        p_maxSequence = 5;
                                      break;
                                   case 250:
                                      if (t == -1) 
                                        p_maxSequence = 245;
                                      else
                                        p_maxSequence = 0;
                                      break;
                                   default:
                                      p_maxSequence += 5*t;
                                      break;
                                }
                                displayMaxSequence();
                                break;
                case  posTrainerDisplay: p_trainerDisplay = (p_trainerDisplay + t + 3) % 3;   // display options for trainer: 0-2
                                displayTrainerDisplay();
                                break;
                case  posEchoDisplay: p_echoDisplay += t;
                                p_echoDisplay = constrain(p_echoDisplay, 1, 3);             // what prompt for echo trainer mode
                                displayEchoDisplay();
                                break;
                case  posEchoRepeats: p_echoRepeats += (t+1);                                 // no of echo repeats: 0-6, 7=forever
                                p_echoRepeats = constrain(p_echoRepeats-1, 0, 7);
                                displayEchoRepeats();
                                break;
                case  posEchoToneShift: p_echoToneShift += (t+1);                             // echo tone shift can be 0, 1 (up) or 2 (down)
                                p_echoToneShift = constrain(p_echoToneShift-1, 0, 2);
                                displayEchoToneShift();
                                break;
                case  posWordDoubler: p_wordDoubler = !p_wordDoubler;
                                displayWordDoubler();
                                break;
                case  posRandomFile: 
                                if (p_randomFile)
                                  p_randomFile = 0;
                                else
                                  p_randomFile = 255;
                                 displayRandomFile();
                                 break;
                case  posEchoConf:  p_echoConf = !p_echoConf;
                                displayEchoConf();
                                break;
                case  posLoraTrainerMode: p_loraTrainerMode += (t+2);                       // transmit lora in generator and player mode; can be 0 (no) or 1 (yes)
                                p_loraTrainerMode = (p_loraTrainerMode % 2);
                                displayLoraTrainerMode();
                                break; 
                case posLoraSyncW: p_loraSyncW = (p_loraSyncW == 0x27 ? 0x66 : 0x27);
                                displayLoraSyncW();
                                break;
                case  posGoertzelBandwidth: p_goertzelBandwidth += (t+2);                   // transmit lora in generator and player mode; can be 0 (no) or 1 (yes)
                                p_goertzelBandwidth = (p_goertzelBandwidth % 2);
                                displayGoertzelBandwidth();
                                break; 
                case  posSpeedAdapt: p_speedAdapt = !p_speedAdapt;
                                displaySpeedAdapt();
                                break; 
                case posKochSeq:  p_lcwoKochSeq = !p_lcwoKochSeq;
                                displayKochSeq();
                                break;
                case posTimeOut:  p_timeOut += (t+1);
                                p_timeOut = constrain(p_timeOut-1, 1, 4);
                                displayTimeOut();
                                break;
                case posQuickStart: p_quickStart = !p_quickStart;
                                displayQuickStart();
                                break;
                case posAutoStop: p_autoStop = !p_autoStop;
                                displayAutoStop();
                                break;
                case posLoraBand: p_loraBand += (t+1);                              // set the LoRa band
                                  p_loraBand = constrain(p_loraBand-1, 0, 2);
                                  displayLoraBand();                                // display LoRa band
                                  switch (p_loraBand) {
                                    case 0:  p_loraQRG = QRG433;
                                        break;  
                                    case 1:  p_loraQRG = QRG866;
                                        break;
                                    case 2:  p_loraQRG = QRG920;
                                        break;
                                  }
                                  break;
                case posLoraQRG:  p_loraQRG += (t*1E5);
                                  switch (p_loraBand) {
                                    case 0: p_loraQRG = constrain(p_loraQRG, 433.65E6, 434.55E6);
                                            break;
                                    case 1: p_loraQRG = constrain(p_loraQRG, 866.25E6, 869.45E6);
                                            break;
                                    case 2: p_loraQRG = constrain(p_loraQRG, 920.25E6, 923.15E6);
                                            break;
                                  }
                                  displayLoraQRG();
                                  break;
                case posSnapRecall: 
                                  if (memCounter) {
                                      memPtr = (memPtr +t + memCounter + 1) % (memCounter+1);
                                      //memPtr += (t+1);
                                      //memPtr = constrain(memPtr-1, 0, memCounter);
                                  }
                                  displaySnapRecall();
                                  break;
                case posSnapStore: 
                                  memPtr = (memPtr + t + 9) % 9;
                                  displaySnapStore();
                                  break;
            }   // end switch(pos)                  
            display.display();                                                      // update the display   

         }      // end if(encoderPos)
         checkShutDown(false);         // check for time out
    }    // end while(true) 
}   // end of function


///////// evaluate the response in Echo Trainer Mode
void echoTrainerEval() {
    delay(interCharacterSpace / 2);
    if (echoResponse == echoTrainerWord) {
      echoTrainerState = SEND_WORD;
      printToScroll(BOLD,  "OK");
      if (p_echoConf) {
          pwmTone(440,  p_sidetoneVolume, false);
          delay(97);
          pwmNoTone();
          pwmTone(587,  p_sidetoneVolume, false);
          delay(193);
          pwmNoTone();
      }
      delay(interWordSpace);
      if (p_speedAdapt)
          changeSpeed(1);
    } else {
      echoTrainerState = REPEAT_WORD;
      if (generatorMode != KOCH_LEARN || echoResponse != "") {
          printToScroll(BOLD, "ERR");
          if (p_echoConf) {
              pwmTone(311,  p_sidetoneVolume, false);
              delay(193);
              pwmNoTone();
          }
      }
      delay(interWordSpace);
      if (p_speedAdapt)
          changeSpeed(-1);
    }
    echoResponse = "";
    clearPaddleLatches();
}   // end of function


void updateTimings() {
  ditLength = 1200 / p_wpm;                    // set new value for length of dits and dahs and other timings
  dahLength = 3 * ditLength;
  interCharacterSpace =  p_interCharSpace *  ditLength;
  //interWordSpace = _max(p_interWordSpace * ditLength, (p_interCharSpace+6)*ditLength);
  interWordSpace = _max(p_interWordSpace, p_interCharSpace+4) * ditLength;

  effWpm = 60000 / (31 * ditLength + 4 * interCharacterSpace + interWordSpace );  ///  effective wpm with lengthened spaces = Farnsworth speed
} 

void changeSpeed( int t) {
  p_wpm += t;
  p_wpm = constrain(p_wpm, 5, 60);
  updateTimings();
  displayCWspeed();                     // update display of CW speed
  charCounter = 0;                                    // reset character counter
}


void keyTransmitter() {
  if (p_keyTrainerMode == 0 || morseState == echoTrainer || morseState == loraTrx )
      return;                              // we never key Tx under these conditions
  if (p_keyTrainerMode == 1 && morseState == morseGenerator)
      return;                              // we key only in keyer mode; in all other case we key TX
  if (keyTx == false)
      return;                              // do not key when input is from tone decoder
   digitalWrite(keyerPin, HIGH);           // turn the LED on, key transmitter, or whatever
}

void createKochWords(uint8_t maxl, uint8_t koch) {                  // this function creates an array of words that are compliant to Koch filter and max word length
  numberOfWords = 0;
  for (int i = WORDS_POINTER[maxl]; i< WORDS_NUMBER_OF_ELEMENTS; ++i) {     // do this for all words with max length maxl
      if (wordIsKoch(words[i]) <= koch)
          kochWords[numberOfWords++] = words[i];
  }
}

uint8_t wordIsKoch(String thisWord) {
  uint8_t thisKoch = 0;
  uint8_t l = thisWord.length();
  for ( int i = 0; i< l; ++i)
    thisKoch = _max(thisKoch, kochChars.indexOf(thisWord.charAt(i))+1);
  return thisKoch;
}


void createKochAbbr(uint8_t maxl, uint8_t koch) {                  // this function creates an array of words that are compliant to Koch filter and max word length
  numberOfAbbr = 0;
  for (int i = ABBREV_POINTER[maxl]; i< ABBREV_NUMBER_OF_ELEMENTS; ++i) {     // do this for all words with max length maxl
      if (wordIsKoch(abbreviations[i]) <= koch)
          kochAbbr[numberOfAbbr++] = abbreviations[i];
  }
}




/// display volume as a progress bar: vol = 1-100
void displayVolume () {
        drawVolumeCtrl(encoderState == speedSettingMode ? false : true, 93, 0, 28, 15, p_sidetoneVolume);
        display.display();
}


///// functions to use the graphics display more or less like a character display
///// basically we use two different fixed-width fonts, Dialoginput 12 (for the status line) and Dialoginput 15 for almost everything else
///// MAYBE we might use an even bigger font (18?) for setting parameters?

//// display functions: (neben display.clear())
//// clearStatusLine();   
//// clearScroll() (= Top, Middle & Bottom line);
//// printOnStatusLine(xpos, char*str)
//// printOnScroll(line, xpos, string); line (0,1 or 2), xpos 0 = leftmost postition
//// clearTopLine(), clearMiddleLine(), clearBottomLine()
//// printToScroll(char letter); push a character onto the scrolling display
//// starting coordinates of the 4 lines: 
//// status line: 0,0
//// top line:    0,15
//// middle line: 0,31
//// bottom line: 0.47

void clearStatusLine() {              // the status line is at the top, and inverted!
      display.setColor(WHITE);
      display.fillRect(0,0,128,15);
      display.setColor(BLACK);

      display.display();
}


String cleanUpProSigns( String &input ) {
    /// clean up clearText   -   S <as>,  - A <ka> - N <kn> - K <sk> - H ch;
    input.replace("S", "<as>");
    input.replace("A", "<ka>");
    input.replace("N", "<kn>");
    input.replace("K", "<sk>");
    input.replace("V", "<ve>");
    input.replace("H", "ch");
    input.replace("E", "<err>");
    input.replace("U", "¬");
    //Serial.println(input);
    return input;
}

void printOnStatusLine(boolean strong, uint8_t xpos, String string) {    // place a string onto the status line; chars are 7px wide = 18 chars per line
      if (strong)
        display.setFont(DialogInput_bold_12);
      else
        display.setFont(DialogInput_plain_12);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      uint8_t w = display.getStringWidth(string);
      display.setColor(WHITE);
      display.fillRect(xpos*7, 0 ,w,15);
      display.setColor(BLACK);
      display.drawString(xpos*7, 0, string);
      display.setColor(WHITE);
      display.display();
      resetTOT();
}


String printToScroll_buffer = "";
FONT_ATTRIB printToScroll_lastStyle = REGULAR;


void printToScroll(FONT_ATTRIB style, String text) {
    // Serial.printf("printToScroll(%s)\n", text.c_str());

String SP = " ";
String SL = "/";
  
  boolean styleChanged = (style != printToScroll_lastStyle);
  boolean lengthExceeded = printToScroll_buffer.length() + text.length() > 10;
  if (styleChanged || lengthExceeded) {
    //Serial.print("style/length " + styleChanged + SL + lengthExceeded + SP);
    flushScroll();
  }
  
  printToScroll_buffer += text;
  printToScroll_lastStyle = style;

  boolean linebreak = text.endsWith("\n");
  boolean printToScroll_autoflush = !p_autoStop;
  //Serial.println("AUTO: " + String(printToScroll_autoflush));
  //((Serial.println("morseState: " + String(morseState));
  if (morseState != morseGenerator)
    printToScroll_autoflush = true;
  if (printToScroll_autoflush || linebreak) {
    //Serial.print("auto/line " + printToScroll_autoflush + SP + linebreak + SP);
    flushScroll();
  }
}


void clearScrollBuffer() {
    //Serial.println("clear 1 " + printToScroll_buffer);
  printToScroll_buffer = "";
  printToScroll_lastStyle = REGULAR;
    //Serial.println("clear 2 " + printToScroll_buffer);
}

void flushScroll() {
  //Serial.println("Flushing String Buffer: " + printToScroll_buffer + "Length: " + printToScroll_buffer.length());

  uint8_t len = printToScroll_buffer.length();
  if (len != 0) {
    printToScroll_internal(printToScroll_lastStyle, printToScroll_buffer);
  /* Serial.print("flush: " + String(len));
  for (int i = 0; (i < len); i++) {
    String t = printToScroll_buffer.substring(i, i+1);
    //Serial.printf(" flushing %d<%s> ", i, t.c_str());
    printToScroll_internal(printToScroll_lastStyle, t);
  }
      */

  clearScrollBuffer();
  }
  else {
  //Serial.println("(flush)");
    }
}


void printToScroll_internal(FONT_ATTRIB style, String text) {
/// store text in textBuffer, if it fits the screen line; otherwise scroll up, clear bottom buffer, store in new buffer, print on new lien
    //Serial.println("printToScroll_internal: " + text);

static uint8_t pos = 0;
static uint8_t screenPos = 0;
//uint8_t printedChars;
//Serial.println("printToScroll_internal: " + text);
static FONT_ATTRIB lastStyle = REGULAR;
  uint8_t l = text.length();
  if (l == 0) {                               // an empty string signals we should clear the buffer
      //Serial.println("printToScroll_internal clearing buffer");
      for (int i = 0; i< NoOfLines; ++i) {
          textBuffer[i][0] = (char) 0;                    /// empty this line 
      }
      refreshScrollArea((NoOfLines + bottomLine-2) % NoOfLines);
      pos = screenPos = 0;                                // reset the position pointers
      return;
  }

  int linebreak = text.equals("\n");
  int textTooLong = (screenPos + l > NoOfCharsPerLine);
  
  if (linebreak || textTooLong) {                 // we need to scroll up and start a new line
    newLine();
    pos = 0;  screenPos = 0; lastStyle = REGULAR;
    if (linebreak) {
      text = "";
      l = text.length();
    }
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
      printOnScroll(2, style, screenPos, text);               // these characters are 9 pixels wide, 
  }
  display.setFont(DialogInput_plain_15);;
  screenPos += (display.getStringWidth(text) / 9);
}


void newLine() {
    linePointer = (linePointer + 1) % NoOfLines;
    if (relPos && relPos != maxPos)
        --relPos;
    bottomLine = linePointer;
    textBuffer[bottomLine][0] = (char) 0;               /// and empty the bottom line
    if (!relPos )                                      /// screen ptr already on top, we need to move the whole screen one line
        refreshScrollArea((bottomLine+1) % NoOfLines);
    else if (relPos == maxPos)
        refreshScrollArea((NoOfLines + bottomLine -2 ) % NoOfLines);
    if (encoderState == scrollMode)
        displayScrollBar(true);
            
}

void refreshScrollArea(int pos) {                                  /// refresh all three lines from buffer in scroll area; pos is topmost buffer line
  refreshScrollLine(pos, 0);           /// refresh all three lines
  refreshScrollLine((pos+1) % NoOfLines, 1);
  //unsigned int t = pos % 15;
  refreshScrollLine((pos+2) % NoOfLines, 2);
  display.display();
}


void refreshScrollLine(int bufferLine, int displayLine) {   /// print a line to the screen
  String temp;
  temp.reserve(16);
  temp = "";
  char c;
  boolean irFlag = false;
  FONT_ATTRIB style = REGULAR;
  int pos = 0;
  uint8_t charsPrinted;
  
  display.setColor(BLACK);
  display.fillRect(0, SCROLL_TOP + displayLine * LINE_HEIGHT , 127, LINE_HEIGHT+1);   // black out the line on screen
  for (int i = 0; (c=textBuffer[bufferLine][i]) ; ++i) {
//Serial.print(String(c,HEX) + " ");
      if (c<4)   {           /// a flag
        if (irFlag)         /// at the end of an emphasized string
          {
            charsPrinted = printOnScroll(displayLine, style, pos, temp) / 9;
            style = REGULAR; pos+=charsPrinted; temp = ""; irFlag = false;
          }
        else                /// at the beginning of an emphasized string
          {
            if (temp.length()) {
              charsPrinted = printOnScroll(displayLine, style, pos, temp) / 9;
              style = REGULAR; pos+=charsPrinted; temp = ""; 
            }
            style = (FONT_ATTRIB) c; irFlag = true;
          }
      }
      else {                /// normal character - add it to temp
        temp += c;
      }
  }
//Serial.println("temp: " + temp);
  if (temp.length()) 
      printOnScroll(displayLine, style, pos, temp);
}


uint8_t printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, String mystring) {    // place a string onto the scroll area; line = 0, 1 or 2
  uint8_t w;
                                                                                        
  if (how > BOLD) 
          display.setColor(WHITE);
  else
    display.setColor(BLACK);

  if (how & BOLD)
        display.setFont(DialogInput_bold_15);
      else
        display.setFont(DialogInput_plain_15);

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  // convert the array characters into a String object

  //if (xpos == 0)
  //        w = 127;
  //else
          w = display.getStringWidth(mystring);

  display.fillRect(xpos*9, SCROLL_TOP + line * LINE_HEIGHT , w, LINE_HEIGHT+1);
      
  if (how > BOLD) 
          display.setColor(BLACK);
  else
    display.setColor(WHITE);
  
  display.drawString(xpos*9, SCROLL_TOP + line * LINE_HEIGHT, mystring);
  display.display();
  resetTOT();
  return w;         // we return the actual width of the output, in case of converted UTF8 characters
}


void clearLine(uint8_t line) {                                              /// clear a line - display is done somewhere else!
  display.setColor(BLACK);
  display.fillRect(0, SCROLL_TOP + line * LINE_HEIGHT , 127, LINE_HEIGHT+1);
  display.setColor(WHITE);
}


void clearScroll() {
  printToScroll_internal(REGULAR, "");
  clearScrollBuffer();
  clearLine(0);
  clearLine(1);
  clearLine(2);
  //Serial.println("clearScroll()");
}


 
void drawVolumeCtrl (boolean inverse, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t volume) {

    if (inverse)
      display.setColor(BLACK);
    else
      display.setColor(WHITE);
    
    display.fillRect(x, y, width, height);
    
    if (!inverse)
      display.setColor(BLACK);
    else
      display.setColor(WHITE);
    
    display.fillRect(x+2, y+4, (width-4) * volume / 100, height-8);
    display.drawHorizontalLine(x+2, y+height/2, width-4);
    display.display();
    resetTOT();
}


void displayScrollBar(boolean visible) {
  const int l_bar = 3*49/NoOfLines;

  if (visible) {            
            display.setColor(WHITE);
            display.drawVerticalLine(127, 15, 49);
            display.setColor(BLACK);
            display.drawVerticalLine(127, 15+(relPos*(49-l_bar)/maxPos), l_bar);
  } else {
            display.setColor(BLACK);
            display.drawVerticalLine(127, 15, 49);
  }
  display.display();
  resetTOT();
}


//// measure battery voltage in mV
int16_t batteryVoltage() {
    int32_t c, diff;

    #if BOARDVERSION == 3
      WiFi.mode( WIFI_MODE_NULL );      // make sure WiFi is not running, as it uses the same ADC as battery measurement!
      const float XS = 1.95;            //The returned reading is multiplied by this XS to get the battery voltage.
      
      analogSetClockDiv(128);           //  this value was found by experimenting - no clue what it really does :-(
      analogSetPinAttenuation(batteryPin,ADC_11db);
      //delay(75);
      c = 0;
      for (int i = 0; i < 2048; ++i)
        c += analogRead(batteryPin);
      
      c = (int) c*XS; 
      c = c / 2048;
      //printOnScroll(1, REGULAR, 1, String(c));
      analogSetClockDiv(1); // 5ms
    
    #elif BOARDVERSION == 2     // probably buggy - but BOARDVERSION 2 is not supported anymore, was prototype only
      adcAttachPin(batteryPin);

      const float XS = 1.255;
      c = analogRead(batteryPin);
      delay(100);
      c = analogRead(batteryPin);
      c = (int) c*XS;
     
     #endif
      
      return c;
}

///// display battery status as icon, parameter v: Voltage in mV
void displayBatteryStatus(int v) {    /// v in millivolts!

int a, b, c; String s; double d;
d = v / 50;
c = round(d) * 50;
// Serial.println("v: " + String (v) + " c: " + String(c));
a = c/1000;
b = (c-1000*a)/100;
s = "U: " + String(a) + "." + String(b);
printOnScroll(2, REGULAR, 0, s + " V");
  int w = constrain(v, 3200, 4050);
  w = map(w, 3200, 4050, 0, 31);
  display.drawRect(75, SCROLL_TOP + 2 * LINE_HEIGHT +3, 35, LINE_HEIGHT-4);
  display.drawRect(110, SCROLL_TOP + 2 * LINE_HEIGHT +5, 4, LINE_HEIGHT-8);
  display.fillRect(77, SCROLL_TOP + 2 * LINE_HEIGHT + 5 , w, LINE_HEIGHT -8);
  display.display();
}

void displayEmptyBattery() {                                /// display a warning and go to (return to) deep sleep
  display.drawRect(10, 11, 95, 50);
  display.drawRect(105, 26, 15, 20); 
  printOnScroll(1, INVERSE_BOLD, 4,  "EMPTY");   
  delay(4000);
  shutMeDown();
}


void resetTOT() {       //// reset the Time Out Timer - we do this whenever there is a screen update
    TOTcounter = millis();
}

void checkShutDown(boolean enforce) {       /// if enforce == true, we shut donw even if there was no time-out
  // unsigend long timeOut = ((morseState == loraTrx) || (morseState == morseTrx)) ? 450000 : 300000;  /// 7,5 or 5 minutes
  unsigned long timeOut;
  switch (p_timeOut) {
      case 4:   timeOut = ULONG_MAX;
                break;
      default:  timeOut = 300000UL * p_timeOut;
                break;
  }
  
  if ((millis() - TOTcounter) > timeOut || enforce == true )  {            
      display.clear();
      printOnScroll(1, INVERSE_BOLD, 0,  "Power OFF...");
      printOnScroll(2, REGULAR, 0, "RED to turn ON");
      display.display();
      delay (1500);
      shutMeDown();
  }  
}

void shutMeDown() {
  display.sleep();                //OLED sleep
  LoRa.sleep();                   //LORA sleep
  delay(50);
  #if BOARDVERSION == 3
  digitalWrite(Vext,HIGH);
  #endif
  delay(50);
  esp_deep_sleep_start();         // go to deep sleep
}




/// cwForLora packs element info (dit, dah, interelement space) into a String that can be sent via LoRA
///  element can be:
///  0: inter-element space
///  1: dit
///  2: dah
///  3: end of word -: cwForLora returns a string that is ready for sending to the LoRa transceiver

//  char loraTxBuffer[32];

void cwForLora (int element) {
  //static String result;
  //result.reserve(36);
  //static char buf[32];
  static int pairCounter = 0;
  uint8_t temp;
  uint8_t header;

  if (pairCounter == 0) {   // we start a brand new word for LoRA - clear buffer and set speed first
      for (int i=0; i<32; ++i)
          loraTxBuffer[i] = (char) 0;
          
      /// 1st byte: version + serial number
      header = ++loRaSerial % 64;
      //Serial.println("loRaSerial: " + String(loRaSerial));
      header += CWLORAVERSION * 64;        //// shift VERSION left 6 bits and add to the serial number
      loraTxBuffer[0] = header;

      temp = p_wpm * 4;                   /// shift left 2 bits
      loraTxBuffer[1] |= temp;
      pairCounter = 7;                    /// so far we have used 7 bit pairs: 4 in the first byte (protocol version+serial); 3 in the 2nd byte (wpm)
      //Serial.println(temp);
      //Serial.println(loraBuffer);
      }

  temp = element & B011;      /// take the two left bits
      //Serial.println("Temp before shift: " + String(temp));
  /// now store these two bits in the correct location in loraBuffer

  if (temp && (temp != 3)) {                 /// no need to do the operation with 0, nor with B11
      temp = temp << (2*(3-(pairCounter % 4)));
      loraTxBuffer[pairCounter/4] |= temp;
  }

  /// now increment, unless we got end of word
  /// have we get end of word, we got end of character (0) before

  if (temp != 3)
      ++pairCounter;
  else {  
      --pairCounter; /// we are at end of word and step back to end of character
      if (pairCounter % 4 != 0)      {           // do nothing if we have a zero in the topmost two bits already, as this was end of character
          temp = temp << (2*(3-(pairCounter % 4)));
          loraTxBuffer[pairCounter/4] |= temp;
      }
      pairCounter = 0;
  }
}


void sendWithLora() {           // hand this string over as payload to the LoRA transceiver
  // send packet
  LoRa.beginPacket();
  LoRa.print(loraTxBuffer);
  LoRa.endPacket();
  if (morseState == loraTrx)
      LoRa.receive();
}

void onReceive(int packetSize)
{
  String result;
  result.reserve(64);
  result = "";
  
  // received a packet
  // read packet
  for (int i = 0; i < packetSize; i++)
  {
    result += (char)LoRa.read();
    //Serial.print((char)LoRa.read());
  }
  if (packetSize < 49)
      storePacket(LoRa.packetRssi(), result);
  else
      Serial.println("LoRa Packet longer than 48 bytes! Discarded...");
  // print RSSI of packet
  //Serial.print("' with RSSI ");
  //Serial.println(LoRa.packetRssi());
  //Serial.print(" S-Meter: ");
  //Serial.println(map(LoRa.packetRssi(), -160, -20, 0, 100));
}



String CWwordToClearText(String cwword) {             // decode the Morse code character in cwword to clear text
  int ptr = 0;
  String result;
  result.reserve(40);
  String symbol;
  symbol.reserve(6);

  
  result = "";
  for (int i = 0; i < cwword.length(); ++i) {
      char c = cwword[i];
      switch (c) {
          case '1': ptr = CWtree[ptr].dit;
                    break;
          case '2': ptr = CWtree[ptr].dah;
                    break;
          case '0': symbol = CWtree[ptr].symb;

                    ptr = 0;
                    result += symbol;
                    break;
      }
  }
  symbol = CWtree[ptr].symb;
  //Serial.println("Symbol: " + symbol + " ptr: " + String(ptr));
  result += symbol;
  return encodeProSigns(result);
}


String encodeProSigns( String &input ) {
    /// clean up clearText   -   S <as>,  - A <ka> - N <kn> - K <sk> - H ch - V <ve>;
    input.replace("<as>", "S");
    input.replace("<ka>","A");
    input.replace("<kn>","N");
    input.replace("<sk>","K");
    input.replace("<ve>","V");
    input.replace("<ch>","H");
    input.replace("<err>","E");
    input.replace("¬", "U");
    //Serial.println(input);
    return input;
}


//// new buffer code: unpack when needed, to save buffer space. We just use 256 bytes of buffer, instead of 32k! 
//// in addition to the received packet, we need to store the RSSI as 8 bit positive number 
//// (it is always between -20 and -150, so an 8bit integer is fine as long as we store it without sign as an unsigned number)
//// the buffer is a 256 byte ring buffer with two pointers:
////   nextBuRead where the next packet starts for reading it out; is incremented by l to get the next buffer read position
////      you can read a packet as long as the buffer is not empty, so we need to check bytesBuFree before we read! if it is 256, the buffer is empty!
////      with a read, the bytesBuFree has to be increased by the number of bytes read
////   nextBuWrite where the next packet should be written; @write:
////       increment nextBuWrite by l to get new pointer; and decrement bytesBuFree by l to get new free space
//// we also need a variable that shows how many bytes are free (not in use): bytesBuFree
//// if the next packet to be stored is larger than bytesBuFree, it is discarded
//// structure of each packet:
////    l:  1 uint8_t length of packet
////    r:  1 uint8_t rssi as a positive number
////    d:  (var. length) data packet as received by LoRa
//// functions:
////    int loRaBuWrite(int rssi, String packet): returns length of buffer if successful. otherwise 0
////    uint8_t loRaBuRead(uint8_t* buIndex): returns length of packet, and index where to read in buffer by reference
////    boolean loRaBuReady():  true if there is something in the buffer, false otherwise
////      example:
////        (somewhere else as global var: ourBuffer[256]
////        uint8_t myIndex;
////        uint8_t mylength;
////        foo() {
////          myLength = loRaBuRead(&myIndex);
////          if (myLength != 0) 
////            doSomethingWith(ourBuffer[myIndex], myLength);
////        }


uint8_t loRaBuWrite(int rssi, String packet) {
////   int loRaBuWrite(int rssi, String packet): returns length of buffer if successful. otherwise 0
////   nextBuWrite where the next packet should be written; @write:
////       increment nextBuWrite by l to get new pointer; and decrement bytesBuFree by l to get new free space
  uint8_t l, posRssi;

  posRssi = (uint8_t) abs(rssi);
  l = 2 + packet.length();
  if (byteBuFree < l)                               // buffer full - discard packet
      return 0;
  loRaRxBuffer[nextBuWrite++] = l;
  loRaRxBuffer[nextBuWrite++] = posRssi;
  for (int i = 0; i < packet.length(); ++i) {       // do this for all chars in the packet
    loRaRxBuffer [nextBuWrite++] = packet[i];       // at end nextBuWrite is alread where it should be
  }
  byteBuFree -= l;
  //Serial.println(byteBuFree);
  //Serial.println((String)loRaRxBuffer[0]);
  return l;
}

boolean loRaBuReady() {
  if (byteBuFree == 256)
    return (false);
  else
    return true;
}


uint8_t loRaBuRead(uint8_t* buIndex) {
////    uint8_t loRaBuRead(uint8_t* buIndex): returns length of packet, and index where to read in buffer by reference
  uint8_t l;  
  if (byteBuFree == 256)
    return 0;
  else {
    l = loRaRxBuffer[nextBuRead++];
    *buIndex = nextBuRead;
    byteBuFree += l;
    --l;
    nextBuRead += l;
    return l;
  }
}




void storePacket(int rssi, String packet) {             // whenever we receive something, we just store it in our buffer
  if (loRaBuWrite(rssi, packet) == 0)
    Serial.println("LoRa Buffer full");
}


/// decodePacket analyzes packet as received and stored in buffer
/// returns the header byte (protocol version*64 + 6bit packet serial number
//// byte 0 (added by receiver): RSSI
//// byte 1: header; first two bits are the protocol version (curently 01), plus 6 bit packet serial number (starting from random)
//// byte 2: first 6 bits are wpm (must be between 5 and 60; values 00 - 04 and 61 to 63 are invalid), the remaining 2 bits are already data payload!


uint8_t decodePacket(int* rssi, int* wpm, String* cwword) {
  uint8_t l, c, header=0;
  uint8_t index = 0;

  l = loRaBuRead(&index);           // where are we in  the buffer, and how long is the total packet inkl. rssi byte?

  for (int i = 0; i < l; ++i) {     // decoding loop
    c = loRaRxBuffer[index+i];

    switch (i) {
      case  0:  * rssi = (int) (-1 * c);    // the rssi byte
                break;
      case  1:  header = c;
                break;
      case  2:  *wpm = (uint8_t) (c >> 2);  // the first data byte contains the wpm info in the first six bits, and actual morse in the remaining two bits
                                            // now take remaining two bits and store them in CWword as ASCII
                *cwword = (char) ((c & B011) +48); 
                break;
      default:                              // decode the rest of the packet; we do this for all but the first byte  /// we need to handle end of word!!! therefore the break
                for (int j = 0; j < 4; ++j) {
                    char cc = ((c >> 2*(3-j)) & B011) ;                // we store them as ASCII characters 0,1,2,3 !
                    if (cc != 3) {
                        *cwword  += (char) (cc + 48);
                    }
                    else break;
                }
                break;
    } // end switch
  }   // end for
  return header;
}      // end decodePacket

/////////////////////////////////////   MORSE DECODER ///////////////////////////////


////////////////////////////
///// Routines for morse decoder - to a certain degree based on code by Hjalmar Skovholm Hansen OZ1JHM - copyleft licence
////////////////////////////

void setupMorseDecoder() {
  /// here we will do the init for decoder mode
  //trainerMode = false;
  encoderState = volumeSettingMode;
  
  display.clear();
  printOnScroll(1, REGULAR, 0, "Start Decoder" );
  delay(750);
  display.clear();
  displayTopLine();
  drawInputStatus(false);
  printToScroll(REGULAR,"");      // clear the buffer
  
  speedChanged = true;
  displayCWspeed();
  displayVolume();
    
  /// set up variables for Goertzel Morse Decoder
  setupGoertzel();
  filteredState = filteredStateBefore = false;
  decoderState = LOW_;
  ditAvg = 60;
  dahAvg = 180;
}

//const float sampling_freq = 106000.0;
//const float target_freq = 698.0; /// adjust for your needs see above
//const int goertzel_n = 304; //// you can use:         152, 304, 456 or 608 - thats the max buffer reserved in checktone()//

void setupGoertzel () {                 /// pre-compute some values that are compute-imntensive and won't change anyway
  goertzel_n = ( p_goertzelBandwidth == 0 ? 152 : 608);                 // update Goertzel parameters depending on chosen bandwidth
  magnitudelimit_low = ( p_goertzelBandwidth? 160000 : 40000);          // values found by experimenting
  magnitudelimit = magnitudelimit_low;

  bw = (sampling_freq / goertzel_n ); //348

  int  k;
  float omega;
  k = (int) (0.5 + ((goertzel_n * target_freq) / sampling_freq)); // 2
  omega = (2.0 * PI * k) / goertzel_n ;                           //0,041314579
  sine = sin(omega);                                              // 0,007210753
  cosine = cos(omega);                                            // 0,999999739
  coeff = 2.0 * cosine;                                           // 1,999999479
  }

#define straightPin leftPin

boolean straightKey() {            // return true if a straight key was closed, or a touch paddle touched
if ((morseState == morseDecoder) && ((!digitalRead(straightPin)) || leftKey || rightKey) )
    return true;
else return false;
}

boolean checkTone() {              /// check if we have a tone signal at A6 with Gortzel's algorithm, and apply some noise blanking as well
                                   /// the result will be in globale variable filteredState
                                   /// we return true when we detected a change in state, false otherwise!
  
  float magnitude ;

  static boolean realstate = false;
  static boolean realstatebefore = false;
  static unsigned long lastStartTime = 0;
  
  uint16_t testData[1216];         /// buffer for freq analysis - max. 608 samples; you could increase this (and n) to a max of 1216, for sample time 10 ms, and bw 88 Hz

///// check straight key first before you check audio in.... (unless we are in transceiver mode)
///// straight key is connected to external paddle connector (tip), i.e. the same as the left pin (dit normally)

if (straightKey() ) {
    realstate = true;
    //Serial.println("Straight Key!");
    //keyTx = true;
    }
else {
    realstate = false;
    //keyTx = false;
    for (int index = 0; index < goertzel_n ; index++)
        testData[index] = analogRead(audioInPin);
    //Serial.println("Read and stored analog values!");
    for (int index = 0; index < goertzel_n ; index++) {
      float Q0;
      Q0 = coeff * Q1 - Q2 + (float) testData[index];
      Q2 = Q1;
      Q1 = Q0;
    }
    //Serial.println("Calculated Q1 and Q2!");

    float magnitudeSquared = (Q1 * Q1) + (Q2 * Q2) - (Q1 * Q2 * coeff); // we do only need the real part //
    magnitude = sqrt(magnitudeSquared);
    Q2 = 0;
    Q1 = 0;
  
   //Serial.println("Magnitude: " + String(magnitude) + " Limit: " + String(magnitudelimit));   //// here you can measure magnitude for setup..
  
    ///////////////////////////////////////////////////////////
    // here we will try to set the magnitude limit automatic //
    ///////////////////////////////////////////////////////////
  
    if (magnitude > magnitudelimit_low) {
      magnitudelimit = (magnitudelimit + ((magnitude - magnitudelimit) / 6)); /// moving average filter
    }
  
    if (magnitudelimit < magnitudelimit_low)
      magnitudelimit = magnitudelimit_low;
  
    ////////////////////////////////////
    // now we check for the magnitude //
    ////////////////////////////////////
  
    if (magnitude > magnitudelimit * 0.6) // just to have some space up
      realstate = true;
    else
      realstate = false;
  }



  /////////////////////////////////////////////////////
  // here we clean up the state with a noise blanker //
  // (debouncing)                                    //
  /////////////////////////////////////////////////////

  if (realstate != realstatebefore)
    lastStartTime = millis();
  if ((millis() - lastStartTime) > nbtime) {
    if (realstate != filteredState) {
      filteredState = realstate;
    }
  }
  realstatebefore = realstate;

 if (filteredState == filteredStateBefore)
  return false;                                 // no change detected in filteredState
 else {
    filteredStateBefore = filteredState;
    return true;                                // change detected in filteredState
 }
}   /// end checkTone()


void doDecode() {
  float lacktime;
  int wpm;

    switch(decoderState) {
      case INTERELEMENT_: if (checkTone()) {
                              ON_();
                              decoderState = HIGH_;
                          } else {
                              lowDuration = millis() - startTimeLow;                        // we record the length of the pause
                              lacktime = 2.2;                                               ///  when high speeds we have to have a little more pause before new letter 
                              //if (p_wpm > 35) lacktime = 2.7;
                              //  else if (p_wpm > 30) lacktime = 2.6;
                              if (lowDuration > (lacktime * ditAvg)) {
                                displayMorse();                                             /// decode the morse character and display it
                                wpm = (p_wpm + (int) (7200 / (dahAvg + 3*ditAvg))) / 2;     //// recalculate speed in wpm
                                if (p_wpm != wpm) {
                                  p_wpm = wpm;
                                  speedChanged = true;
                                }
                                decoderState = INTERCHAR_;
                              }
                          }
                          break;
      case INTERCHAR_:    if (checkTone()) {
                              ON_();
                              decoderState = HIGH_;
                          } else {
                              lowDuration = millis() - startTimeLow;             // we record the length of the pause
                              lacktime = 5;                 ///  when high speeds we have to have a little more pause before new word
                              if (p_wpm > 35) lacktime = 6;
                                else if (p_wpm > 30) lacktime = 5.5;
                              if (lowDuration > (lacktime * ditAvg)) {
                                   printToScroll(REGULAR, " ");                       // output a blank                                
                                   decoderState = LOW_;
                              }
                          }
                          break;
      case LOW_:          if (checkTone()) {
                              ON_();
                              decoderState = HIGH_;
                          }
                          break;
      case HIGH_:         if (checkTone()) {
                              OFF_();
                              decoderState = INTERELEMENT_;
                          }
                          break;
    } 
}

void ON_() {                                  /// what we do when we just detected a rising flank, from low to high
   unsigned long timeNow = millis();
   lowDuration = timeNow - startTimeLow;             // we record the length of the pause
   startTimeHigh = timeNow;                          // prime the timer for the high state

   keyOut(true, false, notes[p_sidetoneFreq], p_sidetoneVolume);

   drawInputStatus(true);
   
   if (lowDuration < ditAvg * 2.4)                    // if we had an inter-element pause,
      recalculateDit(lowDuration);                    // use it to adjust speed
}

void OFF_() {                                 /// what we do when we just detected a falling flank, from high to low
  unsigned long timeNow = millis();
  unsigned int threshold = (int) ( ditAvg * sqrt( dahAvg / ditAvg));

  //Serial.print("threshold: ");
  //Serial.println(threshold);
  highDuration = timeNow - startTimeHigh;
  startTimeLow = timeNow;

  if (highDuration > (ditAvg * 0.5) && highDuration < (dahAvg * 2.5)) {    /// filter out VERY short and VERY long highs
      if (highDuration < threshold) { /// we got a dit -
            treeptr = CWtree[treeptr].dit;
            //Serial.print(".");
            recalculateDit(highDuration);
      }
      else  {        /// we got a dah
            treeptr = CWtree[treeptr].dah;   
            //Serial.print("-");   
            recalculateDah(highDuration);                 
      }
  }
  //pwmNoTone();                     // stop side tone
  //digitalWrite(keyerPin, LOW);      // stop keying Tx
  keyOut(false, false, 0, 0);
  ///////
  drawInputStatus(false);

}

void drawInputStatus( boolean on) {
  if (on)
    display.setColor(BLACK);
  else
      display.setColor(WHITE);
  display.fillRect(1, 1, 20, 13);   
  display.display();
}



void recalculateDit(unsigned long duration) {       /// recalculate the average dit length
  ditAvg = (4*ditAvg + duration) / 5;
  //Serial.print("ditAvg: ");
  //Serial.println(ditAvg);
  //nbtime =ditLength / 5; 
  nbtime = constrain(ditAvg/5, 7, 20);
  //Serial.println(nbtime);
}

void recalculateDah(unsigned long duration) {       /// recalculate the average dah length
  //static uint8_t rot = 0;
  //static unsigned long collector;

  if (duration > 2* dahAvg)   {                       /// very rapid decrease in speed!
      dahAvg = (dahAvg + 2* duration) / 3;            /// we adjust faster, ditAvg as well!
      ditAvg = ditAvg/2 + dahAvg/6;
  }
  else { 
      dahAvg = (3* ditAvg + dahAvg + duration) / 3;
  }
    //Serial.print("dahAvg: ");
    //Serial.println(dahAvg);
    
}


void keyOut(boolean on,  boolean fromHere, int f, int volume) {                                      
  //// generate a side-tone with frequency f when on==true, or turn it off
  //// differentiate external (decoder, sometimes cw_generate) and internal (keyer, sometimes Cw-generate) side tones
  //// key transmitter (and line-out audio if we are in a suitable mode)

  static boolean intTone = false;
  static boolean extTone = false;

  static int intPitch, extPitch;

// Serial.println("keyOut: " + String(on) + String(fromHere));
  if (on) {
      if (fromHere) {
        intPitch = f;
        intTone = true;
        pwmTone(intPitch, volume, true);
        keyTransmitter();
      } else {                    // not from here
        extTone = true;
        extPitch = f;
        if (!intTone) 
          pwmTone(extPitch, volume, false);
        }
  } else {                      // key off
        if (fromHere) {
          intTone = false;
          if (extTone)
            pwmTone(extPitch, volume, false);
          else
            pwmNoTone();
          digitalWrite(keyerPin, LOW);      // stop keying Tx
        } else {                 // not from here
          extTone = false;
          if (!intTone)
            pwmNoTone();
        }
  }   // end key off
}

///////////////// a test function for adjusting audio levels

void audioLevelAdjust() {
    uint16_t i, maxi, mini;
    uint16_t testData[1216];

    display.clear();
    printOnScroll(0, BOLD, 0, "Audio Adjust");
    printOnScroll(1, REGULAR, 0, "End with RED");
    keyTx = true;
    keyOut(true,  true, 698, 0);                                  /// we generate a side tone, f=698 Hz, also on line-out, but with vol down on speaker
    while (true) {
        volButton.Update();
        if (volButton.clicks)
            break;                                                /// pressing the red button gets you out of this mode!
        for (i = 0; i < goertzel_n ; ++i)
            testData[i] = analogRead(audioInPin);                 /// read analog input
        maxi = mini = testData[0];
        for (i = 1; i< goertzel_n ; ++i) {
            if (testData[i] < mini)
              mini = testData[i];
            if (testData[i] > maxi)
              maxi = testData[i];
        }
        int a, b, c;
        a = map(mini, 0, 4096, 0, 125);
        b = map(maxi, 0, 4000, 0, 125);
        c = b - a;
        clearLine(2);
        display.drawRect(5, SCROLL_TOP + 2 * LINE_HEIGHT +5, 102, LINE_HEIGHT-8);
        display.drawRect(30, SCROLL_TOP + 2 * LINE_HEIGHT +5, 52, LINE_HEIGHT-8);
        display.fillRect(a, SCROLL_TOP + 2 * LINE_HEIGHT + 7 , c, LINE_HEIGHT -11);
        display.display();
    } // end while
    keyOut(false,  true, 698, 0);                                  /// stop keying
    keyTx = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// stuff using WiFi - ask for access point credentials, upload player file, do OTA software update
///////////////////////////////////////////////////////////////////////////////////////////////////

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void startAP() {
  //IPaddress a;
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(ssid);
  WiFi.softAP(ssid);
  //a = WiFi.softAPIP();
  display.clear();
  printOnStatusLine(true, 0,    "Enter Wifi Info @");
  printOnScroll(0, REGULAR, 0,  "AP: morserino");
  printOnScroll(1, REGULAR, 0,  "URL: m32.local");
  printOnScroll(2, REGULAR, 0,  "RED to abort");

  //printOnScroll(2, REGULAR, 0, WiFi.softAPIP().toString());
  //Serial.println(WiFi.softAPIP());

  startMDNS();
  
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", myForm);
  });
  
  server.on("/set", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "Wifi Info updated - now restarting Morserino-32...");
    p_wlanSSID = server.arg("ssid");
    p_wlanPassword = server.arg("pw");
    //Serial.println("SSID: " + p_wlanSSID + " Password: " + p_wlanPassword);
    pref.begin("morserino", false);             // open the namespace as read/write
    pref.putString("wlanSSID", p_wlanSSID);
    pref.putString("wlanPassword", p_wlanPassword);
    pref.end();
    
    ESP.restart();
  });
  
  server.onNotFound(handleNotFound);
  
  server.begin();
  while (true) {
      server.handleClient();
      delay(20);
      volButton.Update();
      if (volButton.clicks) {
        display.clear();
        printOnStatusLine(true, 0, "Resetting now...");
        delay(2000);
        ESP.restart();
      }
  }
}


void updateFirmware()   {                   /// start wifi client, web server and upload new binary from a local computer
  if (! wifiConnect())
    return;
 
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updateLoginIndex);
  });
  
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      //Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  //Serial.println("Starting web server");
  server.begin();
  display.clear();
  printOnStatusLine(true, 0, "Waiting f. Update ");
  printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  printOnScroll(1, REGULAR, 0,  "IP:");
  printOnScroll(2, REGULAR, 0, WiFi.localIP().toString());
  while(true) {
    server.handleClient();
    delay(10);
  }
}
  

boolean wifiConnect() {                   // connect to local WLAN
  // Connect to WiFi network
  if (p_wlanSSID == "") 
      return errorConnect(String("WiFi Not Conf"));
    
  WiFi.begin(p_wlanSSID.c_str(), p_wlanPassword.c_str());

  // Wait for connection
  long unsigned int wait = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if ((millis() - wait) > 20000)
      return errorConnect(String("No WiFi:"));
  }
  //Serial.print("Connected to ");
  //Serial.println(p_wlanSSID);
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
  startMDNS();
  return true;
}

boolean errorConnect(String msg) {
  display.clear();
  printOnStatusLine(true, 0, "Not connected");
  printOnScroll(0, INVERSE_BOLD, 0, msg);
  printOnScroll(1, REGULAR, 0, p_wlanSSID);
  delay(3500);
  return false;
}

void startMDNS() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://m32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
      if (MDNS.begin(host))
        break;
    }
  }
  //Serial.println("mDNS responder started");
}

void uploadFile() {
  if (! wifiConnect())
    return;
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", uploadLoginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

server.on("/update", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
    ESP.restart();},                                  // Send status 200 (OK) to tell the client we are ready to receive; when done, restart the ESP32
    handleFileUpload                                    // Receive and save the file
  );
  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  //Serial.println("HTTP server started");
  display.clear();
  display.clear();
  printOnStatusLine(true, 0, "Waiting f. Upload ");
  printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  printOnScroll(1, REGULAR, 0,  "IP:");
  printOnScroll(2, REGULAR, 0, WiFi.localIP().toString());  
  while(true) {
    server.handleClient();
    //delay(5);
  }
}


String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  //Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                            // If there's a compressed version available
      path += ".gz";                                          // Use the compressed verion
    File file = SPIFFS.open(path, "r");                       // Open the file
    server.streamFile(file, contentType);                     // Send it to the client
    file.close();                                             // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  //Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    //Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open("/player.txt", "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      p_fileWordPointer = 0;                              // reset word counter for file player
      pref.begin("morserino", false);              // open the namespace as read/write
          pref.putUInt("fileWordPtr", p_fileWordPointer);
      pref.end(); 

      //Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      //server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      //server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}


String getWord() {
  String result = "";
  byte c;
  
  while (file.available()) {
      c=file.read();
      //Serial.println((int) c);
      if (!isSpace(c))
        result += (char) c;
      else if (result.length() > 0)    {               // end of word
        ++p_fileWordPointer;
        //Serial.println("word: " + result);
        return result;
      }
    }
    file.close(); file = SPIFFS.open("/player.txt");
    p_fileWordPointer = 0;
    while (!file.available())
      ;
    return result;                                    // at eof
}

String cleanUpText(String w) {                        // all to lower case, and convert umlauts
  String result = "";
  char c;
  result.reserve(64);
  w.toLowerCase();
  w = utf8umlaut(w);
  
  for (unsigned int i = 0; i<w.length(); ++i) {
    if (kochChars.indexOf(c = w.charAt(i)) != -1)
      result += c;
  }
  return result;
}


String utf8umlaut(String s) { /// replace umtf umlauts with digraphs, and interpret pro signs, written e.g. as [kn] or <kn>
      s.replace("ä", "ae");
      s.replace("ö", "oe");
      s.replace("ü", "ue");
      s.replace("Ä", "ae");
      s.replace("Ö", "oe");
      s.replace("Ü", "ue");
      s.replace("ß", "ss");
      s.replace("[", "<");
      s.replace("]", ">");
      s.replace("<ar>", "+");
      s.replace("<bt>", "=");
      s.replace("<as>", "S");
      s.replace("<ka>", "K");
      s.replace("<kn>", "N");
      s.replace("<sk>", "K");
      s.replace("<ve>", "V");
      return s;
}

void skipWords(uint32_t count) {             /// just skip count words in open file fn
  while (count > 0) {
    getWord();
    --count;
  }
}

/////////////// READING and WRITING parameters from / into Non Volatile Storage, using ESP32 preferences

void readPreferences(String repository) {
  unsigned int l = 15;
  char repName[l];
  uint8_t temp;
  uint32_t tempInt;  

  boolean atStart = false;
  
  if (repository == "morserino")
    atStart = true;

  repository.toCharArray(repName, l);
  // Serial.println("Reading from repository: " + String(repName));
  // read preferences from non-volatile storage
  // if version cannot be read, we have a new ESP32 and need to write the preferences first

  if (atStart) 
    pref.begin(repName, false);                // open namespace in read/write mode
  else
    pref.begin(repName, true);                 // read only in all other cases
 
  /// new code for reading preferences values - we check if we have a value, and if yes, we use it; if no, we use and write a default value

    if (atStart) {
      if ((temp = pref.getUChar("version_major")) != p_version_major)
         pref.putUChar("version_major", p_version_major);
      if ((temp = pref.getUChar("version_minor")) != p_version_minor)
         pref.putUChar("version_minor", p_version_minor);
    }
      
    if ((temp = pref.getUChar("sidetoneFreq")))
       p_sidetoneFreq = temp;
    else if (atStart)
       pref.putUChar("sidetoneFreq", p_sidetoneFreq);

    if ((temp = pref.getUChar("wpm")))
       p_wpm = temp;
    else if (atStart)
       pref.putUChar("wpm", p_wpm);

    if ((temp = pref.getUChar("sidetoneVolume",255)) != 255)
       p_sidetoneVolume = temp;
    else if (atStart)
       pref.putUChar("sidetoneVolume", p_sidetoneVolume);

    if ((temp = pref.getUChar("keyermode")))
       p_keyermode = temp;
    else if (atStart)
       pref.putUChar("keyermode", p_keyermode);

    if ((temp = pref.getUChar("farnsworthMode")))
       p_interCharSpace = temp;
    else if (atStart)
       pref.putUChar("farnsworthMode", p_interCharSpace);

    if ((temp = pref.getUChar("ACSlength",255)) != 255)
       p_ACSlength = temp;
    else if (atStart)
       pref.putUChar("ACSlength", p_ACSlength);

    if ((temp = pref.getUChar("keyTrainerMode", 255)) != 255)
       p_keyTrainerMode = temp;
    else if (atStart)
       pref.putUChar("keyTrainerMode", p_keyTrainerMode);
 
    if ((temp = pref.getUChar("randomLength")))
       p_randomLength = temp;
    else if (atStart)
       pref.putUChar("randomLength", p_randomLength);

    if ((temp = pref.getUChar("randomOption", 255)) != 255)
       p_randomOption = temp;
    else if (atStart)
       pref.putUChar("randomOption", p_randomOption);

    if ((temp = pref.getUChar("callLength", 255)) != 255)
       p_callLength = temp;
    else if (atStart)
       pref.putUChar("callLength", p_callLength);
       
    if ((temp = pref.getUChar("abbrevLength", 255)) != 255)
       p_abbrevLength = temp;
    else if (atStart)
       pref.putUChar("abbrevLength", p_abbrevLength);

    if ((temp = pref.getUChar("wordLength", 255)) != 255)
       p_wordLength = temp;
    else if (atStart)
       pref.putUChar("wordLength", p_wordLength);

    if ((temp = pref.getUChar("trainerDisplay", 255)) != 255)
       p_trainerDisplay = temp;
    else if (atStart)
       pref.putUChar("trainerDisplay", p_trainerDisplay);

    if ((temp = pref.getUChar("echoDisplay", 255)) != 255)
       p_echoDisplay = temp;
    else if (atStart)
       pref.putUChar("echoDisplay", p_echoDisplay);

    if ((temp = pref.getUChar("curtisBTiming", 255)) != 255)
       p_curtisBTiming = temp;
    else if (atStart)
       pref.putUChar("curtisBTiming", p_curtisBTiming);

    if ((temp = pref.getUChar("curtisBDotT", 255)) != 255)
       p_curtisBDotTiming = temp;
    else if (atStart)
       pref.putUChar("curtisBDotT", p_curtisBDotTiming);

    if ((temp = pref.getUChar("interWordSpace")))
       p_interWordSpace = temp;
    else if (atStart)
       pref.putUChar("interWordSpace", p_interWordSpace);

    if ((temp = pref.getUChar("echoRepeats", 255)) != 255)
       p_echoRepeats = temp;
    else if (atStart)
       pref.putUChar("echoRepeats", p_echoRepeats);

    if ((temp = pref.getUChar("echoToneShift", 255)) != 255)
       p_echoToneShift = temp;
    else if (atStart)
       pref.putUChar("echoToneShift", p_echoToneShift);
    
    if (atStart) {
        if ((temp = pref.getUChar("kochFilter")))
           p_kochFilter = temp;
        else 
           pref.putUChar("kochFilter", p_kochFilter);
    }

    if ((temp = pref.getUChar("loraTrainerMode")))
        p_loraTrainerMode = temp;
    else if (atStart)
        pref.putUChar("loraTrainerMode", p_loraTrainerMode);

    if ((temp = pref.getUChar("goertzelBW")))
        p_goertzelBandwidth = temp;
    else if (atStart)
        pref.putUChar("goertzelBW", p_goertzelBandwidth);

    if ((temp = pref.getUChar("latency")))
        p_latency = temp;
    else if (atStart)
        pref.putUChar("latency", p_latency);
    
    if ((temp = pref.getUChar("randomFile")))
        p_randomFile = temp;
        
    if ((temp = pref.getUChar("lastExecuted")))
       p_menuPtr = temp;
    //Serial.println("read: p_menuPtr = " + String(p_menuPtr));

    if ((temp = pref.getUChar("timeOut")))
       p_timeOut = temp;
    else if (atStart)
       pref.putUChar("timeOut", p_timeOut);

    if ((temp = pref.getUChar("loraSyncW")))
        p_loraSyncW = temp;
    else if (atStart)
        pref.putUChar("loraSyncW", p_loraSyncW);

    if ((temp = pref.getUChar("maxSequence", p_maxSequence)))
        p_maxSequence = temp;
        
    p_didah = pref.getBool("didah", true);
    p_useExtPaddle = pref.getBool("useExtPaddle");
    p_encoderClicks = pref.getBool("encoderClicks", true);
    p_echoConf = pref.getBool("echoConf", true);
    p_wordDoubler = pref.getBool("wordDoubler");
    p_speedAdapt  = pref.getBool("speedAdapt");

    p_wlanSSID = pref.getString("wlanSSID");
    p_wlanPassword = pref.getString("wlanPassword");
    p_fileWordPointer = pref.getUInt("fileWordPtr");
    p_lcwoKochSeq = pref.getBool("lcwoKochSeq");
    p_quickStart = pref.getBool("quickStart");

    p_autoStop  = pref.getBool("autoStop");


    if (atStart) {
      if (temp = pref.getUChar("loraBand"))
        p_loraBand = temp;
      else
       p_loraBand = 0;
  
      if (tempInt = pref.getUInt("loraQRG")) {
        p_loraQRG = tempInt;
      }
      else
        p_loraQRG = QRG433;
  
     if (temp = pref.getUChar("snapShots")) {
        p_snapShots = temp;
        updateMemory(temp);
     }  // end: we have snapshots
    }  // endif atStart
   pref.end();
   updateTimings();
}

void writePreferences(String repository) {
  unsigned int l = 15;
  char repName[l];
  uint8_t temp;
  uint32_t tempInt;  

  boolean morserino = false;
  
  if (repository == "morserino")
    morserino = true;
//Serial.println("Writing to repository: " + repository);
  repository.toCharArray(repName, l);

  pref.begin(repName, false);                // open namespace in read/write mode
 
    if (p_sidetoneFreq != pref.getUChar("sidetoneFreq"))
        pref.putUChar("sidetoneFreq", p_sidetoneFreq);    
    if (p_didah != pref.getBool("didah"))
        pref.putBool("didah", p_didah);
    if (p_keyermode != pref.getUChar("keyermode"))
        pref.putUChar("keyermode", p_keyermode);
    if (p_interCharSpace != pref.getUChar("farnsworthMode"))
        pref.putUChar("farnsworthMode", p_interCharSpace);
    if (p_useExtPaddle != pref.getBool("useExtPaddle"))
        pref.putBool("useExtPaddle", p_useExtPaddle);
    if (p_ACSlength != pref.getUChar("ACSlength"))
        pref.putUChar("ACSlength", p_ACSlength);
    if (p_keyTrainerMode != pref.getUChar("keyTrainerMode"))
        pref.putUChar("keyTrainerMode", p_keyTrainerMode);
    if (p_encoderClicks != pref.getBool("encoderClicks"))
        pref.putBool("encoderClicks", p_encoderClicks);   
    if (p_randomLength != pref.getUChar("randomLength"))
        pref.putUChar("randomLength", p_randomLength);
    if (p_randomOption != pref.getUChar("randomOption"))
        pref.putUChar("randomOption", p_randomOption);
    if (p_callLength != pref.getUChar("callLength"))
        pref.putUChar("callLength", p_callLength);
    if (p_abbrevLength != pref.getUChar("abbrevLength")) {
        pref.putUChar("abbrevLength", p_abbrevLength);
        if (morserino) 
          createKochAbbr(p_abbrevLength, p_kochFilter); // update the abbrev array!
    }
    if (p_wordLength != pref.getUChar("wordLength")) {
        pref.putUChar("wordLength", p_wordLength);
        if (morserino)
          createKochWords(p_wordLength, p_kochFilter) ;  // update the word array!
    }
    if (p_trainerDisplay != pref.getUChar("trainerDisplay"))
        pref.putUChar("trainerDisplay", p_trainerDisplay);
    if (p_echoDisplay != pref.getUChar("echoDisplay"))
        pref.putUChar("echoDisplay", p_echoDisplay);
    if (p_curtisBTiming != pref.getUChar("curtisBTiming"))
        pref.putUChar("curtisBTiming", p_curtisBTiming);
    if (p_curtisBDotTiming != pref.getUChar("curtisBDotT")) 
        pref.putUChar("curtisBDotT", p_curtisBDotTiming);
    if (p_interWordSpace != pref.getUChar("interWordSpace"))
        pref.putUChar("interWordSpace", p_interWordSpace);
    if (p_echoRepeats != pref.getUChar("echoRepeats"))
        pref.putUChar("echoRepeats", p_echoRepeats);
    if (p_echoToneShift != pref.getUChar("echoToneShift"))
        pref.putUChar("echoToneShift", p_echoToneShift);
    if (p_echoConf != pref.getBool("echoConf"))
        pref.putBool("echoConf", p_echoConf);
    if (morserino) {
        if (p_kochFilter != pref.getUChar("kochFilter")) {
          pref.putUChar("kochFilter", p_kochFilter);
          createKochWords(p_wordLength, p_kochFilter) ;  // update the arrays!
          createKochAbbr(p_abbrevLength, p_kochFilter);
        }
    }
    if (p_lcwoKochSeq != pref.getBool("lcwoKochSeq")) {
        pref.putBool("lcwoKochSeq", p_lcwoKochSeq);
        if (morserino) {
          kochChars = p_lcwoKochSeq ? lcwoKochChars : morserinoKochChars;
          createKochWords(p_wordLength, p_kochFilter) ;  // update the arrays!
          createKochAbbr(p_abbrevLength, p_kochFilter);
        }
    }
    if (p_wordDoubler != pref.getBool("wordDoubler"))
        pref.putBool("wordDoubler", p_wordDoubler);
    if (p_speedAdapt != pref.getBool("speedAdapt"))
        pref.putBool("speedAdapt", p_speedAdapt);
    if (p_loraTrainerMode != pref.getUChar("loraTrainerMode"))
        pref.putUChar("loraTrainerMode", p_loraTrainerMode);
    if (p_goertzelBandwidth != pref.getUChar("goertzelBW")) {
        pref.putUChar("goertzelBW", p_goertzelBandwidth);
        if (morserino)
          setupGoertzel();
    }
    if (p_loraSyncW != pref.getUChar("loraSyncW")) {
        pref.putUChar("loraSyncW", p_loraSyncW);
        if (morserino)
          LoRa.setSyncWord(p_loraSyncW);               
    }
    if (p_maxSequence != pref.getUChar("maxSequence")) 
        pref.putUChar("maxSequence", p_maxSequence);

    if (p_latency != pref.getUChar("latency"))
        pref.putUChar("latency", p_latency);
    if (p_randomFile != pref.getUChar("randomFile"))
        pref.putUChar("randomFile", p_randomFile);
    if (p_timeOut != pref.getUChar("timeOut"))
        pref.putUChar("timeOut", p_timeOut);
    if (p_quickStart != pref.getBool("quickStart"))
        pref.putBool("quickStart", p_quickStart);
    if (p_autoStop != pref.getBool("autoStop"))
        pref.putBool("autoStop", p_autoStop);

    if (p_snapShots != pref.getUChar("snapShots"))
        pref.putUChar("snapShots", p_snapShots);

    if (! morserino)  {
        pref.putUChar("lastExecuted", p_menuPtr);   // store last executed command in snapshots
        //Serial.println("write: last executed: " + String(p_menuPtr));
    }


  pref.end();
}

boolean  recallSnapshot() {         // return true if we selected a real recall, false when it was cancelled
    String snapname;
    String text;

    memPtr = 0;
    displayKeyerPreferencesMenu(posSnapRecall);
    if (!adjustKeyerPreference(posSnapRecall)) {
        //Serial.println("recall memPtr: " + String(memPtr));
        text = "Snap " + String(memories[memPtr]+1) + " RECALLD";
        if(memCounter) {
          if (memPtr != memCounter)  {
            snapname = "snap" + String(memories[memPtr]);
            //Serial.println("recall snapname: " + snapname);
            readPreferences(snapname);
            printOnScroll(2, BOLD, 0, text);
            //Serial.println("after recall - p_menuPtr: " + String(p_menuPtr));
            delay(1000);
            return true;
          }
          return false;
        }
    } return false;
      
}

boolean storeSnapshot(uint8_t menu) {        // return true if we selected a real store, false when it was cancelled
    String snapname;
    uint8_t mask = 1;
    String text;

    memPtr = 0;
    displayKeyerPreferencesMenu(posSnapStore);
    adjustKeyerPreference(posSnapStore);
    volButton.Update();
        //Serial.println("store memPtr: " + String(memPtr));
    if (memPtr != 8)  {
        p_menuPtr = menu;     // also store last menu selection
        //Serial.println("menu: " + String(p_menuPtr));
        text = "Snap " + String(memPtr+1) + " STORED ";
        snapname = "snap" + String(memPtr);
        //Serial.println("store snapname: " + snapname);
        //Serial.println("store: p_menuPtr = " + String(p_menuPtr));
        writePreferences(snapname);
        /// insert the correct bit into p_snapShots & update memory variables
        mask = mask << memPtr;
        p_snapShots = p_snapShots | mask;
        //Serial.println("store p_snapShots: " + String(p_snapShots));
        printOnScroll(2, BOLD, 0, text);
        updateMemory(p_snapShots);
        delay(1000);
        return true;
      }
      return false;
}


void updateMemory(uint8_t temp) {
  memCounter = 0;                 // create an array that contains the snapshots (memories) that are in use
        for (int i = 0; i<8; ++i) {
          if (temp & 1) {     // mask rightmost bit
            memories[memCounter] = i;
            ++memCounter;
          }
          temp = temp >> 1;   // shift one position to the right
        }
}


void clearMemory(uint8_t ptr) {
  String text = "Snap " + String(memories[ptr]+1) + " CLEARED";

  p_snapShots &= ~(1 << memories[ptr]);     // clear the bit
  printOnScroll(2, BOLD, 0, text);
  updateMemory(p_snapShots);
  delay(1000);
}
